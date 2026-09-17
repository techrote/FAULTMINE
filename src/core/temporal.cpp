#include "faultmine/temporal.hpp"

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"
#include "faultmine/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

constexpr std::uint64_t kMaximumPeriod = 65536U;
constexpr std::uint64_t kMaximumBandHeight = 65536U;
constexpr std::int64_t kMaximumAmplitude = 4096;

[[nodiscard]] const ParameterValue* parameter(const OperatorInstance& instance, const std::string_view name) noexcept {
    const auto found = instance.parameters.find(name);
    return found == instance.parameters.end() ? nullptr : &found->second;
}

[[nodiscard]] std::optional<std::string> read_u64(
    const OperatorInstance& instance,
    const std::string_view name,
    std::uint64_t& value,
    const std::uint64_t minimum,
    const std::uint64_t maximum) {
    const ParameterValue* raw = parameter(instance, name);
    const auto* typed = raw == nullptr ? nullptr : std::get_if<std::uint64_t>(raw);
    if (typed == nullptr || *typed < minimum || *typed > maximum) {
        return std::string{name} + " must be an unsigned integer in [" +
            std::to_string(minimum) + "," + std::to_string(maximum) + "]";
    }
    value = *typed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> read_i64(
    const OperatorInstance& instance,
    const std::string_view name,
    std::int64_t& value,
    const std::int64_t minimum,
    const std::int64_t maximum) {
    const ParameterValue* raw = parameter(instance, name);
    const auto* typed = raw == nullptr ? nullptr : std::get_if<std::int64_t>(raw);
    if (typed == nullptr || *typed < minimum || *typed > maximum) {
        return std::string{name} + " must be a signed integer in [" +
            std::to_string(minimum) + "," + std::to_string(maximum) + "]";
    }
    value = *typed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> read_text(
    const OperatorInstance& instance,
    const std::string_view name,
    std::string_view& value) {
    const ParameterValue* raw = parameter(instance, name);
    const auto* typed = raw == nullptr ? nullptr : std::get_if<std::string>(raw);
    if (typed == nullptr) return std::string{name} + " must be a string";
    value = *typed;
    return std::nullopt;
}

[[nodiscard]] MutationMetadata unsigned_range(
    const std::uint64_t minimum,
    const std::uint64_t maximum,
    const std::uint64_t step = 1U) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::unsigned_range;
    metadata.unsigned_min = minimum;
    metadata.unsigned_max = maximum;
    metadata.unsigned_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata signed_range(
    const std::int64_t minimum,
    const std::int64_t maximum,
    const std::int64_t step = 1) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::signed_range;
    metadata.signed_min = minimum;
    metadata.signed_max = maximum;
    metadata.signed_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata choice(std::vector<std::string> values) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::choice;
    metadata.choices = std::move(values);
    return metadata;
}

[[nodiscard]] ParameterDescriptor u64_parameter(
    std::string name,
    const std::uint64_t minimum,
    const std::uint64_t maximum,
    const std::uint64_t step = 1U) {
    return ParameterDescriptor{std::move(name), ParameterKind::unsigned_integer, true, unsigned_range(minimum, maximum, step)};
}

[[nodiscard]] ParameterDescriptor i64_parameter(
    std::string name,
    const std::int64_t minimum,
    const std::int64_t maximum,
    const std::int64_t step = 1) {
    return ParameterDescriptor{std::move(name), ParameterKind::signed_integer, true, signed_range(minimum, maximum, step)};
}

[[nodiscard]] ParameterDescriptor choice_parameter(std::string name, std::vector<std::string> values) {
    return ParameterDescriptor{std::move(name), ParameterKind::text, true, choice(std::move(values))};
}

[[nodiscard]] std::vector<std::string> boundary_choices() {
    return {"wrap", "clamp", "fill"};
}

[[nodiscard]] std::vector<std::string> modulator_choices() {
    return {"triangle", "saw", "square", "ramp", "sample-hold", "keyed-noise"};
}

[[nodiscard]] bool resolve_coordinate(
    const std::int64_t logical,
    const std::uint32_t extent,
    const std::string_view boundary,
    std::uint32_t& resolved) noexcept {
    const std::int64_t size = static_cast<std::int64_t>(extent);
    if (boundary == "wrap") {
        std::int64_t value = logical % size;
        if (value < 0) value += size;
        resolved = static_cast<std::uint32_t>(value);
        return true;
    }
    if (boundary == "clamp") {
        resolved = logical < 0 ? 0U : logical >= size ? extent - 1U : static_cast<std::uint32_t>(logical);
        return true;
    }
    if (boundary == "fill") {
        if (logical < 0 || logical >= size) return false;
        resolved = static_cast<std::uint32_t>(logical);
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_boundary(const std::string_view boundary) noexcept {
    return boundary == "wrap" || boundary == "clamp" || boundary == "fill";
}

[[nodiscard]] std::size_t byte_index(
    const ImageBuffer& image,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::uint32_t channel = 0U) noexcept {
    return (static_cast<std::size_t>(y) * image.width + x) * 4U + channel;
}

void copy_displaced_pixel(
    const ImageBuffer& input,
    ImageBuffer& output,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::int64_t dx,
    const std::int64_t dy,
    const std::string_view boundary) {
    std::uint32_t source_x{};
    std::uint32_t source_y{};
    const bool present = resolve_coordinate(static_cast<std::int64_t>(x) - dx, input.width, boundary, source_x) &&
        resolve_coordinate(static_cast<std::int64_t>(y) - dy, input.height, boundary, source_y);
    const std::size_t destination = byte_index(output, x, y);
    if (!present) {
        for (std::size_t channel = 0U; channel < 4U; ++channel) output.bytes[destination + channel] = 0U;
        return;
    }
    const std::size_t source = byte_index(input, source_x, source_y);
    std::copy_n(input.bytes.begin() + static_cast<std::ptrdiff_t>(source), 4U,
        output.bytes.begin() + static_cast<std::ptrdiff_t>(destination));
}

[[nodiscard]] std::uint8_t blend_byte(
    const std::uint8_t current,
    const std::uint8_t previous,
    const std::uint64_t previous_weight_256) noexcept {
    const std::uint64_t current_weight = 256U - previous_weight_256;
    const std::uint64_t sum = static_cast<std::uint64_t>(current) * current_weight +
        static_cast<std::uint64_t>(previous) * previous_weight_256 + 128U;
    return static_cast<std::uint8_t>(sum >> 8U);
}

[[nodiscard]] std::optional<std::string> validate_previous(
    const ImageBuffer& input,
    const OperatorTemporalState* state,
    const ImageBuffer*& previous) {
    previous = nullptr;
    if (state == nullptr || !state->previous_output.has_value()) return std::nullopt;
    const ImageBuffer& candidate = *state->previous_output;
    if (candidate.width != input.width || candidate.height != input.height || candidate.format != input.format ||
        candidate.bytes.size() != input.bytes.size()) {
        return std::string{"temporal state image does not match the current canonical input extent"};
    }
    previous = &candidate;
    return std::nullopt;
}

void commit_state(const ImageBuffer& output, OperatorTemporalState& next_state) {
    next_state.previous_output = output;
}

[[nodiscard]] std::optional<std::string> feedback_blend_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed,
    std::uint64_t,
    const OperatorTemporalState* previous_state,
    OperatorTemporalState& next_state) {
    std::uint64_t amount{};
    if (auto error = read_u64(instance, "amount_256", amount, 0U, 256U); error.has_value()) return error;
    const ImageBuffer* previous = nullptr;
    if (auto error = validate_previous(input, previous_state, previous); error.has_value()) return error;
    output = input;
    if (previous != nullptr) {
        for (std::size_t index = 0U; index < output.bytes.size(); ++index) {
            output.bytes[index] = blend_byte(input.bytes[index], previous->bytes[index], amount);
        }
    }
    commit_state(output, next_state);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> feedback_displace_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed,
    std::uint64_t,
    const OperatorTemporalState* previous_state,
    OperatorTemporalState& next_state) {
    std::int64_t dx{};
    std::int64_t dy{};
    std::uint64_t amount{};
    std::string_view boundary;
    if (auto error = read_i64(instance, "dx", dx, -kMaximumAmplitude, kMaximumAmplitude); error.has_value()) return error;
    if (auto error = read_i64(instance, "dy", dy, -kMaximumAmplitude, kMaximumAmplitude); error.has_value()) return error;
    if (auto error = read_u64(instance, "amount_256", amount, 0U, 256U); error.has_value()) return error;
    if (auto error = read_text(instance, "boundary", boundary); error.has_value()) return error;
    if (!valid_boundary(boundary)) return std::string{"boundary must be wrap, clamp, or fill"};
    const ImageBuffer* previous = nullptr;
    if (auto error = validate_previous(input, previous_state, previous); error.has_value()) return error;
    output = input;
    if (previous != nullptr) {
        ImageBuffer displaced = input;
        std::fill(displaced.bytes.begin(), displaced.bytes.end(), std::uint8_t{0});
        for (std::uint32_t y = 0U; y < input.height; ++y) {
            for (std::uint32_t x = 0U; x < input.width; ++x) {
                copy_displaced_pixel(*previous, displaced, x, y, dx, dy, boundary);
            }
        }
        for (std::size_t index = 0U; index < output.bytes.size(); ++index) {
            output.bytes[index] = blend_byte(input.bytes[index], displaced.bytes[index], amount);
        }
    }
    commit_state(output, next_state);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> partial_refresh_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed,
    const std::uint64_t frame_index,
    const OperatorTemporalState* previous_state,
    OperatorTemporalState& next_state) {
    std::uint64_t band_height{};
    std::uint64_t phase_bands{};
    if (auto error = read_u64(instance, "band_height", band_height, 1U, kMaximumBandHeight); error.has_value()) return error;
    if (auto error = read_u64(instance, "phase_bands", phase_bands, 0U, kMaximumPeriod - 1U); error.has_value()) return error;
    const ImageBuffer* previous = nullptr;
    if (auto error = validate_previous(input, previous_state, previous); error.has_value()) return error;
    output = input;
    if (previous != nullptr) {
        output = *previous;
        const std::uint64_t bands = (static_cast<std::uint64_t>(input.height) + band_height - 1U) / band_height;
        const std::uint64_t active = (frame_index % bands + phase_bands % bands) % bands;
        const std::uint64_t first_y = active * band_height;
        const std::uint64_t end_y = std::min<std::uint64_t>(first_y + band_height, input.height);
        for (std::uint64_t y = first_y; y < end_y; ++y) {
            const std::size_t begin = byte_index(input, 0U, static_cast<std::uint32_t>(y));
            const std::size_t count = static_cast<std::size_t>(input.width) * 4U;
            std::copy_n(input.bytes.begin() + static_cast<std::ptrdiff_t>(begin), count,
                output.bytes.begin() + static_cast<std::ptrdiff_t>(begin));
        }
    }
    commit_state(output, next_state);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> trail_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed,
    std::uint64_t,
    const OperatorTemporalState* previous_state,
    OperatorTemporalState& next_state) {
    std::int64_t dx{};
    std::int64_t dy{};
    std::uint64_t decay{};
    std::string_view boundary;
    if (auto error = read_i64(instance, "dx", dx, -kMaximumAmplitude, kMaximumAmplitude); error.has_value()) return error;
    if (auto error = read_i64(instance, "dy", dy, -kMaximumAmplitude, kMaximumAmplitude); error.has_value()) return error;
    if (auto error = read_u64(instance, "decay_256", decay, 0U, 256U); error.has_value()) return error;
    if (auto error = read_text(instance, "boundary", boundary); error.has_value()) return error;
    if (!valid_boundary(boundary)) return std::string{"boundary must be wrap, clamp, or fill"};
    const ImageBuffer* previous = nullptr;
    if (auto error = validate_previous(input, previous_state, previous); error.has_value()) return error;
    output = input;
    if (previous != nullptr) {
        ImageBuffer displaced = input;
        std::fill(displaced.bytes.begin(), displaced.bytes.end(), std::uint8_t{0});
        for (std::uint32_t y = 0U; y < input.height; ++y) {
            for (std::uint32_t x = 0U; x < input.width; ++x) {
                copy_displaced_pixel(*previous, displaced, x, y, dx, dy, boundary);
            }
        }
        for (std::size_t index = 0U; index < output.bytes.size(); ++index) {
            const std::uint64_t faded = (static_cast<std::uint64_t>(displaced.bytes[index]) * decay + 128U) >> 8U;
            output.bytes[index] = std::max(input.bytes[index], static_cast<std::uint8_t>(faded));
        }
    }
    commit_state(output, next_state);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> modulator_parameters(
    const OperatorInstance& instance,
    std::string_view& shape,
    std::uint64_t& period,
    std::uint64_t& phase,
    std::int64_t& amplitude,
    std::string_view& boundary) {
    std::uint64_t amplitude_unsigned{};
    if (auto error = read_text(instance, "shape", shape); error.has_value()) return error;
    if (auto error = read_u64(instance, "period_frames", period, 1U, kMaximumPeriod); error.has_value()) return error;
    if (auto error = read_u64(instance, "phase_frames", phase, 0U, kMaximumPeriod - 1U); error.has_value()) return error;
    if (auto error = read_u64(instance, "amplitude_pixels", amplitude_unsigned, 0U, static_cast<std::uint64_t>(kMaximumAmplitude)); error.has_value()) return error;
    amplitude = static_cast<std::int64_t>(amplitude_unsigned);
    if (auto error = read_text(instance, "boundary", boundary); error.has_value()) return error;
    if (!valid_boundary(boundary)) return std::string{"boundary must be wrap, clamp, or fill"};
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> phase_drift_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed,
    const std::uint64_t frame_index,
    const OperatorTemporalState*,
    OperatorTemporalState& next_state) {
    std::string_view shape;
    std::string_view boundary;
    std::uint64_t period{};
    std::uint64_t phase{};
    std::int64_t amplitude{};
    if (auto error = modulator_parameters(instance, shape, period, phase, amplitude, boundary); error.has_value()) return error;
    std::int64_t displacement{};
    if (auto error = evaluate_temporal_modulator(
            shape, frame_index, period, phase, amplitude, root_seed, instance.instance_id,
            "phase-drift", displacement); error.has_value()) return error;
    output = input;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            copy_displaced_pixel(input, output, x, y, displacement, 0, boundary);
        }
    }
    next_state.previous_output.reset();
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> tearing_phase_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed,
    const std::uint64_t frame_index,
    const OperatorTemporalState*,
    OperatorTemporalState& next_state) {
    std::string_view shape;
    std::string_view boundary;
    std::uint64_t period{};
    std::uint64_t phase{};
    std::int64_t amplitude{};
    if (auto error = modulator_parameters(instance, shape, period, phase, amplitude, boundary); error.has_value()) return error;
    std::uint64_t band_height{};
    if (auto error = read_u64(instance, "band_height", band_height, 1U, kMaximumBandHeight); error.has_value()) return error;
    output = input;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        const std::uint64_t band = static_cast<std::uint64_t>(y) / band_height;
        std::int64_t displacement{};
        if (auto error = evaluate_temporal_modulator(
                shape, frame_index + band, period, phase, amplitude, root_seed, instance.instance_id,
                "tearing-phase", displacement); error.has_value()) return error;
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            copy_displaced_pixel(input, output, x, y, displacement, 0, boundary);
        }
    }
    next_state.previous_output.reset();
    return std::nullopt;
}

