#include "faultmine/laboratory.hpp"

#include "json.hpp"
#include "faultmine/sha256.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <string>
#include <utility>

namespace faultmine::laboratory {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] std::string json_string(const std::string_view value) {
    return "\"" + core::json::escape_string(value) + "\"";
}

[[nodiscard]] const Value* field(const Value& value, const std::string_view name) noexcept {
    if (value.type != ValueType::object) return nullptr;
    for (const auto& pair : value.object) {
        if (pair.first == name) return &pair.second;
    }
    return nullptr;
}

[[nodiscard]] bool only_fields(
    const Value& value,
    const std::initializer_list<std::string_view> allowed,
    std::string& unexpected) {
    if (value.type != ValueType::object) return false;
    for (const auto& pair : value.object) {
        if (std::find(allowed.begin(), allowed.end(), pair.first) == allowed.end()) {
            unexpected = pair.first;
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_u64(const Value& value, std::uint64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty() || value.text.front() == '-') return false;
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto result = std::from_chars(first, last, output, 10);
    return result.ec == std::errc{} && result.ptr == last;
}

[[nodiscard]] bool valid_identity(const std::string_view value) noexcept {
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

[[nodiscard]] char hex_digit(const std::uint8_t value) noexcept {
    constexpr char digits[] = "0123456789abcdef";
    return digits[value & 0x0fU];
}

[[nodiscard]] std::string bytes_to_hex(const std::span<const std::uint8_t> bytes) {
    std::string result;
    result.resize(bytes.size() * 2U);
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        result[index * 2U] = hex_digit(static_cast<std::uint8_t>(bytes[index] >> 4U));
        result[index * 2U + 1U] = hex_digit(bytes[index]);
    }
    return result;
}

[[nodiscard]] std::optional<std::uint8_t> from_hex(const char character) noexcept {
    if (character >= '0' && character <= '9') return static_cast<std::uint8_t>(character - '0');
    if (character >= 'a' && character <= 'f') return static_cast<std::uint8_t>(10 + character - 'a');
    return std::nullopt;
}

[[nodiscard]] bool hex_to_bytes(
    const std::string_view text,
    const std::size_t maximum_bytes,
    std::vector<std::uint8_t>& output) {
    if ((text.size() & 1U) != 0U || text.size() / 2U > maximum_bytes) return false;
    output.clear();
    output.reserve(text.size() / 2U);
    for (std::size_t index = 0U; index < text.size(); index += 2U) {
        const auto high = from_hex(text[index]);
        const auto low = from_hex(text[index + 1U]);
        if (!high.has_value() || !low.has_value()) return false;
        output.push_back(static_cast<std::uint8_t>((*high << 4U) | *low));
    }
    return true;
}

[[nodiscard]] std::string mutation_kind_name(const ByteMutationKind kind) {
    switch (kind) {
        case ByteMutationKind::bit_flip: return "bit-flip";
        case ByteMutationKind::xor_range: return "xor-range";
        case ByteMutationKind::duplicate_range: return "duplicate-range";
        case ByteMutationKind::drop_range: return "drop-range";
    }
    return "bit-flip";
}

[[nodiscard]] std::size_t bytes_per_pixel(const RawFormat format) noexcept {
    switch (format) {
        case RawFormat::gray8: return 1U;
        case RawFormat::rgb8: return 3U;
        case RawFormat::rgba8:
        case RawFormat::bgra8: return 4U;
    }
    return 0U;
}

[[nodiscard]] bool checked_mul(const std::uint64_t left, const std::uint64_t right, std::uint64_t& output) noexcept {
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left) return false;
    output = left * right;
    return true;
}

[[nodiscard]] bool checked_add(const std::uint64_t left, const std::uint64_t right, std::uint64_t& output) noexcept {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) return false;
    output = left + right;
    return true;
}

