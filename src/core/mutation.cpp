#include "faultmine/mutation.hpp"

#include "faultmine/colour.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

struct GeneTarget {
    std::size_t operator_index{};
    const ParameterDescriptor* descriptor{};
};

[[nodiscard]] MutationResult make_error(
    const MutationErrorCode code,
    std::string message,
    DescendantProvenance provenance) {
    MutationResult result;
    result.error = MutationError{code, std::move(message)};
    result.provenance = std::move(provenance);
    return result;
}

[[nodiscard]] bool instance_exists(const Genome& genome, const InstanceId id) noexcept {
    return std::any_of(
        genome.operators.begin(), genome.operators.end(),
        [id](const OperatorInstance& instance) { return instance.instance_id == id; });
}

[[nodiscard]] const OperatorInstance* find_instance(const Genome& genome, const InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

[[nodiscard]] bool operator_locked(const MutationLocks& locks, const InstanceId id) noexcept {
    return std::find(locks.operators.begin(), locks.operators.end(), id) != locks.operators.end();
}

[[nodiscard]] bool parameter_locked(
    const MutationLocks& locks,
    const InstanceId id,
    const std::string_view parameter) noexcept {
    return std::any_of(
        locks.parameters.begin(), locks.parameters.end(),
        [id, parameter](const MutationParameterLock& lock) {
            return lock.instance_id == id && lock.parameter == parameter;
        });
}

[[nodiscard]] bool any_parameter_locked(const MutationLocks& locks, const InstanceId id) noexcept {
    return std::any_of(
        locks.parameters.begin(), locks.parameters.end(),
        [id](const MutationParameterLock& lock) { return lock.instance_id == id; });
}

[[nodiscard]] std::optional<std::string> validate_locks(
    const Genome& parent,
    const OperatorRegistry& registry,
    const MutationLocks& locks) {
    for (const InstanceId id : locks.operators) {
        if (find_instance(parent, id) == nullptr) {
            return "operator mutation lock targets an instance absent from the parent genome";
        }
    }
    for (const MutationParameterLock& lock : locks.parameters) {
        const OperatorInstance* instance = find_instance(parent, lock.instance_id);
        if (instance == nullptr) {
            return "parameter mutation lock targets an instance absent from the parent genome";
        }
        const OperatorDescriptor* descriptor = registry.find(instance->type_id);
        if (descriptor == nullptr) {
            return "parameter mutation lock targets an operator absent from the registry";
        }
        const auto found = std::find_if(
            descriptor->parameters.begin(), descriptor->parameters.end(),
            [&lock](const ParameterDescriptor& parameter) { return parameter.name == lock.parameter; });
        if (found == descriptor->parameters.end()) {
            return "parameter mutation lock targets an undeclared parameter";
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> validate_mutation_descriptor(
    const ParameterDescriptor& parameter) {
    const MutationMetadata& metadata = parameter.mutation;
    if (metadata.policy_version == 0U) {
        return "parameter '" + parameter.name + "' has mutation policy version zero";
    }
    if (!metadata.mutable_gene) {
        return std::nullopt;
    }

    switch (metadata.domain) {
        case MutationDomain::opaque:
            return "mutable parameter '" + parameter.name + "' has opaque mutation metadata";
        case MutationDomain::toggle:
            if (parameter.kind != ParameterKind::boolean) {
                return "toggle mutation parameter '" + parameter.name + "' is not boolean";
            }
            break;
        case MutationDomain::signed_range:
            if (parameter.kind != ParameterKind::signed_integer ||
                metadata.signed_step <= 0 ||
                metadata.signed_min >= metadata.signed_max) {
                return "signed-range mutation metadata for '" + parameter.name + "' is incomplete or invalid";
            }
            break;
        case MutationDomain::unsigned_range:
            if (parameter.kind != ParameterKind::unsigned_integer ||
                metadata.unsigned_step == 0U ||
                metadata.unsigned_min >= metadata.unsigned_max) {
                return "unsigned-range mutation metadata for '" + parameter.name + "' is incomplete or invalid";
            }
            break;
        case MutationDomain::choice:
            if (parameter.kind != ParameterKind::text || metadata.choices.size() < 2U) {
                return "choice mutation metadata for '" + parameter.name + "' needs at least two text choices";
            }
            for (std::size_t index = 0U; index < metadata.choices.size(); ++index) {
                if (metadata.choices[index].empty()) {
                    return "choice mutation metadata for '" + parameter.name + "' contains an empty choice";
                }
                if (std::find(
                        metadata.choices.begin(),
                        metadata.choices.begin() + static_cast<std::ptrdiff_t>(index),
                        metadata.choices[index]) !=
                    metadata.choices.begin() + static_cast<std::ptrdiff_t>(index)) {
                    return "choice mutation metadata for '" + parameter.name + "' contains duplicate choices";
                }
            }
            break;
        case MutationDomain::bitmask:
            if (parameter.kind != ParameterKind::unsigned_integer) {
                return "bitmask mutation parameter '" + parameter.name + "' is not unsigned";
            }
            break;
        case MutationDomain::colour_rgba:
        case MutationDomain::palette:
        case MutationDomain::lut:
            if (parameter.kind != ParameterKind::text) {
                return "structured colour mutation parameter '" + parameter.name + "' is not text-backed";
            }
            break;
    }
    return std::nullopt;
}

[[nodiscard]] std::array<std::uint64_t, 4U> identity_words(const std::string_view identity) noexcept {
    std::array<std::uint64_t, 4U> words{};
    if (identity.size() != 64U) {
        return words;
    }
    for (std::size_t index = 0U; index < words.size(); ++index) {
        const std::string_view part = identity.substr(index * 16U, 16U);
        const auto converted = std::from_chars(part.data(), part.data() + part.size(), words[index], 16);
        if (converted.ec != std::errc{} || converted.ptr != part.data() + part.size()) {
            return {};
        }
    }
    return words;
}

[[nodiscard]] DeterministicStream mutation_stream(
    const MutationRequest& request,
    const InstanceId owner,
    const std::string_view purpose,
    const std::array<std::uint64_t, 4U>& parent_words,
    const std::uint64_t extra_a = 0U,
    const std::uint64_t extra_b = 0U) noexcept {
    const std::array<std::uint64_t, 9U> identity{
        parent_words[0], parent_words[1], parent_words[2], parent_words[3],
        request.descendant_index,
        static_cast<std::uint64_t>(request.radius),
        static_cast<std::uint64_t>(kMutationPolicyVersion),
        extra_a,
        extra_b};
    return make_named_stream(request.mutation_seed, owner, purpose, identity);
}

[[nodiscard]] std::optional<ParameterValue> default_parameter_value(
    const ParameterDescriptor& descriptor) {
    const MutationMetadata& metadata = descriptor.mutation;
    switch (descriptor.kind) {
        case ParameterKind::boolean:
            return ParameterValue{false};
        case ParameterKind::signed_integer:
            if (metadata.domain == MutationDomain::signed_range) {
                if (metadata.signed_min <= 0 && metadata.signed_max >= 0 &&
                    metadata.signed_min % metadata.signed_step == 0) {
                    return ParameterValue{std::int64_t{0}};
                }
                return ParameterValue{metadata.signed_min};
            }
            return ParameterValue{std::int64_t{0}};
        case ParameterKind::unsigned_integer:
            if (metadata.domain == MutationDomain::unsigned_range) {
                return ParameterValue{metadata.unsigned_min};
            }
            return ParameterValue{std::uint64_t{0U}};
        case ParameterKind::text:
            if (metadata.domain == MutationDomain::choice && !metadata.choices.empty()) {
                return ParameterValue{metadata.choices.front()};
            }
            if (metadata.domain == MutationDomain::colour_rgba) {
                return ParameterValue{rgba8_to_hex(Rgba8{0U, 0U, 0U, 255U})};
            }
            if (metadata.domain == MutationDomain::palette) {
                Palette palette;
                palette.entries = {Rgba8{0U, 0U, 0U, 255U}, Rgba8{255U, 255U, 255U, 255U}};
                return ParameterValue{serialize_palette_canonical(palette)};
            }
            if (metadata.domain == MutationDomain::lut) {
                return ParameterValue{serialize_lut_canonical(make_identity_lut())};
            }
            return ParameterValue{std::string{}};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<OperatorInstance> make_default_instance(
    const OperatorDescriptor& descriptor,
    const InstanceId id) {
    OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = descriptor.type_id;
    instance.type_version = descriptor.current_version;
    instance.enabled = true;
    for (const ParameterDescriptor& parameter : descriptor.parameters) {
        if (!parameter.required) {
            continue;
        }
        const auto value = default_parameter_value(parameter);
        if (!value.has_value()) {
            return std::nullopt;
        }
        instance.parameters.emplace(parameter.name, *value);
    }
    return instance;
}

[[nodiscard]] std::uint64_t radius_steps(
    const MutationRadius radius,
    DeterministicStream& stream) {
    switch (radius) {
        case MutationRadius::low: return 1U;
        case MutationRadius::medium: return 2U + stream.uniform_below(7U);
        case MutationRadius::high: return 9U + stream.uniform_below(56U);
    }
    return 1U;
}

[[nodiscard]] bool move_signed(
    std::int64_t& value,
    const MutationMetadata& metadata,
    DeterministicStream& stream,
    const MutationRadius radius) {
    if (value < metadata.signed_min || value > metadata.signed_max ||
        metadata.signed_step <= 0 ||
        (value - metadata.signed_min) % metadata.signed_step != 0) {
        return false;
    }
    int direction = stream.uniform_below(2U) == 0U ? -1 : 1;
    if (value == metadata.signed_min) direction = 1;
    if (value == metadata.signed_max) direction = -1;
    const std::uint64_t steps = radius_steps(radius, stream);
    const std::int64_t step = metadata.signed_step;
    const std::int64_t original = value;
    for (std::uint64_t count = 0U; count < steps; ++count) {
        if (direction > 0) {
            if (value == metadata.signed_max) break;
            if (value > std::numeric_limits<std::int64_t>::max() - step) {
                value = metadata.signed_max;
                break;
            }
            const std::int64_t next = static_cast<std::int64_t>(value + step);
            value = std::min(next, metadata.signed_max);
        } else {
            if (value == metadata.signed_min) break;
            if (value < std::numeric_limits<std::int64_t>::min() + step) {
                value = metadata.signed_min;
                break;
            }
            const std::int64_t next = static_cast<std::int64_t>(value - step);
            value = std::max(next, metadata.signed_min);
        }
    }
    return value != original;
}

[[nodiscard]] bool move_unsigned(
    std::uint64_t& value,
    const MutationMetadata& metadata,
    DeterministicStream& stream,
    const MutationRadius radius) {
    if (value < metadata.unsigned_min || value > metadata.unsigned_max ||
        metadata.unsigned_step == 0U ||
        (value - metadata.unsigned_min) % metadata.unsigned_step != 0U) {
        return false;
    }
    int direction = stream.uniform_below(2U) == 0U ? -1 : 1;
    if (value == metadata.unsigned_min) direction = 1;
    if (value == metadata.unsigned_max) direction = -1;
    const std::uint64_t steps = radius_steps(radius, stream);
    const std::uint64_t step = metadata.unsigned_step;
    const std::uint64_t original = value;
    for (std::uint64_t count = 0U; count < steps; ++count) {
        if (direction > 0) {
            const std::uint64_t distance = metadata.unsigned_max - value;
            value = step >= distance ? metadata.unsigned_max : value + step;
        } else {
            const std::uint64_t distance = value - metadata.unsigned_min;
            value = step >= distance ? metadata.unsigned_min : value - step;
        }
    }
    return value != original;
}

[[nodiscard]] bool mutate_choice(
    std::string& value,
    const MutationMetadata& metadata,
    DeterministicStream& stream,
    const MutationRadius radius) {
    const auto found = std::find(metadata.choices.begin(), metadata.choices.end(), value);
    if (found == metadata.choices.end() || metadata.choices.size() < 2U) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(std::distance(metadata.choices.begin(), found));
    std::size_t next = index;
    if (radius == MutationRadius::low) {
        if (index == 0U) {
            next = 1U;
        } else if (index + 1U == metadata.choices.size()) {
            next = index - 1U;
        } else {
            next = stream.uniform_below(2U) == 0U ? index - 1U : index + 1U;
        }
    } else {
        const std::size_t offset = static_cast<std::size_t>(1U + stream.uniform_below(metadata.choices.size() - 1U));
        next = (index + offset) % metadata.choices.size();
    }
    value = metadata.choices[next];
    return next != index;
}

[[nodiscard]] std::uint8_t mutate_byte(
    const std::uint8_t value,
    DeterministicStream& stream,
    const MutationRadius radius) {
    if (radius == MutationRadius::high) {
        const std::uint8_t offset = static_cast<std::uint8_t>(1U + stream.uniform_below(255U));
        return static_cast<std::uint8_t>(static_cast<unsigned int>(value) + offset);
    }
    const unsigned int maximum_delta = radius == MutationRadius::low ? 8U : 64U;
    const unsigned int delta = 1U + static_cast<unsigned int>(stream.uniform_below(maximum_delta));
    const bool increase = stream.uniform_below(2U) != 0U;
    if (increase) {
        return static_cast<std::uint8_t>(std::min<unsigned int>(255U, static_cast<unsigned int>(value) + delta));
    }
    return static_cast<std::uint8_t>(delta >= value ? 0U : static_cast<unsigned int>(value) - delta);
}

[[nodiscard]] bool mutate_colour(
    std::string& value,
    DeterministicStream& stream,
    const MutationRadius radius) {
    const auto parsed = parse_rgba8_hex(value);
    if (!parsed.has_value()) {
        return false;
    }
    Rgba8 colour = *parsed;
    std::array<std::uint8_t*, 4U> channels{&colour.r, &colour.g, &colour.b, &colour.a};
    const std::size_t channel = static_cast<std::size_t>(stream.uniform_below(channels.size()));
    const std::uint8_t before = *channels[channel];
    std::uint8_t after = mutate_byte(before, stream, radius);
    if (after == before) {
        after = before == 255U ? 254U : static_cast<std::uint8_t>(before + 1U);
    }
    *channels[channel] = after;
    value = rgba8_to_hex(colour);
    return true;
}

[[nodiscard]] bool mutate_palette_value(
    std::string& value,
    DeterministicStream& stream,
    const MutationRadius radius) {
    auto parsed = parse_palette(value);
    if (!parsed.ok() || parsed.palette->entries.empty()) {
        return false;
    }
    Palette palette = std::move(*parsed.palette);
    const std::size_t entry = static_cast<std::size_t>(stream.uniform_below(palette.entries.size()));
    Rgba8& colour = palette.entries[entry];
    std::array<std::uint8_t*, 4U> channels{&colour.r, &colour.g, &colour.b, &colour.a};
    const std::size_t channel = static_cast<std::size_t>(stream.uniform_below(channels.size()));
    const std::uint8_t before = *channels[channel];
    std::uint8_t after = mutate_byte(before, stream, radius);
    if (after == before) after = before == 255U ? 254U : static_cast<std::uint8_t>(before + 1U);
    *channels[channel] = after;
    value = serialize_palette_canonical(palette);
    return true;
}

[[nodiscard]] bool mutate_lut_value(
    std::string& value,
    DeterministicStream& stream,
    const MutationRadius radius) {
    auto parsed = parse_lut(value);
    if (!parsed.ok()) {
        return false;
    }
    Lut256 lut = std::move(*parsed.lut);
    const std::size_t channel = static_cast<std::size_t>(stream.uniform_below(4U));
    const std::size_t entry = static_cast<std::size_t>(stream.uniform_below(256U));
    const std::uint8_t before = lut.channels[channel][entry];
    std::uint8_t after = mutate_byte(before, stream, radius);
    if (after == before) after = before == 255U ? 254U : static_cast<std::uint8_t>(before + 1U);
    lut.channels[channel][entry] = after;
    value = serialize_lut_canonical(lut);
    return true;
}

[[nodiscard]] bool mutate_parameter(
    OperatorInstance& instance,
    const ParameterDescriptor& descriptor,
    const MutationRequest& request,
    const std::array<std::uint64_t, 4U>& parent_words,
    std::string& error) {
    const auto found = instance.parameters.find(descriptor.name);
    if (found == instance.parameters.end()) {
        error = "required mutable parameter '" + descriptor.name + "' is absent";
        return false;
    }
    ParameterValue& value = found->second;
    const MutationMetadata& metadata = descriptor.mutation;
    auto stream = mutation_stream(
        request,
        instance.instance_id,
        "mutation-gene-v1",
        parent_words,
        stable_tag_hash(descriptor.name),
        metadata.policy_version);

    switch (metadata.domain) {
        case MutationDomain::toggle: {
            auto* typed = std::get_if<bool>(&value);
            if (typed == nullptr) break;
            *typed = !*typed;
            return true;
        }
        case MutationDomain::signed_range: {
            auto* typed = std::get_if<std::int64_t>(&value);
            if (typed != nullptr && move_signed(*typed, metadata, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::unsigned_range: {
            auto* typed = std::get_if<std::uint64_t>(&value);
            if (typed != nullptr && move_unsigned(*typed, metadata, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::choice: {
            auto* typed = std::get_if<std::string>(&value);
            if (typed != nullptr && mutate_choice(*typed, metadata, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::bitmask: {
            auto* typed = std::get_if<std::uint64_t>(&value);
            if (typed == nullptr) break;
            const unsigned int flips = request.radius == MutationRadius::low ? 1U :
                (request.radius == MutationRadius::medium ? 2U : 4U);
            const unsigned int start = static_cast<unsigned int>(stream.uniform_below(8U));
            unsigned int stride = static_cast<unsigned int>(1U + 2U * stream.uniform_below(4U));
            while (std::gcd(stride, 8U) != 1U) stride = (stride + 2U) % 8U;
            for (unsigned int index = 0U; index < flips; ++index) {
                const unsigned int bit = (start + index * stride) % 8U;
                *typed ^= std::uint64_t{1U} << bit;
            }
            return true;
        }
        case MutationDomain::colour_rgba: {
            auto* typed = std::get_if<std::string>(&value);
            if (typed != nullptr && mutate_colour(*typed, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::palette: {
            auto* typed = std::get_if<std::string>(&value);
            if (typed != nullptr && mutate_palette_value(*typed, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::lut: {
            auto* typed = std::get_if<std::string>(&value);
            if (typed != nullptr && mutate_lut_value(*typed, stream, request.radius)) return true;
            break;
        }
        case MutationDomain::opaque:
            break;
    }

    error = "parameter '" + descriptor.name + "' does not satisfy its mutation descriptor/current value contract";
    return false;
}

[[nodiscard]] InstanceId unique_child_id(
    const Genome& genome,
    const MutationRequest& request,
    const InstanceId parent_identity,
    const std::string_view purpose,
    std::uint64_t ordinal) noexcept {
    for (std::uint64_t attempt = 0U; attempt < 1024U; ++attempt) {
        const InstanceId candidate = derive_instance_id(
            request.mutation_seed,
            parent_identity,
            purpose,
            ordinal + attempt);
        if (!instance_exists(genome, candidate)) {
            return candidate;
        }
    }
    return derive_instance_id(request.mutation_seed, parent_identity, purpose, ordinal ^ 0xffffffffffffffffULL);
}

[[nodiscard]] bool apply_insert(
    Genome& genome,
    const OperatorRegistry& registry,
    const MutationRequest& request,
    const std::array<std::uint64_t, 4U>& parent_words,
    DeterministicStream& stream) {
    if (genome.operators.size() >= kMaximumMutationOperators || registry.descriptors().empty()) {
        return false;
    }
    const std::size_t descriptor_index = static_cast<std::size_t>(stream.uniform_below(registry.descriptors().size()));
    const OperatorDescriptor& descriptor = registry.descriptors()[descriptor_index];
    const std::size_t position = static_cast<std::size_t>(stream.uniform_below(genome.operators.size() + 1U));
    const InstanceId parent_identity{parent_words[0], parent_words[1]};
    const std::uint64_t ordinal = mix64(
        request.descendant_index ^ stable_tag_hash(descriptor.type_id) ^
        (static_cast<std::uint64_t>(position) << 32U) ^ parent_words[2]);
    const InstanceId id = unique_child_id(genome, request, parent_identity, "mutation-insert-v1", ordinal);
    auto instance = make_default_instance(descriptor, id);
    if (!instance.has_value()) {
        return false;
    }
    genome.operators.insert(genome.operators.begin() + static_cast<std::ptrdiff_t>(position), std::move(*instance));
    return true;
}

[[nodiscard]] bool apply_remove(
    Genome& genome,
    const MutationLocks& locks,
    DeterministicStream& stream) {
    std::vector<std::size_t> candidates;
    for (std::size_t index = 0U; index < genome.operators.size(); ++index) {
        const InstanceId id = genome.operators[index].instance_id;
        if (!operator_locked(locks, id) && !any_parameter_locked(locks, id)) {
            candidates.push_back(index);
        }
    }
    if (candidates.empty()) return false;
    const std::size_t index = candidates[static_cast<std::size_t>(stream.uniform_below(candidates.size()))];
    genome.operators.erase(genome.operators.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

[[nodiscard]] bool apply_reorder(
    Genome& genome,
    const MutationLocks& locks,
    DeterministicStream& stream) {
    struct Move { std::size_t from; std::size_t to; };
    std::vector<Move> candidates;
    for (std::size_t from = 0U; from < genome.operators.size(); ++from) {
        if (operator_locked(locks, genome.operators[from].instance_id)) continue;
        for (std::size_t to = 0U; to < genome.operators.size(); ++to) {
            if (from == to) continue;
            const std::size_t first = std::min(from, to);
            const std::size_t last = std::max(from, to);
            bool crosses_locked = false;
            for (std::size_t index = first; index <= last; ++index) {
                if (index != from && operator_locked(locks, genome.operators[index].instance_id)) {
                    crosses_locked = true;
                    break;
                }
            }
            if (!crosses_locked) candidates.push_back(Move{from, to});
        }
    }
    if (candidates.empty()) return false;
    const Move move = candidates[static_cast<std::size_t>(stream.uniform_below(candidates.size()))];
    OperatorInstance instance = std::move(genome.operators[move.from]);
    genome.operators.erase(genome.operators.begin() + static_cast<std::ptrdiff_t>(move.from));
    genome.operators.insert(genome.operators.begin() + static_cast<std::ptrdiff_t>(move.to), std::move(instance));
    return true;
}

[[nodiscard]] bool apply_substitute(
    Genome& genome,
    const OperatorRegistry& registry,
    const MutationLocks& locks,
    const MutationRequest& request,
    const std::array<std::uint64_t, 4U>& parent_words,
    DeterministicStream& stream) {
    if (registry.descriptors().size() < 2U || genome.operators.empty()) return false;
    std::vector<std::size_t> candidates;
    for (std::size_t index = 0U; index < genome.operators.size(); ++index) {
        const InstanceId id = genome.operators[index].instance_id;
        if (!operator_locked(locks, id) && !any_parameter_locked(locks, id)) candidates.push_back(index);
    }
    if (candidates.empty()) return false;
    const std::size_t index = candidates[static_cast<std::size_t>(stream.uniform_below(candidates.size()))];
    const std::string& old_type = genome.operators[index].type_id;

    std::size_t descriptor_index = static_cast<std::size_t>(stream.uniform_below(registry.descriptors().size()));
    for (std::size_t attempt = 0U; attempt < registry.descriptors().size(); ++attempt) {
        const std::size_t candidate = (descriptor_index + attempt) % registry.descriptors().size();
        if (registry.descriptors()[candidate].type_id != old_type) {
            descriptor_index = candidate;
            break;
        }
    }
    const OperatorDescriptor& descriptor = registry.descriptors()[descriptor_index];
    if (descriptor.type_id == old_type) return false;

    const InstanceId parent_identity{parent_words[0], parent_words[1]};
    const std::uint64_t ordinal = mix64(
        request.descendant_index ^ stable_tag_hash(descriptor.type_id) ^
        stable_tag_hash(old_type) ^ static_cast<std::uint64_t>(index) ^ parent_words[3]);
    const InstanceId id = unique_child_id(genome, request, parent_identity, "mutation-substitute-v1", ordinal);
    auto replacement = make_default_instance(descriptor, id);
    if (!replacement.has_value()) return false;
    genome.operators[index] = std::move(*replacement);
    return true;
}

[[nodiscard]] bool apply_high_topology(
    Genome& genome,
    const OperatorRegistry& registry,
    const MutationRequest& request,
    const std::array<std::uint64_t, 4U>& parent_words) {
    const InstanceId owner{parent_words[0], parent_words[1]};
    auto stream = mutation_stream(request, owner, "mutation-topology-v1", parent_words);
    const std::uint64_t initial = stream.uniform_below(5U);
    if (initial == 0U) return false;

    for (std::uint64_t attempt = 0U; attempt < 4U; ++attempt) {
        const std::uint64_t kind = 1U + ((initial - 1U + attempt) % 4U);
        bool changed = false;
        switch (kind) {
            case 1U: changed = apply_insert(genome, registry, request, parent_words, stream); break;
            case 2U: changed = apply_remove(genome, request.locks, stream); break;
            case 3U: changed = apply_reorder(genome, request.locks, stream); break;
            case 4U: changed = apply_substitute(genome, registry, request.locks, request, parent_words, stream); break;
            default: break;
        }
        if (changed) return true;
    }
    return false;
}

}  // namespace

std::string_view mutation_radius_name(const MutationRadius radius) noexcept {
    switch (radius) {
        case MutationRadius::low: return "low";
        case MutationRadius::medium: return "medium";
        case MutationRadius::high: return "high";
    }
    return "medium";
}

std::optional<std::string> validate_mutation_descriptors(const OperatorRegistry& registry) {
    for (const OperatorDescriptor& descriptor : registry.descriptors()) {
        for (const ParameterDescriptor& parameter : descriptor.parameters) {
            if (const auto error = validate_mutation_descriptor(parameter); error.has_value()) {
                return descriptor.type_id + "." + parameter.name + ": " + *error;
            }
        }
    }
    return std::nullopt;
}

MutationResult generate_descendant(
    const Genome& parent,
    const OperatorRegistry& registry,
    const MutationRequest& request) {
    DescendantProvenance provenance;
    provenance.parent_genome_identity = genome_identity_hex(parent);
    provenance.mutation_seed = request.mutation_seed;
    provenance.descendant_index = request.descendant_index;
    provenance.radius = request.radius;

    if (const auto parent_error = validate_genome(parent, registry); parent_error.has_value()) {
        return make_error(
            MutationErrorCode::invalid_parent,
            parent_error->path + ": " + parent_error->message,
            std::move(provenance));
    }
    if (const auto descriptor_error = validate_mutation_descriptors(registry); descriptor_error.has_value()) {
        return make_error(MutationErrorCode::invalid_descriptor, *descriptor_error, std::move(provenance));
    }
    if (const auto lock_error = validate_locks(parent, registry, request.locks); lock_error.has_value()) {
        return make_error(MutationErrorCode::invalid_lock, *lock_error, std::move(provenance));
    }

    const auto parent_words = identity_words(provenance.parent_genome_identity);
    Genome child = parent;
    std::vector<GeneTarget> targets;
    for (std::size_t operator_index = 0U; operator_index < parent.operators.size(); ++operator_index) {
        const OperatorInstance& instance = parent.operators[operator_index];
        if (operator_locked(request.locks, instance.instance_id)) continue;
        const OperatorDescriptor* descriptor = registry.find(instance.type_id);
        if (descriptor == nullptr) continue;
        for (const ParameterDescriptor& parameter : descriptor->parameters) {
            if (!parameter.mutation.mutable_gene ||
                parameter_locked(request.locks, instance.instance_id, parameter.name)) {
                continue;
            }
            targets.push_back(GeneTarget{operator_index, &parameter});
        }
    }

    bool changed = false;
    if (!targets.empty()) {
        const InstanceId selection_owner{parent_words[0], parent_words[1]};
        auto selection = mutation_stream(request, selection_owner, "mutation-target-selection-v1", parent_words);
        const std::size_t wanted = request.radius == MutationRadius::low ? 1U :
            (request.radius == MutationRadius::medium ? 2U : 3U);
        const std::size_t count = std::min(wanted, targets.size());
        const std::size_t start = static_cast<std::size_t>(selection.uniform_below(targets.size()));
        std::size_t stride = static_cast<std::size_t>(1U + selection.uniform_below(targets.size()));
        while (std::gcd(stride, targets.size()) != 1U) {
            stride = stride == targets.size() ? 1U : stride + 1U;
        }
        for (std::size_t ordinal = 0U; ordinal < count; ++ordinal) {
            const GeneTarget target = targets[(start + ordinal * stride) % targets.size()];
            std::string error;
            if (!mutate_parameter(
                    child.operators[target.operator_index],
                    *target.descriptor,
                    request,
                    parent_words,
                    error)) {
                return make_error(MutationErrorCode::invalid_parameter, std::move(error), std::move(provenance));
            }
            changed = true;
        }
    }

    bool topology_changed = false;
    if (request.radius == MutationRadius::high) {
        topology_changed = apply_high_topology(child, registry, request, parent_words);
        changed = changed || topology_changed;
    }

    if (const auto child_error = validate_genome(child, registry); child_error.has_value()) {
        return make_error(
            MutationErrorCode::invalid_descendant,
            child_error->path + ": " + child_error->message,
            std::move(provenance));
    }

    MutationResult result;
    result.genome = std::move(child);
    result.provenance = std::move(provenance);
    result.changed = changed;
    result.topology_changed = topology_changed;
    if (!changed) {
        result.no_change_reason = "all mutable genes/topology candidates were locked or unavailable under the selected radius";
    }
    return result;
}

}  // namespace faultmine::core