[[nodiscard]] bool channel_selected(const std::string_view channels, const std::uint32_t channel) noexcept {
    constexpr std::array<char, 4> names{'r', 'g', 'b', 'a'};
    return channels.find(names[channel]) != std::string_view::npos;
}

[[nodiscard]] std::optional<std::string> channel_phase_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed,
    const std::uint64_t frame_index,
    const OperatorTemporalState*,
    OperatorTemporalState& next_state) {
    std::string_view shape;
    std::string_view boundary;
    std::uint64_t period{};
    std::uint64_t phase{};
    std::int64_t amplitude{};
    if (auto error = modulator_parameters(instance, shape, period, phase, amplitude, boundary); error.has_value()) return error;
    std::string_view channels;
    if (auto error = read_text(instance, "channels", channels); error.has_value()) return error;
    if (channels.empty() || channels.find_first_not_of("rgba") != std::string_view::npos) {
        return std::string{"channels must be a non-empty subset of rgba"};
    }
    for (std::size_t i = 0U; i < channels.size(); ++i) {
        if (channels.find(channels[i]) != i) return std::string{"channels must not contain duplicates"};
    }
    std::int64_t displacement{};
    if (auto error = evaluate_temporal_modulator(
            shape, frame_index, period, phase, amplitude, root_seed, instance.instance_id,
            "channel-phase", displacement); error.has_value()) return error;
    output = input;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            std::uint32_t source_x{};
            const bool present = resolve_coordinate(static_cast<std::int64_t>(x) - displacement, input.width, boundary, source_x);
            for (std::uint32_t channel = 0U; channel < 4U; ++channel) {
                if (!channel_selected(channels, channel)) continue;
                const std::size_t destination = byte_index(output, x, y, channel);
                output.bytes[destination] = present ? input.bytes[byte_index(input, source_x, y, channel)] : 0U;
            }
        }
    }
    next_state.previous_output.reset();
    return std::nullopt;
}

