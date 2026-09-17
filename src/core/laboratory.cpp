#include "faultmine/laboratory.hpp"

#include "faultmine/sha256.hpp"
#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::core {
namespace {

constexpr InstanceId kLaboratoryMutationInstance{0x464d4c41424d5554ULL, 0x4154453030303031ULL};
constexpr std::uint64_t kMaximumMutationFlips = 1ULL << 20U;
constexpr std::uint64_t kMaximumFrozenPixels = 256ULL * 1024ULL * 1024ULL;

[[nodiscard]] bool checked_add(const std::uint64_t left, const std::uint64_t right, std::uint64_t& result) noexcept {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) return false;
    result = left + right;
    return true;
}

[[nodiscard]] bool checked_mul(const std::uint64_t left, const std::uint64_t right, std::uint64_t& result) noexcept {
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) return false;
    result = left * right;
    return true;
}

[[nodiscard]] bool valid_hex64(const std::string_view text) noexcept {
    return text.size() == 64U && std::all_of(text.begin(), text.end(), [](const unsigned char value) {
        return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
    });
}

[[nodiscard]] std::string json_string(const std::string_view text) {
    return "\"" + json::escape_string(text) + "\"";
}

[[nodiscard]] const json::Value* field(const json::Value& object, const std::string_view name) noexcept {
    if (object.type != json::ValueType::object) return nullptr;
    for (const auto& item : object.object) {
        if (item.first == name) return &item.second;
    }
    return nullptr;
}