[[nodiscard]] std::optional<std::uint8_t> read_raw_byte(
    const std::span<const std::uint8_t> bytes,
    const std::uint64_t offset,
    const RawBoundaryPolicy boundary,
    const std::uint8_t fill) noexcept {
    if (offset < bytes.size()) return bytes[static_cast<std::size_t>(offset)];
    switch (boundary) {
        case RawBoundaryPolicy::drop: return std::nullopt;
        case RawBoundaryPolicy::fill: return fill;
        case RawBoundaryPolicy::wrap:
            if (bytes.empty()) return std::nullopt;
            return bytes[static_cast<std::size_t>(offset % static_cast<std::uint64_t>(bytes.size()))];
    }
    return std::nullopt;
}

[[nodiscard]] std::string provenance_json(const LaboratoryProvenance& provenance) {
    std::string output{"{\"encoded_input_identity\":"};
    output += json_string(provenance.encoded_input_identity);
    output += ",\"mutated_input_identity\":" + json_string(provenance.mutated_input_identity);
    output += ",\"mutation_recipe\":" + json_string(provenance.mutation_recipe);
    output += ",\"root_seed\":" + json_string(provenance.root_seed);
    output += ",\"worker_protocol_version\":" + std::to_string(provenance.worker_protocol_version);
    output += ",\"worker_application_version\":" + json_string(provenance.worker_application_version);
    output += ",\"os_metadata\":" + json_string(provenance.os_metadata);
    output += ",\"decoder_metadata\":" + json_string(provenance.decoder_metadata);
    output += ",\"outcome\":" + json_string(outcome_name(provenance.outcome));
    output += ",\"diagnostic\":" + json_string(provenance.diagnostic);
    output += ",\"materialized_source_identity\":";
    output += provenance.materialized_source_identity.has_value()
        ? json_string(*provenance.materialized_source_identity)
        : "null";
    output += '}';
    return output;
}

[[nodiscard]] LaboratoryParseResult parse_provenance_value(const Value& root) {
    if (root.type != ValueType::object) return {std::nullopt, "laboratory provenance must be an object"};
    std::string unexpected;
    if (!only_fields(root, {
            "encoded_input_identity", "mutated_input_identity", "mutation_recipe", "root_seed",
            "worker_protocol_version", "worker_application_version", "os_metadata", "decoder_metadata",
            "outcome", "diagnostic", "materialized_source_identity"}, unexpected)) {
        return {std::nullopt, "unexpected laboratory provenance field: " + unexpected};
    }
    const Value* original = field(root, "encoded_input_identity");
    const Value* mutated = field(root, "mutated_input_identity");
    const Value* recipe = field(root, "mutation_recipe");
    const Value* seed = field(root, "root_seed");
    const Value* protocol = field(root, "worker_protocol_version");
    const Value* application = field(root, "worker_application_version");
    const Value* os = field(root, "os_metadata");
    const Value* decoder = field(root, "decoder_metadata");
    const Value* outcome = field(root, "outcome");
    const Value* diagnostic = field(root, "diagnostic");
    const Value* materialized = field(root, "materialized_source_identity");
    if (original == nullptr || mutated == nullptr || recipe == nullptr || seed == nullptr || protocol == nullptr ||
        application == nullptr || os == nullptr || decoder == nullptr || outcome == nullptr || diagnostic == nullptr ||
        materialized == nullptr) {
        return {std::nullopt, "laboratory provenance is missing a required field"};
    }
    if (original->type != ValueType::string || mutated->type != ValueType::string || recipe->type != ValueType::string ||
        seed->type != ValueType::string || application->type != ValueType::string || os->type != ValueType::string ||
        decoder->type != ValueType::string || outcome->type != ValueType::string || diagnostic->type != ValueType::string ||
        !valid_identity(original->text) || !valid_identity(mutated->text) || !core::RootSeed::parse(seed->text).has_value()) {
        return {std::nullopt, "laboratory provenance contains invalid identity, seed, or string fields"};
    }
    std::uint64_t protocol_number{};
    if (!parse_u64(*protocol, protocol_number) || protocol_number != kLaboratoryProtocolVersion) {
        return {std::nullopt, "unsupported laboratory worker protocol version"};
    }
    const auto parsed_outcome = parse_outcome(outcome->text);
    if (!parsed_outcome.has_value()) return {std::nullopt, "unknown laboratory outcome"};
    if (diagnostic->text.size() > kMaxLaboratoryDiagnosticBytes) return {std::nullopt, "laboratory diagnostic exceeds the bounded display contract"};

    LaboratoryProvenance result;
    result.encoded_input_identity = original->text;
    result.mutated_input_identity = mutated->text;
    result.mutation_recipe = recipe->text;
    result.root_seed = seed->text;
    result.worker_protocol_version = static_cast<std::uint32_t>(protocol_number);
    result.worker_application_version = application->text;
    result.os_metadata = os->text;
    result.decoder_metadata = decoder->text;
    result.outcome = *parsed_outcome;
    result.diagnostic = diagnostic->text;
    if (materialized->type == ValueType::string) {
        if (!valid_identity(materialized->text)) return {std::nullopt, "materialized source identity is invalid"};
        result.materialized_source_identity = materialized->text;
    } else if (materialized->type != ValueType::null_value) {
        return {std::nullopt, "materialized source identity must be a string or null"};
    }
    return {std::move(result), {}};
}

}  // namespace

