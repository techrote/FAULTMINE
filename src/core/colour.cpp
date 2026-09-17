#include "faultmine/colour.hpp"

#include "faultmine/sha256.hpp"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::core {
namespace {

[[nodiscard]] int hex_nibble(const char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return 10 + character - 'a';
    }
    if (character >= 'A' && character <= 'F') {
        return 10 + character - 'A';
    }
    return -1;
}

[[nodiscard]] char hex_digit(const std::uint8_t value) noexcept {
    constexpr char digits[] = "0123456789abcdef";
    return digits[value & 0x0fU];
}

void append_hex_byte(std::string& output, const std::uint8_t value) {
    output.push_back(hex_digit(static_cast<std::uint8_t>(value >> 4U)));
    output.push_back(hex_digit(value));
}

[[nodiscard]] const json::Value* find_member(
    const json::Value& object,
    const std::string_view name) noexcept {
    if (object.type != json::ValueType::object) {
        return nullptr;
    }
    for (const auto& member : object.object) {
        if (member.first == name) {
            return &member.second;
        }
    }
    return nullptr;
}

[[nodiscard]] bool member_allowed(
    const std::string_view name,
    const std::initializer_list<std::string_view> allowed) noexcept {
    for (const std::string_view candidate : allowed) {
        if (name == candidate) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] ColourAssetError make_error(
    const ColourAssetErrorCode code,
    std::string path,
    std::string message,
    const std::optional<std::size_t> offset = std::nullopt) {
    return ColourAssetError{code, std::move(path), std::move(message), offset};
}

[[nodiscard]] std::optional<ColourAssetError> validate_palette(const Palette& palette) {
    if (palette.schema_version != kPaletteSchemaVersion) {
        return make_error(ColourAssetErrorCode::unsupported_version, "$.schema_version", "unsupported palette schema version");
    }
    if (palette.entries.empty() || palette.entries.size() > kMaximumPaletteEntries) {
        return make_error(ColourAssetErrorCode::invalid_value, "$.entries", "palette must contain between 1 and 256 entries");
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ColourAssetError> validate_lut(const Lut256& lut) {
    if (lut.schema_version != kLutSchemaVersion) {
        return make_error(ColourAssetErrorCode::unsupported_version, "$.schema_version", "unsupported LUT schema version");
    }
    return std::nullopt;
}

[[nodiscard]] std::string serialize_channel_lut(const std::array<std::uint8_t, 256U>& channel) {
    std::string output;
    output.reserve(512U);
    for (const std::uint8_t value : channel) {
        append_hex_byte(output, value);
    }
    return output;
}

[[nodiscard]] bool parse_channel_lut(
    const std::string_view text,
    std::array<std::uint8_t, 256U>& output) noexcept {
    if (text.size() != 512U) {
        return false;
    }
    for (std::size_t index = 0U; index < output.size(); ++index) {
        const int high = hex_nibble(text[index * 2U]);
        const int low = hex_nibble(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return false;
        }
        output[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return true;
}

[[nodiscard]] std::uint8_t interpolate_byte(
    const std::uint8_t start,
    const std::uint8_t end,
    const std::uint32_t numerator,
    const std::uint32_t denominator) noexcept {
    if (denominator == 0U) {
        return start;
    }
    const std::uint32_t weighted =
        static_cast<std::uint32_t>(start) * (denominator - numerator) +
        static_cast<std::uint32_t>(end) * numerator;
    return static_cast<std::uint8_t>((weighted + denominator / 2U) / denominator);
}

[[nodiscard]] std::uint8_t add_clamped_jitter(
    const std::uint8_t value,
    const std::int64_t delta) noexcept {
    const std::int64_t adjusted = static_cast<std::int64_t>(value) + delta;
    if (adjusted < 0) {
        return 0U;
    }
    if (adjusted > 255) {
        return 255U;
    }
    return static_cast<std::uint8_t>(adjusted);
}

}  // namespace

std::string rgba8_to_hex(const Rgba8 colour) {
    std::string output;
    output.reserve(8U);
    append_hex_byte(output, colour.r);
    append_hex_byte(output, colour.g);
    append_hex_byte(output, colour.b);
    append_hex_byte(output, colour.a);
    return output;
}

std::optional<Rgba8> parse_rgba8_hex(const std::string_view text) noexcept {
    if (text.size() != 8U) {
        return std::nullopt;
    }
    std::array<std::uint8_t, 4U> bytes{};
    for (std::size_t index = 0U; index < bytes.size(); ++index) {
        const int high = hex_nibble(text[index * 2U]);
        const int low = hex_nibble(text[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return Rgba8{bytes[0], bytes[1], bytes[2], bytes[3]};
}

std::string serialize_palette_canonical(const Palette& palette) {
    if (const auto error = validate_palette(palette); error.has_value()) {
        throw std::invalid_argument(error->message);
    }
    std::string output{"{\"schema_version\":1,\"entries\":["};
    for (std::size_t index = 0U; index < palette.entries.size(); ++index) {
        if (index != 0U) {
            output.push_back(',');
        }
        output.push_back('"');
        output += rgba8_to_hex(palette.entries[index]);
        output.push_back('"');
    }
    output += "]}\n";
    return output;
}

PaletteParseResult parse_palette(const std::string_view text) {
    const json::ParseResult parsed = json::parse(text);
    if (!parsed.value.has_value()) {
        const ColourAssetErrorCode code = parsed.error.has_value() && parsed.error->kind == json::ParseErrorKind::duplicate_key
            ? ColourAssetErrorCode::duplicate_field
            : ColourAssetErrorCode::syntax_error;
        return PaletteParseResult{
            std::nullopt,
            make_error(
                code,
                "$",
                parsed.error.has_value() ? parsed.error->message : "palette JSON parse failed",
                parsed.error.has_value() ? std::optional<std::size_t>{parsed.error->offset} : std::nullopt)};
    }
    const json::Value& root = *parsed.value;
    if (root.type != json::ValueType::object) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, "$", "palette root must be an object")};
    }
    for (const auto& member : root.object) {
        if (!member_allowed(member.first, {"schema_version", "entries"})) {
            return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::unexpected_field, "$." + member.first, "unexpected palette field")};
        }
    }
    const json::Value* version = find_member(root, "schema_version");
    const json::Value* entries = find_member(root, "entries");
    if (version == nullptr) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::missing_field, "$.schema_version", "palette schema_version is required")};
    }
    if (entries == nullptr) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::missing_field, "$.entries", "palette entries are required")};
    }
    if (version->type != json::ValueType::number) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, "$.schema_version", "palette schema_version must be a number")};
    }
    if (version->text != "1") {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::unsupported_version, "$.schema_version", "unsupported palette schema version")};
    }
    if (entries->type != json::ValueType::array) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, "$.entries", "palette entries must be an array")};
    }
    if (entries->array.empty() || entries->array.size() > kMaximumPaletteEntries) {
        return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::invalid_value, "$.entries", "palette must contain between 1 and 256 entries")};
    }

    Palette palette;
    palette.entries.reserve(entries->array.size());
    for (std::size_t index = 0U; index < entries->array.size(); ++index) {
        const json::Value& value = entries->array[index];
        const std::string path = "$.entries[" + std::to_string(index) + "]";
        if (value.type != json::ValueType::string) {
            return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, path, "palette entry must be an 8-digit RGBA hex string")};
        }
        const auto colour = parse_rgba8_hex(value.text);
        if (!colour.has_value()) {
            return PaletteParseResult{std::nullopt, make_error(ColourAssetErrorCode::invalid_value, path, "palette entry must be exactly rrggbbaa hexadecimal")};
        }
        palette.entries.push_back(*colour);
    }
    return PaletteParseResult{std::move(palette), std::nullopt};
}