[[nodiscard]] OperatorDescriptor make_descriptor(
    std::string type_id,
    std::vector<ParameterDescriptor> parameters) {
    OperatorDescriptor descriptor;
    descriptor.type_id = std::move(type_id);
    descriptor.minimum_supported_version = 1U;
    descriptor.current_version = 1U;
    descriptor.parameters = std::move(parameters);
    return descriptor;
}

void register_required(FaultRegistry& registry, OperatorDescriptor descriptor, const TemporalOperatorExecutor executor) {
    std::string error;
    if (!registry.register_temporal_operator(std::move(descriptor), executor, &error)) {
        std::abort();
    }
}

[[nodiscard]] std::vector<ParameterDescriptor> phase_parameters() {
    return {
        choice_parameter("shape", modulator_choices()),
        u64_parameter("period_frames", 1U, kMaximumPeriod),
        u64_parameter("phase_frames", 0U, kMaximumPeriod - 1U),
        u64_parameter("amplitude_pixels", 0U, static_cast<std::uint64_t>(kMaximumAmplitude)),
        choice_parameter("boundary", boundary_choices())};
}

}  // namespace

std::optional<std::string> evaluate_temporal_modulator(
    const std::string_view shape,
    const std::uint64_t frame_index,
    const std::uint64_t period_frames,
    const std::uint64_t phase_frames,
    const std::int64_t amplitude,
    const RootSeed root_seed,
    const InstanceId instance_id,
    const std::string_view purpose_tag,
    std::int64_t& output) {
    if (period_frames == 0U || period_frames > kMaximumPeriod) return std::string{"period_frames is outside the temporal v1 contract"};
    if (amplitude < 0 || amplitude > kMaximumAmplitude) return std::string{"amplitude is outside the temporal v1 contract"};
    const std::uint64_t shifted = frame_index + phase_frames;
    if (amplitude == 0) {
        output = 0;
        return std::nullopt;
    }
    const std::int64_t span = amplitude * 2;
    if (shape == "ramp") {
        if (period_frames == 1U || shifted >= period_frames - 1U) output = amplitude;
        else output = -amplitude + (span * static_cast<std::int64_t>(shifted)) / static_cast<std::int64_t>(period_frames - 1U);
        return std::nullopt;
    }
    if (shape == "sample-hold" || shape == "keyed-noise") {
        const std::uint64_t identity = shape == "sample-hold" ? shifted / period_frames : shifted;
        const std::array<std::uint64_t, 2> words{identity, period_frames};
        DeterministicStream stream = make_named_stream(root_seed, instance_id, purpose_tag, words);
        const std::uint64_t range = static_cast<std::uint64_t>(span) + 1U;
        output = -amplitude + static_cast<std::int64_t>(stream.uniform_below(range));
        return std::nullopt;
    }
    const std::uint64_t position = shifted % period_frames;
    if (shape == "square") {
        output = position * 2U < period_frames ? -amplitude : amplitude;
        return std::nullopt;
    }
    if (shape != "saw" && shape != "triangle") return std::string{"unsupported temporal modulator shape"};
    std::int64_t saw = 0;
    if (period_frames > 1U) {
        saw = -amplitude + (span * static_cast<std::int64_t>(position)) /
            static_cast<std::int64_t>(period_frames - 1U);
    }
    if (shape == "saw") {
        output = saw;
    } else {
        output = amplitude - 2 * std::llabs(saw);
    }
    return std::nullopt;
}