std::string_view outcome_name(const Outcome outcome) noexcept {
    switch (outcome) {
        case Outcome::success: return "success";
        case Outcome::decode_failure: return "decode-failure";
        case Outcome::worker_crash: return "worker-crash";
        case Outcome::timeout: return "timeout";
        case Outcome::cancelled: return "cancelled";
        case Outcome::rejected_response: return "rejected-response";
    }
    return "rejected-response";
}

std::optional<Outcome> parse_outcome(const std::string_view text) noexcept {
    if (text == "success") return Outcome::success;
    if (text == "decode-failure") return Outcome::decode_failure;
    if (text == "worker-crash") return Outcome::worker_crash;
    if (text == "timeout") return Outcome::timeout;
    if (text == "cancelled") return Outcome::cancelled;
    if (text == "rejected-response") return Outcome::rejected_response;
    return std::nullopt;
}

std::string serialize_byte_mutation_plan(const ByteMutationPlan& plan) {
    std::string output{"{\"version\":1,\"seed\":"};
    output += json_string(plan.seed.to_string());
    output += ",\"protected_prefix\":" + std::to_string(plan.protected_prefix);
    output += ",\"operations\":[";
    for (std::size_t index = 0U; index < plan.operations.size(); ++index) {
        if (index != 0U) output += ',';
        const ByteMutationOperation& operation = plan.operations[index];
        output += "{\"kind\":" + json_string(mutation_kind_name(operation.kind));
        output += ",\"offset\":" + std::to_string(operation.offset);
        output += ",\"length\":" + std::to_string(operation.length);
        output += ",\"value\":" + std::to_string(operation.value);
        output += ",\"count\":" + std::to_string(operation.count) + '}';
    }
    output += "]}";
    return output;
}