std::string palette_identity_hex(const Palette& palette) {
    return sha256_hex(serialize_palette_canonical(palette));
}

Lut256 make_identity_lut() {
    Lut256 lut;
    for (std::size_t channel = 0U; channel < lut.channels.size(); ++channel) {
        for (std::size_t value = 0U; value < 256U; ++value) {
            lut.channels[channel][value] = static_cast<std::uint8_t>(value);
        }
    }
    return lut;
}

std::string serialize_lut_canonical(const Lut256& lut) {
    if (const auto error = validate_lut(lut); error.has_value()) {
        throw std::invalid_argument(error->message);
    }
    std::string output{"{\"schema_version\":1,\"channels\":{\"r\":\""};
    output += serialize_channel_lut(lut.channels[0]);
    output += "\",\"g\":\"";
    output += serialize_channel_lut(lut.channels[1]);
    output += "\",\"b\":\"";
    output += serialize_channel_lut(lut.channels[2]);
    output += "\",\"a\":\"";
    output += serialize_channel_lut(lut.channels[3]);
    output += "\"}}\n";
    return output;
}

LutParseResult parse_lut(const std::string_view text) {
    const json::ParseResult parsed = json::parse(text);
    if (!parsed.value.has_value()) {
        const ColourAssetErrorCode code = parsed.error.has_value() && parsed.error->kind == json::ParseErrorKind::duplicate_key
            ? ColourAssetErrorCode::duplicate_field
            : ColourAssetErrorCode::syntax_error;
        return LutParseResult{
            std::nullopt,
            make_error(
                code,
                "$",
                parsed.error.has_value() ? parsed.error->message : "LUT JSON parse failed",
                parsed.error.has_value() ? std::optional<std::size_t>{parsed.error->offset} : std::nullopt)};
    }
    const json::Value& root = *parsed.value;
    if (root.type != json::ValueType::object) {
        return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, "$", "LUT root must be an object")};
    }
    for (const auto& member : root.object) {
        if (!member_allowed(member.first, {"schema_version", "channels"})) {
            return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::unexpected_field, "$." + member.first, "unexpected LUT field")};
        }
    }
    const json::Value* version = find_member(root, "schema_version");
    const json::Value* channels = find_member(root, "channels");
    if (version == nullptr || channels == nullptr) {
        return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::missing_field, "$", "LUT requires schema_version and channels")};
    }
    if (version->type != json::ValueType::number || version->text != "1") {
        return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::unsupported_version, "$.schema_version", "unsupported LUT schema version")};
    }
    if (channels->type != json::ValueType::object) {
        return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, "$.channels", "LUT channels must be an object")};
    }
    for (const auto& member : channels->object) {
        if (!member_allowed(member.first, {"r", "g", "b", "a"})) {
            return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::unexpected_field, "$.channels." + member.first, "unexpected LUT channel")};
        }
    }

    Lut256 lut;
    constexpr std::array<std::string_view, 4U> names{"r", "g", "b", "a"};
    for (std::size_t index = 0U; index < names.size(); ++index) {
        const json::Value* channel = find_member(*channels, names[index]);
        const std::string path = "$.channels." + std::string{names[index]};
        if (channel == nullptr) {
            return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::missing_field, path, "LUT channel is required")};
        }
        if (channel->type != json::ValueType::string) {
            return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::wrong_type, path, "LUT channel must be a 512-digit hexadecimal string")};
        }
        if (!parse_channel_lut(channel->text, lut.channels[index])) {
            return LutParseResult{std::nullopt, make_error(ColourAssetErrorCode::invalid_value, path, "LUT channel must encode exactly 256 bytes as hexadecimal")};
        }
    }
    return LutParseResult{std::move(lut), std::nullopt};
}