[[nodiscard]] bool only_fields(
    const json::Value& object,
    const std::initializer_list<std::string_view> allowed,
    std::string& unexpected) {
    if (object.type != json::ValueType::object) return false;
    for (const auto& item : object.object) {
        if (std::find(allowed.begin(), allowed.end(), item.first) == allowed.end()) {
            unexpected = item.first;
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_u32(const json::Value& value, std::uint32_t& output) noexcept {
    if (value.type != json::ValueType::number || value.text.empty() || value.text.front() == '-') return false;
    std::uint64_t parsed{};
    const char* begin = value.text.data();
    const char* end = begin + value.text.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] std::optional<std::uint8_t> read_raw_byte(
    const std::span<const std::uint8_t> input,
    const std::uint64_t logical,
    const RawBinaryBoundary boundary,
    const std::uint8_t fill) noexcept {
    if (logical < input.size()) return input[static_cast<std::size_t>(logical)];
    if (boundary == RawBinaryBoundary::wrap && !input.empty()) {
        return input[static_cast<std::size_t>(logical % input.size())];
    }
    if (boundary == RawBinaryBoundary::fill) return fill;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint8_t> hex_nibble(const char value) noexcept {
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(10 + value - 'a');
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(10 + value - 'A');
    return std::nullopt;
}

}  // namespace

std::optional<EncodedMutationResult> mutate_encoded_bytes(
    const std::span<const std::uint8_t> input,
    const EncodedMutationPlan& plan,
    const RootSeed seed,
    std::string* error) {
    if (input.empty()) {
        if (error != nullptr) *error = "encoded mutation input must not be empty";
        return std::nullopt;
    }
    if (plan.maximum_output_bytes == 0U || input.size() > plan.maximum_output_bytes) {
        if (error != nullptr) *error = "encoded mutation input exceeds the configured output bound";
        return std::nullopt;
    }
    if (plan.protected_prefix_bytes > input.size()) {
        if (error != nullptr) *error = "protected prefix exceeds encoded input length";
        return std::nullopt;
    }
    if (plan.random_bit_flips > kMaximumMutationFlips) {
        if (error != nullptr) *error = "random bit flip count exceeds policy-v1 work bound";
        return std::nullopt;
    }
    const auto range_valid = [&](const std::uint64_t offset, const std::uint64_t length, const char* label) {
        if (length == 0U) return true;
        std::uint64_t end{};
        if (offset < plan.protected_prefix_bytes || !checked_add(offset, length, end) || end > input.size()) {
            if (error != nullptr) *error = std::string{label} + " range is outside the mutable encoded region";
            return false;
        }
        return true;
    };
    if (!range_valid(plan.xor_offset, plan.xor_length, "xor") ||
        !range_valid(plan.duplicate_offset, plan.duplicate_length, "duplicate") ||
        !range_valid(plan.drop_offset, plan.drop_length, "drop")) {
        return std::nullopt;
    }

    std::uint64_t duplicate_size{};
    if (!checked_add(static_cast<std::uint64_t>(input.size()), plan.duplicate_length, duplicate_size) ||
        duplicate_size > plan.maximum_output_bytes) {
        if (error != nullptr) *error = "duplicate range would exceed the configured output bound";
        return std::nullopt;
    }

    EncodedMutationResult result;
    result.bytes.assign(input.begin(), input.end());
    result.original_identity = sha256_hex(input);

    for (std::uint64_t index = 0U; index < plan.xor_length; ++index) {
        result.bytes[static_cast<std::size_t>(plan.xor_offset + index)] ^= plan.xor_mask;
    }

    const std::uint64_t mutable_length = static_cast<std::uint64_t>(result.bytes.size()) - plan.protected_prefix_bytes;
    for (std::uint64_t flip = 0U; flip < plan.random_bit_flips; ++flip) {
        if (mutable_length == 0U) break;
        const std::uint64_t words[] = {flip, static_cast<std::uint64_t>(result.bytes.size()), plan.protected_prefix_bytes};
        DeterministicStream stream = make_named_stream(seed, kLaboratoryMutationInstance, "encoded-bit-flip-v1", words);
        const std::uint64_t byte_index = plan.protected_prefix_bytes + stream.uniform_below(mutable_length);
        const std::uint8_t bit = static_cast<std::uint8_t>(stream.uniform_below(8U));
        result.bytes[static_cast<std::size_t>(byte_index)] ^= static_cast<std::uint8_t>(1U << bit);
    }

    if (plan.duplicate_length != 0U) {
        const auto first = result.bytes.begin() + static_cast<std::ptrdiff_t>(plan.duplicate_offset);
        const auto last = first + static_cast<std::ptrdiff_t>(plan.duplicate_length);
        std::vector<std::uint8_t> copy(first, last);
        result.bytes.insert(last, copy.begin(), copy.end());
    }

    if (plan.drop_length != 0U) {
        // The drop address names the original stream. Account for an earlier
        // duplicate only when it was inserted before the requested drop point.
        std::uint64_t adjusted = plan.drop_offset;
        if (plan.duplicate_length != 0U && plan.duplicate_offset + plan.duplicate_length <= plan.drop_offset) {
            adjusted += plan.duplicate_length;
        }
        const auto first = result.bytes.begin() + static_cast<std::ptrdiff_t>(adjusted);
        const auto last = first + static_cast<std::ptrdiff_t>(plan.drop_length);
        result.bytes.erase(first, last);
    }

    result.mutated_identity = sha256_hex(std::span<const std::uint8_t>{result.bytes});
    return result;
}

std::string encoded_mutation_plan_text(const EncodedMutationPlan& plan) {
    return "policy=1;protect=" + std::to_string(plan.protected_prefix_bytes) +
        ";flips=" + std::to_string(plan.random_bit_flips) +
        ";xor=" + std::to_string(plan.xor_offset) + ":" + std::to_string(plan.xor_length) + ":" + std::to_string(plan.xor_mask) +
        ";duplicate=" + std::to_string(plan.duplicate_offset) + ":" + std::to_string(plan.duplicate_length) +
        ";drop=" + std::to_string(plan.drop_offset) + ":" + std::to_string(plan.drop_length) +
        ";max=" + std::to_string(plan.maximum_output_bytes);
}

std::string_view raw_binary_boundary_name(const RawBinaryBoundary boundary) noexcept {
    switch (boundary) {
        case RawBinaryBoundary::wrap: return "wrap";
        case RawBinaryBoundary::fill: return "fill";
        case RawBinaryBoundary::drop: return "drop";
    }
    return "drop";
}

std::optional<RawBinaryResult> interpret_raw_binary(
    const std::span<const std::uint8_t> input,
    const RawBinarySpec& spec,
    std::string* error) {
    if (input.empty()) {
        if (error != nullptr) *error = "raw binary input must not be empty";
        return std::nullopt;
    }
    if (spec.width == 0U || spec.width > spec.maximum_dimension || spec.maximum_dimension == 0U ||
        spec.bytes_per_pixel < 1U || spec.bytes_per_pixel > 4U) {
        if (error != nullptr) *error = "raw binary dimensions/bytes-per-pixel are outside policy bounds";
        return std::nullopt;
    }
    std::uint64_t tight_stride{};
    if (!checked_mul(spec.width, spec.bytes_per_pixel, tight_stride)) {
        if (error != nullptr) *error = "raw binary tight stride overflow";
        return std::nullopt;
    }
    const std::uint64_t stride = spec.stride == 0U ? tight_stride : spec.stride;
    if (stride == 0U) {
        if (error != nullptr) *error = "raw binary stride must be positive";
        return std::nullopt;
    }
    std::uint32_t height = spec.height;
    if (height == 0U) {
        if (spec.offset >= input.size()) {
            if (error != nullptr) *error = "raw binary derived-height offset is beyond input";
            return std::nullopt;
        }
        const std::uint64_t remaining = static_cast<std::uint64_t>(input.size()) - spec.offset;
        const std::uint64_t derived = (remaining + stride - 1U) / stride;
        if (derived == 0U || derived > spec.maximum_dimension) {
            if (error != nullptr) *error = "raw binary derived height is outside policy bounds";
            return std::nullopt;
        }
        height = static_cast<std::uint32_t>(derived);
    }
    if (height == 0U || height > spec.maximum_dimension) {
        if (error != nullptr) *error = "raw binary height is outside policy bounds";
        return std::nullopt;
    }

    auto created = make_rgba8_image(spec.width, height);
    if (!created.ok()) {
        if (error != nullptr) *error = created.error.has_value() ? created.error->message : "could not allocate raw binary image";
        return std::nullopt;
    }
    ImageBuffer image = std::move(*created.image);
    for (std::uint32_t y = 0U; y < height; ++y) {
        std::uint64_t row_delta{};
        if (!checked_mul(y, stride, row_delta)) {
            if (error != nullptr) *error = "raw binary row address overflow";
            return std::nullopt;
        }
        std::uint64_t row_origin{};
        if (!checked_add(spec.offset, row_delta, row_origin)) {
            if (error != nullptr) *error = "raw binary row origin overflow";
            return std::nullopt;
        }
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            std::uint64_t pixel_delta{};
            if (!checked_mul(x, spec.bytes_per_pixel, pixel_delta)) {
                if (error != nullptr) *error = "raw binary pixel address overflow";
                return std::nullopt;
            }
            std::uint64_t pixel_origin{};
            if (!checked_add(row_origin, pixel_delta, pixel_origin)) {
                if (error != nullptr) *error = "raw binary pixel origin overflow";
                return std::nullopt;
            }
            std::uint8_t logical[4]{0U, 0U, 0U, 255U};
            bool dropped = false;
            for (std::uint32_t channel = 0U; channel < spec.bytes_per_pixel; ++channel) {
                std::uint64_t address{};
                if (!checked_add(pixel_origin, channel, address)) {
                    if (error != nullptr) *error = "raw binary channel address overflow";
                    return std::nullopt;
                }
                const auto value = read_raw_byte(input, address, spec.boundary, spec.fill_byte);
                if (!value.has_value()) {
                    dropped = true;
                    break;
                }
                logical[channel] = *value;
            }
            if (spec.bytes_per_pixel == 1U && !dropped) logical[1] = logical[2] = logical[0];
            const std::size_t destination = (static_cast<std::size_t>(y) * spec.width + x) * 4U;
            if (dropped) {
                image.bytes[destination + 0U] = 0U;
                image.bytes[destination + 1U] = 0U;
                image.bytes[destination + 2U] = 0U;
                image.bytes[destination + 3U] = 0U;
            } else {
                image.bytes[destination + 0U] = logical[0];
                image.bytes[destination + 1U] = logical[1];
                image.bytes[destination + 2U] = logical[2];
                image.bytes[destination + 3U] = spec.bytes_per_pixel == 4U ? logical[3] : 255U;
            }
        }
    }
    RawBinaryResult result;
    result.source_identity = source_identity_hex(image);
    result.image = std::move(image);
    return result;
}

std::string_view laboratory_outcome_name(const LaboratoryOutcome outcome) noexcept {
    switch (outcome) {
        case LaboratoryOutcome::success: return "success";
        case LaboratoryOutcome::decode_failure: return "decode-failure";
        case LaboratoryOutcome::worker_crash: return "worker-crash";
        case LaboratoryOutcome::timeout: return "timeout";
        case LaboratoryOutcome::cancelled: return "cancelled";
        case LaboratoryOutcome::rejected_response: return "rejected-response";
    }
    return "rejected-response";
}

std::optional<LaboratoryOutcome> parse_laboratory_outcome(const std::string_view text) noexcept {
    if (text == "success") return LaboratoryOutcome::success;
    if (text == "decode-failure") return LaboratoryOutcome::decode_failure;
    if (text == "worker-crash") return LaboratoryOutcome::worker_crash;
    if (text == "timeout") return LaboratoryOutcome::timeout;
    if (text == "cancelled") return LaboratoryOutcome::cancelled;
    if (text == "rejected-response") return LaboratoryOutcome::rejected_response;
    return std::nullopt;
}

bool validate_laboratory_provenance(const LaboratoryProvenance& provenance, std::string* error) {
    if (provenance.provenance_version != kLaboratoryProvenanceVersion ||
        !valid_hex64(provenance.original_encoded_identity) || !valid_hex64(provenance.mutated_encoded_identity) ||
        !valid_hex64(provenance.materialized_source_identity) || !RootSeed::parse(provenance.mutation_seed).has_value() ||
        provenance.mutation_parameters.empty() || provenance.worker_protocol_version == 0U ||
        provenance.worker_application_version.empty() || provenance.decoder_identifier.empty()) {
        if (error != nullptr) *error = "laboratory provenance is incomplete or invalid";
        return false;
    }
    if (provenance.outcome != LaboratoryOutcome::success) {
        if (error != nullptr) *error = "only successful decoder results can be materialized as project sources";
        return false;
    }
    return true;
}

bool validate_materialized_laboratory_source(const MaterializedLaboratorySource& source, std::string* error) {
    if (const auto image_error = validate_canonical_image(source.image); image_error.has_value()) {
        if (error != nullptr) *error = image_error->message;
        return false;
    }
    if (!validate_laboratory_provenance(source.provenance, error)) return false;
    if (source_identity_hex(source.image) != source.provenance.materialized_source_identity) {
        if (error != nullptr) *error = "materialized laboratory pixel identity does not match provenance";
        return false;
    }
    return true;
}

std::string serialize_laboratory_provenance_json(const LaboratoryProvenance& provenance) {
    std::string output{"{\"provenance_version\":" + std::to_string(provenance.provenance_version)};
    output += ",\"original_encoded_identity\":" + json_string(provenance.original_encoded_identity);
    output += ",\"mutated_encoded_identity\":" + json_string(provenance.mutated_encoded_identity);
    output += ",\"mutation_seed\":" + json_string(provenance.mutation_seed);
    output += ",\"mutation_parameters\":" + json_string(provenance.mutation_parameters);
    output += ",\"worker_protocol_version\":" + std::to_string(provenance.worker_protocol_version);
    output += ",\"worker_application_version\":" + json_string(provenance.worker_application_version);
    output += ",\"decoder_identifier\":" + json_string(provenance.decoder_identifier);
    output += ",\"os_build\":" + json_string(provenance.os_build);
    output += ",\"outcome\":" + json_string(laboratory_outcome_name(provenance.outcome));
    output += ",\"diagnostic\":" + json_string(provenance.diagnostic);
    output += ",\"materialized_source_identity\":" + json_string(provenance.materialized_source_identity) + "}\n";
    return output;
}

std::optional<LaboratoryProvenance> parse_laboratory_provenance_json(const std::string_view text, std::string* error) {
    const auto parsed = json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != json::ValueType::object) {
        if (error != nullptr) *error = parsed.error.has_value() ? parsed.error->message : "laboratory provenance root must be an object";
        return std::nullopt;
    }
    const json::Value& root = *parsed.value;
    std::string unexpected;
    if (!only_fields(root, {"provenance_version", "original_encoded_identity", "mutated_encoded_identity", "mutation_seed",
            "mutation_parameters", "worker_protocol_version", "worker_application_version", "decoder_identifier", "os_build",
            "outcome", "diagnostic", "materialized_source_identity"}, unexpected)) {
        if (error != nullptr) *error = "unexpected laboratory provenance field: " + unexpected;
        return std::nullopt;
    }
    const auto* version = field(root, "provenance_version");
    const auto* original = field(root, "original_encoded_identity");
    const auto* mutated = field(root, "mutated_encoded_identity");
    const auto* seed = field(root, "mutation_seed");
    const auto* params = field(root, "mutation_parameters");
    const auto* protocol = field(root, "worker_protocol_version");
    const auto* app = field(root, "worker_application_version");
    const auto* decoder = field(root, "decoder_identifier");
    const auto* os = field(root, "os_build");
    const auto* outcome = field(root, "outcome");
    const auto* diagnostic = field(root, "diagnostic");
    const auto* materialized = field(root, "materialized_source_identity");
    LaboratoryProvenance result;
    if (version == nullptr || original == nullptr || mutated == nullptr || seed == nullptr || params == nullptr || protocol == nullptr ||
        app == nullptr || decoder == nullptr || os == nullptr || outcome == nullptr || diagnostic == nullptr || materialized == nullptr ||
        !parse_u32(*version, result.provenance_version) || !parse_u32(*protocol, result.worker_protocol_version) ||
        original->type != json::ValueType::string || mutated->type != json::ValueType::string || seed->type != json::ValueType::string ||
        params->type != json::ValueType::string || app->type != json::ValueType::string || decoder->type != json::ValueType::string ||
        os->type != json::ValueType::string || outcome->type != json::ValueType::string || diagnostic->type != json::ValueType::string ||
        materialized->type != json::ValueType::string) {
        if (error != nullptr) *error = "laboratory provenance fields are missing or have invalid types";
        return std::nullopt;
    }
    result.original_encoded_identity = original->text;
    result.mutated_encoded_identity = mutated->text;
    result.mutation_seed = seed->text;
    result.mutation_parameters = params->text;
    result.worker_application_version = app->text;
    result.decoder_identifier = decoder->text;
    result.os_build = os->text;
    result.diagnostic = diagnostic->text;
    result.materialized_source_identity = materialized->text;
    const auto parsed_outcome = parse_laboratory_outcome(outcome->text);
    if (!parsed_outcome.has_value()) {
        if (error != nullptr) *error = "laboratory provenance outcome is invalid";
        return std::nullopt;
    }
    result.outcome = *parsed_outcome;
    if (!validate_laboratory_provenance(result, error)) return std::nullopt;
    return result;
}

std::string bytes_to_hex(const std::span<const std::uint8_t> bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string output;
    output.resize(bytes.size() * 2U);
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        output[index * 2U] = kHex[bytes[index] >> 4U];
        output[index * 2U + 1U] = kHex[bytes[index] & 0x0fU];
    }
    return output;
}

std::optional<std::vector<std::uint8_t>> bytes_from_hex(
    const std::string_view text,
    const std::uint64_t maximum_bytes,
    std::string* error) {
    if ((text.size() & 1U) != 0U || text.size() / 2U > maximum_bytes) {
        if (error != nullptr) *error = "hex byte payload has invalid length or exceeds the configured bound";
        return std::nullopt;
    }
    std::vector<std::uint8_t> output(text.size() / 2U);
    for (std::size_t index = 0U; index < output.size(); ++index) {
        const auto high = hex_nibble(text[index * 2U]);
        const auto low = hex_nibble(text[index * 2U + 1U]);
        if (!high.has_value() || !low.has_value()) {
            if (error != nullptr) *error = "hex byte payload contains a non-hexadecimal character";
            return std::nullopt;
        }
        output[index] = static_cast<std::uint8_t>((*high << 4U) | *low);
    }
    return output;
}

}  // namespace faultmine::core