ByteMutationResult mutate_encoded_bytes(
    const std::span<const std::uint8_t> input,
    const ByteMutationPlan& plan) {
    ByteMutationResult result;
    if (input.size() > kMaxLaboratoryEncodedBytes) {
        result.error = "encoded input exceeds the laboratory byte limit";
        return result;
    }
    if (plan.protected_prefix > input.size()) {
        result.error = "protected prefix exceeds encoded input length";
        return result;
    }
    result.bytes.assign(input.begin(), input.end());
    result.original_identity = core::sha256_hex(input);
    result.canonical_recipe = serialize_byte_mutation_plan(plan);

    for (std::size_t operation_index = 0U; operation_index < plan.operations.size(); ++operation_index) {
        const ByteMutationOperation& operation = plan.operations[operation_index];
        if (operation.offset < plan.protected_prefix || operation.offset > result.bytes.size()) {
            result.error = "mutation operation crosses the protected header prefix or starts beyond the current byte stream";
            return result;
        }
        const std::uint64_t remaining = static_cast<std::uint64_t>(result.bytes.size()) - operation.offset;
        const std::uint64_t effective_length = operation.length == 0U ? remaining : operation.length;
        if (effective_length > remaining) {
            result.error = "mutation operation range exceeds the current byte stream";
            return result;
        }
        const std::size_t begin = static_cast<std::size_t>(operation.offset);
        const std::size_t length = static_cast<std::size_t>(effective_length);

        switch (operation.kind) {
            case ByteMutationKind::bit_flip: {
                if (length == 0U || operation.count == 0U || operation.count > 1'000'000U) {
                    result.error = "bit-flip mutation requires a non-empty range and a bounded positive count";
                    return result;
                }
                for (std::uint64_t ordinal = 0U; ordinal < operation.count; ++ordinal) {
                    const std::uint64_t address_word = core::mix64(
                        plan.seed.value ^ (static_cast<std::uint64_t>(operation_index) << 32U) ^ ordinal ^ 0x464d2d424954ULL);
                    const std::uint64_t bit_word = core::mix64(address_word ^ 0xB17B17B17B17B17ULL);
                    const std::size_t position = begin + static_cast<std::size_t>(address_word % length);
                    const std::uint8_t mask = static_cast<std::uint8_t>(1U << static_cast<unsigned>(bit_word & 7U));
                    result.bytes[position] ^= mask;
                }
                break;
            }
            case ByteMutationKind::xor_range: {
                const std::uint8_t mask = static_cast<std::uint8_t>(operation.value & 0xffU);
                for (std::size_t index = 0U; index < length; ++index) result.bytes[begin + index] ^= mask;
                break;
            }
            case ByteMutationKind::duplicate_range: {
                if (length == 0U || operation.count == 0U || operation.count > 16U) {
                    result.error = "duplicate-range requires a non-empty range and 1..16 copies";
                    return result;
                }
                const std::uint64_t added = static_cast<std::uint64_t>(length) * operation.count;
                if (added > kMaxLaboratoryEncodedBytes || result.bytes.size() > kMaxLaboratoryEncodedBytes - static_cast<std::size_t>(added)) {
                    result.error = "duplicate-range would exceed the laboratory byte limit";
                    return result;
                }
                const std::vector<std::uint8_t> copy(result.bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                    result.bytes.begin() + static_cast<std::ptrdiff_t>(begin + length));
                std::vector<std::uint8_t> insertion;
                insertion.reserve(static_cast<std::size_t>(added));
                for (std::uint64_t copy_index = 0U; copy_index < operation.count; ++copy_index) {
                    insertion.insert(insertion.end(), copy.begin(), copy.end());
                }
                result.bytes.insert(result.bytes.begin() + static_cast<std::ptrdiff_t>(begin + length), insertion.begin(), insertion.end());
                break;
            }
            case ByteMutationKind::drop_range:
                result.bytes.erase(
                    result.bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                    result.bytes.begin() + static_cast<std::ptrdiff_t>(begin + length));
                break;
        }
    }

    result.mutated_identity = core::sha256_hex(result.bytes);
    return result;
}