std::string lut_identity_hex(const Lut256& lut) {
    return sha256_hex(serialize_lut_canonical(lut));
}

std::optional<Palette> generate_palette_ramp(
    const PaletteGenerationSpec& spec,
    const RootSeed root_seed,
    const InstanceId instance_id,
    std::string* error) {
    if (spec.count == 0U || spec.count > kMaximumPaletteEntries) {
        if (error != nullptr) {
            *error = "palette generator count must be in the inclusive range 1..256";
        }
        return std::nullopt;
    }

    Palette palette;
    palette.entries.reserve(spec.count);
    const std::uint32_t denominator = spec.count > 1U ? spec.count - 1U : 0U;
    for (std::uint32_t index = 0U; index < spec.count; ++index) {
        Rgba8 colour{
            interpolate_byte(spec.start.r, spec.end.r, index, denominator),
            interpolate_byte(spec.start.g, spec.end.g, index, denominator),
            interpolate_byte(spec.start.b, spec.end.b, index, denominator),
            interpolate_byte(spec.start.a, spec.end.a, index, denominator)};

        if (spec.jitter != 0U && index != 0U && index + 1U != spec.count) {
            for (std::uint64_t channel = 0U; channel < 3U; ++channel) {
                const std::array<std::uint64_t, 4U> identity{
                    static_cast<std::uint64_t>(index),
                    channel,
                    static_cast<std::uint64_t>(spec.count),
                    static_cast<std::uint64_t>(spec.jitter)};
                auto stream = make_named_stream(root_seed, instance_id, "palette-ramp-jitter", identity);
                const std::uint64_t span = static_cast<std::uint64_t>(spec.jitter) * 2U + 1U;
                const std::int64_t delta =
                    static_cast<std::int64_t>(stream.uniform_below(span)) -
                    static_cast<std::int64_t>(spec.jitter);
                if (channel == 0U) {
                    colour.r = add_clamped_jitter(colour.r, delta);
                } else if (channel == 1U) {
                    colour.g = add_clamped_jitter(colour.g, delta);
                } else {
                    colour.b = add_clamped_jitter(colour.b, delta);
                }
            }
        }
        palette.entries.push_back(colour);
    }
    return palette;
}

}  // namespace faultmine::core