void register_temporal_faults(FaultRegistry& registry) {
    register_required(registry, make_descriptor(kFaultFeedbackBlend, {
        u64_parameter("amount_256", 0U, 256U)}), &feedback_blend_executor);
    register_required(registry, make_descriptor(kFaultFeedbackDisplace, {
        i64_parameter("dx", -kMaximumAmplitude, kMaximumAmplitude),
        i64_parameter("dy", -kMaximumAmplitude, kMaximumAmplitude),
        u64_parameter("amount_256", 0U, 256U),
        choice_parameter("boundary", boundary_choices())}), &feedback_displace_executor);
    register_required(registry, make_descriptor(kFaultPartialRefresh, {
        u64_parameter("band_height", 1U, kMaximumBandHeight),
        u64_parameter("phase_bands", 0U, kMaximumPeriod - 1U)}), &partial_refresh_executor);
    register_required(registry, make_descriptor(kFaultTrailAccumulation, {
        i64_parameter("dx", -kMaximumAmplitude, kMaximumAmplitude),
        i64_parameter("dy", -kMaximumAmplitude, kMaximumAmplitude),
        u64_parameter("decay_256", 0U, 256U),
        choice_parameter("boundary", boundary_choices())}), &trail_executor);
    register_required(registry, make_descriptor(kFaultPhaseDrift, phase_parameters()), &phase_drift_executor);
    auto tearing = phase_parameters();
    tearing.push_back(u64_parameter("band_height", 1U, kMaximumBandHeight));
    register_required(registry, make_descriptor(kFaultTearingPhase, std::move(tearing)), &tearing_phase_executor);
    auto channels = phase_parameters();
    channels.push_back(choice_parameter("channels", {"r", "g", "b", "a", "rg", "rb", "gb", "rgb", "rgba"}));
    register_required(registry, make_descriptor(kFaultChannelPhase, std::move(channels)), &channel_phase_executor);
}

std::string temporal_frame_identity_hex(
    const std::string_view source_identity,
    const Genome& genome,
    const std::uint64_t frame_index,
    const ImageBuffer& image) {
    const std::string material = "FAULTMINE-TEMPORAL-FRAME-v1\nsource=" + std::string{source_identity} +
        "\ngenome=" + genome_identity_hex(genome) +
        "\nframe=" + std::to_string(frame_index) +
        "\nimage=" + source_identity_hex(image) + "\n";
    return sha256_hex(material);
}

}  // namespace faultmine::core