RawInterpretResult interpret_binary_as_image(
    const std::span<const std::uint8_t> bytes,
    const RawBinarySpec& spec) {
    if (spec.width == 0U) return {std::nullopt, "raw interpretation width must be positive"};
    const std::uint64_t bpp = bytes_per_pixel(spec.format);
    std::uint64_t tight_stride{};
    if (!checked_mul(spec.width, bpp, tight_stride) || tight_stride == 0U) {
        return {std::nullopt, "raw interpretation row size overflows"};
    }
    const std::uint64_t stride = spec.stride == 0U ? tight_stride : spec.stride;
    if (stride < tight_stride) return {std::nullopt, "raw interpretation stride is smaller than one logical row"};
    if (spec.offset > bytes.size()) return {std::nullopt, "raw interpretation offset exceeds input length"};

    std::uint64_t height = spec.height;
    if (height == 0U) {
        const std::uint64_t available = static_cast<std::uint64_t>(bytes.size()) - spec.offset;
        height = available / stride;
        if (available % stride != 0U && spec.boundary != RawBoundaryPolicy::drop) ++height;
        if (height == 0U) return {std::nullopt, "raw interpretation cannot derive a positive finite height"};
    }
    if (height > std::numeric_limits<std::uint32_t>::max()) {
        return {std::nullopt, "raw interpretation height exceeds the canonical image contract"};
    }
    std::uint64_t last_row_offset{};
    std::uint64_t last_pixel_bytes{};
    if (!checked_mul(height - 1U, stride, last_row_offset) ||
        !checked_add(spec.offset, last_row_offset, last_row_offset) ||
        !checked_add(last_row_offset, tight_stride, last_pixel_bytes)) {
        return {std::nullopt, "raw interpretation address calculation overflows"};
    }
    if (spec.boundary == RawBoundaryPolicy::drop && last_pixel_bytes > bytes.size()) {
        return {std::nullopt, "raw interpretation drop policy encountered an incomplete requested image"};
    }

    auto created = core::make_rgba8_image(spec.width, static_cast<std::uint32_t>(height));
    if (!created.ok()) return {std::nullopt, created.error->message};
    if (created.image->bytes.size() > kMaxLaboratoryPixelBytes) {
        return {std::nullopt, "raw interpretation exceeds the laboratory pixel byte limit"};
    }
    core::ImageBuffer image = std::move(*created.image);
    for (std::uint64_t y = 0U; y < height; ++y) {
        std::uint64_t row_address{};
        if (!checked_mul(y, stride, row_address) || !checked_add(spec.offset, row_address, row_address)) {
            return {std::nullopt, "raw interpretation row address overflows"};
        }
        for (std::uint32_t x = 0U; x < spec.width; ++x) {
            std::uint64_t pixel_delta{};
            std::uint64_t pixel_address{};
            if (!checked_mul(x, bpp, pixel_delta) || !checked_add(row_address, pixel_delta, pixel_address)) {
                return {std::nullopt, "raw interpretation pixel address overflows"};
            }
            std::uint8_t channels[4]{0U, 0U, 0U, 255U};
            for (std::size_t channel = 0U; channel < static_cast<std::size_t>(bpp); ++channel) {
                const auto value = read_raw_byte(bytes, pixel_address + channel, spec.boundary, spec.fill_byte);
                if (!value.has_value()) return {std::nullopt, "raw interpretation drop policy encountered an unavailable byte"};
                channels[channel] = *value;
            }
            const std::size_t destination = static_cast<std::size_t>(y) * static_cast<std::size_t>(image.row_stride) +
                static_cast<std::size_t>(x) * 4U;
            switch (spec.format) {
                case RawFormat::gray8:
                    image.bytes[destination] = channels[0];
                    image.bytes[destination + 1U] = channels[0];
                    image.bytes[destination + 2U] = channels[0];
                    image.bytes[destination + 3U] = 255U;
                    break;
                case RawFormat::rgb8:
                    image.bytes[destination] = channels[0];
                    image.bytes[destination + 1U] = channels[1];
                    image.bytes[destination + 2U] = channels[2];
                    image.bytes[destination + 3U] = 255U;
                    break;
                case RawFormat::rgba8:
                    for (std::size_t channel = 0U; channel < 4U; ++channel) image.bytes[destination + channel] = channels[channel];
                    break;
                case RawFormat::bgra8:
                    image.bytes[destination] = channels[2];
                    image.bytes[destination + 1U] = channels[1];
                    image.bytes[destination + 2U] = channels[0];
                    image.bytes[destination + 3U] = channels[3];
                    break;
            }
        }
    }
    return {std::move(image), {}};
}

std::string serialize_laboratory_provenance(const LaboratoryProvenance& provenance) {
    return provenance_json(provenance) + "\n";
}

LaboratoryParseResult parse_laboratory_provenance(const std::string_view text) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value()) {
        return {std::nullopt, parsed.error.has_value() ? parsed.error->message : "invalid laboratory provenance JSON"};
    }
    return parse_provenance_value(*parsed.value);
}

std::optional<std::string> validate_materialized_source(const MaterializedSource& source) {
    if (const auto error = core::validate_canonical_image(source.image); error.has_value()) return error->message;
    if (source.image.bytes.size() > kMaxLaboratoryPixelBytes) return std::string{"materialized source exceeds the laboratory pixel byte limit"};
    if (source.provenance.outcome != Outcome::success) return std::string{"only a successful worker result can be materialized"};
    const std::string identity = core::source_identity_hex(source.image);
    if (!source.provenance.materialized_source_identity.has_value() ||
        *source.provenance.materialized_source_identity != identity) {
        return std::string{"materialized source identity does not match frozen normalized pixels"};
    }
    if (!valid_identity(source.provenance.encoded_input_identity) || !valid_identity(source.provenance.mutated_input_identity)) {
        return std::string{"materialized source encoded identities are invalid"};
    }
    return std::nullopt;
}

std::string serialize_materialized_source(const MaterializedSource& source) {
    std::string output{"{\"materialized_source_version\":1,\"width\":"};
    output += std::to_string(source.image.width);
    output += ",\"height\":" + std::to_string(source.image.height);
    output += ",\"format\":\"rgba8-unorm\",\"rgba8_hex\":" + json_string(bytes_to_hex(source.image.bytes));
    output += ",\"provenance\":" + provenance_json(source.provenance) + "}\n";
    return output;
}

MaterializedParseResult parse_materialized_source(const std::string_view text) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) {
        return {std::nullopt, parsed.error.has_value() ? parsed.error->message : "materialized source root must be an object"};
    }
    const Value& root = *parsed.value;
    std::string unexpected;
    if (!only_fields(root, {"materialized_source_version", "width", "height", "format", "rgba8_hex", "provenance"}, unexpected)) {
        return {std::nullopt, "unexpected materialized source field: " + unexpected};
    }
    const Value* version = field(root, "materialized_source_version");
    const Value* width = field(root, "width");
    const Value* height = field(root, "height");
    const Value* format = field(root, "format");
    const Value* pixels = field(root, "rgba8_hex");
    const Value* provenance = field(root, "provenance");
    std::uint64_t parsed_version{};
    std::uint64_t parsed_width{};
    std::uint64_t parsed_height{};
    if (version == nullptr || width == nullptr || height == nullptr || format == nullptr || pixels == nullptr || provenance == nullptr ||
        !parse_u64(*version, parsed_version) || parsed_version != 1U ||
        !parse_u64(*width, parsed_width) || parsed_width == 0U || parsed_width > std::numeric_limits<std::uint32_t>::max() ||
        !parse_u64(*height, parsed_height) || parsed_height == 0U || parsed_height > std::numeric_limits<std::uint32_t>::max() ||
        format->type != ValueType::string || format->text != "rgba8-unorm" || pixels->type != ValueType::string) {
        return {std::nullopt, "materialized source metadata is invalid"};
    }
    const LaboratoryParseResult provenance_result = parse_provenance_value(*provenance);
    if (!provenance_result.ok()) return {std::nullopt, provenance_result.error};
    const auto expected_size = core::canonical_rgba8_byte_size(
        static_cast<std::uint32_t>(parsed_width), static_cast<std::uint32_t>(parsed_height));
    if (!expected_size.has_value() || *expected_size > kMaxLaboratoryPixelBytes || pixels->text.size() != *expected_size * 2U) {
        return {std::nullopt, "materialized source pixel length does not match dimensions or exceeds the bounded contract"};
    }
    std::vector<std::uint8_t> bytes;
    if (!hex_to_bytes(pixels->text, kMaxLaboratoryPixelBytes, bytes)) return {std::nullopt, "materialized source pixels are not valid lowercase hexadecimal bytes"};
    auto created = core::make_rgba8_image(
        static_cast<std::uint32_t>(parsed_width), static_cast<std::uint32_t>(parsed_height), bytes);
    if (!created.ok()) return {std::nullopt, created.error->message};
    MaterializedSource result{std::move(*created.image), std::move(*provenance_result.provenance)};
    if (const auto validation = validate_materialized_source(result); validation.has_value()) return {std::nullopt, *validation};
    return {std::move(result), {}};
}

}  // namespace faultmine::laboratory
