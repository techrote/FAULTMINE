#include "faultmine/representation_bit_operators.hpp"

#include "faultmine/logical_address.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

[[nodiscard]] const ParameterValue* find_parameter(
    const OperatorInstance& instance,
    const std::string_view name) noexcept {
    const auto iterator = instance.parameters.find(name);
    return iterator == instance.parameters.end() ? nullptr : &iterator->second;
}

[[nodiscard]] std::optional<std::string> get_i64(
    const OperatorInstance& instance,
    const std::string_view name,
    std::int64_t& output) {
    const ParameterValue* value = find_parameter(instance, name);
    if (value == nullptr) {
        return "missing signed parameter '" + std::string{name} + "'";
    }
    const auto* typed = std::get_if<std::int64_t>(value);
    if (typed == nullptr) {
        return "parameter '" + std::string{name} + "' must be i64";
    }
    output = *typed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> get_u64(
    const OperatorInstance& instance,
    const std::string_view name,
    std::uint64_t& output) {
    const ParameterValue* value = find_parameter(instance, name);
    if (value == nullptr) {
        return "missing unsigned parameter '" + std::string{name} + "'";
    }
    const auto* typed = std::get_if<std::uint64_t>(value);
    if (typed == nullptr) {
        return "parameter '" + std::string{name} + "' must be u64";
    }
    output = *typed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> get_text(
    const OperatorInstance& instance,
    const std::string_view name,
    std::string_view& output) {
    const ParameterValue* value = find_parameter(instance, name);
    if (value == nullptr) {
        return "missing text parameter '" + std::string{name} + "'";
    }
    const auto* typed = std::get_if<std::string>(value);
    if (typed == nullptr) {
        return "parameter '" + std::string{name} + "' must be string";
    }
    output = *typed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> allocate_like(
    const ImageBuffer& input,
    ImageBuffer& output) {
    auto created = make_rgba8_image(input.width, input.height);
    if (!created.ok()) {
        return "failed to allocate canonical operator output";
    }
    output = std::move(*created.image);
    return std::nullopt;
}

[[nodiscard]] std::uint64_t pixel_count(const ImageBuffer& image) noexcept {
    return static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height);
}

[[nodiscard]] std::optional<std::string> parse_channels(
    const std::string_view text,
    std::array<bool, 4>& channels) {
    channels = {false, false, false, false};
    if (text.empty()) {
        return "channels must contain at least one of r, g, b, a";
    }
    for (const char character : text) {
        std::size_t index = 0U;
        switch (character) {
            case 'r': index = 0U; break;
            case 'g': index = 1U; break;
            case 'b': index = 2U; break;
            case 'a': index = 3U; break;
            default: return "channels may contain only r, g, b, a";
        }
        if (channels[index]) {
            return "channels must not contain duplicates";
        }
        channels[index] = true;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> get_boundary(
    const OperatorInstance& instance,
    BoundaryPolicy& output) {
    std::string_view text;
    if (auto error = get_text(instance, "boundary", text); error.has_value()) {
        return error;
    }
    const auto parsed = parse_boundary_policy(text);
    if (!parsed.has_value()) {
        return "boundary must be one of wrap, clamp, fill";
    }
    output = *parsed;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::uint8_t> routed_channel_value(
    const ImageBuffer& input,
    const std::size_t pixel_offset,
    const char route) noexcept {
    switch (route) {
        case 'r': return input.bytes[pixel_offset + 0U];
        case 'g': return input.bytes[pixel_offset + 1U];
        case 'b': return input.bytes[pixel_offset + 2U];
        case 'a': return input.bytes[pixel_offset + 3U];
        case '0': return std::uint8_t{0U};
        case '1': return std::uint8_t{255U};
        default: return std::nullopt;
    }
}

[[nodiscard]] std::optional<std::string> execute_channel_route(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view routes;
    if (auto error = get_text(instance, "routes", routes); error.has_value()) {
        return error;
    }
    if (routes.size() != 4U) {
        return "routes must contain exactly four symbols from r, g, b, a, 0, 1";
    }
    for (const char route : routes) {
        if (route != 'r' && route != 'g' && route != 'b' && route != 'a' && route != '0' && route != '1') {
            return "routes may contain only r, g, b, a, 0, 1";
        }
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            output.bytes[offset + channel] = *routed_channel_value(input, offset, routes[channel]);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_channel_offset(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    std::int64_t dx = 0;
    std::int64_t dy = 0;
    BoundaryPolicy boundary{};
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) {
        return error;
    }
    if (auto error = get_i64(instance, "dx", dx); error.has_value()) {
        return error;
    }
    if (auto error = get_i64(instance, "dy", dy); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) {
        return error;
    }

    output = input;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            const auto source_x = resolve_displaced_index(x, dx, input.width, boundary);
            const auto source_y = resolve_displaced_index(y, dy, input.height, boundary);
            const std::size_t destination_offset =
                (static_cast<std::size_t>(y) * input.width + x) * 4U;
            for (std::size_t channel = 0U; channel < 4U; ++channel) {
                if (!channels[channel]) {
                    continue;
                }
                if (!source_x.has_value() || !source_y.has_value()) {
                    output.bytes[destination_offset + channel] = 0U;
                    continue;
                }
                const std::size_t source_offset =
                    (static_cast<std::size_t>(*source_y) * input.width + static_cast<std::size_t>(*source_x)) * 4U;
                output.bytes[destination_offset + channel] = input.bytes[source_offset + channel];
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_word_lanes(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t word_bytes = 0U;
    std::string_view mode;
    if (auto error = get_u64(instance, "word_bytes", word_bytes); error.has_value()) {
        return error;
    }
    if (word_bytes != 2U && word_bytes != 4U) {
        return "word_bytes must be 2 or 4";
    }
    if (auto error = get_text(instance, "mode", mode); error.has_value()) {
        return error;
    }
    if (mode != "reverse" && mode != "rotate-left" && mode != "rotate-right") {
        return "mode must be reverse, rotate-left, or rotate-right";
    }

    output = input;
    const std::size_t width = static_cast<std::size_t>(word_bytes);
    for (std::size_t base = 0U; base < input.bytes.size(); base += width) {
        for (std::size_t lane = 0U; lane < width; ++lane) {
            std::size_t source_lane = 0U;
            if (mode == "reverse") {
                source_lane = width - 1U - lane;
            } else if (mode == "rotate-left") {
                source_lane = (lane + 1U) % width;
            } else {
                source_lane = (lane + width - 1U) % width;
            }
            output.bytes[base + lane] = input.bytes[base + source_lane];
        }
    }
    return std::nullopt;
}

enum class PackedFormat {
    rgb565,
    bgr565,
    rgba4444,
    argb1555,
};

enum class ByteOrder {
    little,
    big,
};

[[nodiscard]] std::optional<PackedFormat> parse_packed_format(const std::string_view text) noexcept {
    if (text == "rgb565") return PackedFormat::rgb565;
    if (text == "bgr565") return PackedFormat::bgr565;
    if (text == "rgba4444") return PackedFormat::rgba4444;
    if (text == "argb1555") return PackedFormat::argb1555;
    return std::nullopt;
}

[[nodiscard]] std::optional<ByteOrder> parse_byte_order(const std::string_view text) noexcept {
    if (text == "little") return ByteOrder::little;
    if (text == "big") return ByteOrder::big;
    return std::nullopt;
}

[[nodiscard]] std::uint16_t pack_pixel(
    const PackedFormat format,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue,
    const std::uint8_t alpha) noexcept {
    switch (format) {
        case PackedFormat::rgb565:
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(red >> 3U) << 11U) |
                (static_cast<std::uint16_t>(green >> 2U) << 5U) |
                static_cast<std::uint16_t>(blue >> 3U));
        case PackedFormat::bgr565:
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(blue >> 3U) << 11U) |
                (static_cast<std::uint16_t>(green >> 2U) << 5U) |
                static_cast<std::uint16_t>(red >> 3U));
        case PackedFormat::rgba4444:
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(red >> 4U) << 12U) |
                (static_cast<std::uint16_t>(green >> 4U) << 8U) |
                (static_cast<std::uint16_t>(blue >> 4U) << 4U) |
                static_cast<std::uint16_t>(alpha >> 4U));
        case PackedFormat::argb1555:
            return static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(alpha >= 128U ? 1U : 0U) << 15U) |
                (static_cast<std::uint16_t>(red >> 3U) << 10U) |
                (static_cast<std::uint16_t>(green >> 3U) << 5U) |
                static_cast<std::uint16_t>(blue >> 3U));
    }
    return 0U;
}

[[nodiscard]] std::uint8_t expand4(const std::uint16_t value) noexcept {
    return static_cast<std::uint8_t>((value << 4U) | value);
}

[[nodiscard]] std::uint8_t expand5(const std::uint16_t value) noexcept {
    return static_cast<std::uint8_t>((value << 3U) | (value >> 2U));
}

[[nodiscard]] std::uint8_t expand6(const std::uint16_t value) noexcept {
    return static_cast<std::uint8_t>((value << 2U) | (value >> 4U));
}

void decode_pixel(
    const PackedFormat format,
    const std::uint16_t word,
    std::uint8_t* output) noexcept {
    switch (format) {
        case PackedFormat::rgb565:
            output[0] = expand5(static_cast<std::uint16_t>((word >> 11U) & 0x1fU));
            output[1] = expand6(static_cast<std::uint16_t>((word >> 5U) & 0x3fU));
            output[2] = expand5(static_cast<std::uint16_t>(word & 0x1fU));
            output[3] = 255U;
            break;
        case PackedFormat::bgr565:
            output[0] = expand5(static_cast<std::uint16_t>(word & 0x1fU));
            output[1] = expand6(static_cast<std::uint16_t>((word >> 5U) & 0x3fU));
            output[2] = expand5(static_cast<std::uint16_t>((word >> 11U) & 0x1fU));
            output[3] = 255U;
            break;
        case PackedFormat::rgba4444:
            output[0] = expand4(static_cast<std::uint16_t>((word >> 12U) & 0x0fU));
            output[1] = expand4(static_cast<std::uint16_t>((word >> 8U) & 0x0fU));
            output[2] = expand4(static_cast<std::uint16_t>((word >> 4U) & 0x0fU));
            output[3] = expand4(static_cast<std::uint16_t>(word & 0x0fU));
            break;
        case PackedFormat::argb1555:
            output[0] = expand5(static_cast<std::uint16_t>((word >> 10U) & 0x1fU));
            output[1] = expand5(static_cast<std::uint16_t>((word >> 5U) & 0x1fU));
            output[2] = expand5(static_cast<std::uint16_t>(word & 0x1fU));
            output[3] = ((word >> 15U) & 1U) != 0U ? 255U : 0U;
            break;
    }
}

[[nodiscard]] std::optional<std::string> execute_packed_reinterpret(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view source_format_text;
    std::string_view source_endian_text;
    std::string_view interpret_format_text;
    std::string_view interpret_endian_text;
    if (auto error = get_text(instance, "source_format", source_format_text); error.has_value()) return error;
    if (auto error = get_text(instance, "source_endian", source_endian_text); error.has_value()) return error;
    if (auto error = get_text(instance, "interpret_format", interpret_format_text); error.has_value()) return error;
    if (auto error = get_text(instance, "interpret_endian", interpret_endian_text); error.has_value()) return error;

    const auto source_format = parse_packed_format(source_format_text);
    const auto source_endian = parse_byte_order(source_endian_text);
    const auto interpret_format = parse_packed_format(interpret_format_text);
    const auto interpret_endian = parse_byte_order(interpret_endian_text);
    if (!source_format.has_value() || !interpret_format.has_value()) {
        return "packed format must be rgb565, bgr565, rgba4444, or argb1555";
    }
    if (!source_endian.has_value() || !interpret_endian.has_value()) {
        return "endianness must be little or big";
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        const std::uint16_t packed = pack_pixel(
            *source_format,
            input.bytes[offset + 0U],
            input.bytes[offset + 1U],
            input.bytes[offset + 2U],
            input.bytes[offset + 3U]);
        std::uint8_t first = 0U;
        std::uint8_t second = 0U;
        if (*source_endian == ByteOrder::little) {
            first = static_cast<std::uint8_t>(packed & 0xffU);
            second = static_cast<std::uint8_t>(packed >> 8U);
        } else {
            first = static_cast<std::uint8_t>(packed >> 8U);
            second = static_cast<std::uint8_t>(packed & 0xffU);
        }
        const std::uint16_t interpreted = *interpret_endian == ByteOrder::little
            ? static_cast<std::uint16_t>(static_cast<std::uint16_t>(first) | (static_cast<std::uint16_t>(second) << 8U))
            : static_cast<std::uint16_t>((static_cast<std::uint16_t>(first) << 8U) | static_cast<std::uint16_t>(second));
        decode_pixel(*interpret_format, interpreted, output.bytes.data() + offset);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_planar_layout(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view mode;
    if (auto error = get_text(instance, "mode", mode); error.has_value()) {
        return error;
    }
    if (mode != "interleaved-as-planar" && mode != "planar-as-interleaved") {
        return "mode must be interleaved-as-planar or planar-as-interleaved";
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    if (mode == "interleaved-as-planar") {
        for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
            for (std::size_t channel = 0U; channel < 4U; ++channel) {
                output.bytes[pixel * 4U + channel] = input.bytes[channel * pixels + pixel];
            }
        }
    } else {
        for (std::size_t output_index = 0U; output_index < output.bytes.size(); ++output_index) {
            const std::size_t plane = output_index / pixels;
            const std::size_t pixel = output_index % pixels;
            output.bytes[output_index] = input.bytes[pixel * 4U + plane];
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_signed_byte(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    std::string_view mode;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    if (auto error = get_text(instance, "mode", mode); error.has_value()) return error;
    if (mode != "bias-flip" && mode != "absolute-signed" && mode != "clamp-negative") {
        return "mode must be bias-flip, absolute-signed, or clamp-negative";
    }
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    output = input;
    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[channel]) continue;
            const std::uint8_t value = input.bytes[offset + channel];
            if (mode == "bias-flip") {
                output.bytes[offset + channel] = static_cast<std::uint8_t>(value ^ 0x80U);
                continue;
            }
            const std::int16_t signed_value = value < 128U
                ? static_cast<std::int16_t>(value)
                : static_cast<std::int16_t>(static_cast<std::int16_t>(value) - 256);
            if (mode == "absolute-signed") {
                const std::int16_t magnitude = signed_value < 0 ? static_cast<std::int16_t>(-signed_value) : signed_value;
                output.bytes[offset + channel] = static_cast<std::uint8_t>(magnitude);
            } else {
                output.bytes[offset + channel] = signed_value < 0 ? 0U : static_cast<std::uint8_t>(signed_value);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_bit_shift(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    std::string_view direction;
    std::uint64_t amount = 0U;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    if (auto error = get_text(instance, "direction", direction); error.has_value()) return error;
    if (auto error = get_u64(instance, "amount", amount); error.has_value()) return error;
    if (direction != "left" && direction != "right") return "direction must be left or right";
    if (amount > 8U) return "amount must be in the inclusive range 0..8";
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    output = input;
    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[channel]) continue;
            if (amount == 8U) {
                output.bytes[offset + channel] = 0U;
            } else if (direction == "left") {
                const std::uint16_t shifted = static_cast<std::uint16_t>(input.bytes[offset + channel]) << static_cast<unsigned int>(amount);
                output.bytes[offset + channel] = static_cast<std::uint8_t>(shifted & 0xffU);
            } else {
                output.bytes[offset + channel] = static_cast<std::uint8_t>(input.bytes[offset + channel] >> static_cast<unsigned int>(amount));
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_nibble_swap(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    output = input;
    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[channel]) continue;
            const std::uint8_t value = input.bytes[offset + channel];
            output.bytes[offset + channel] = static_cast<std::uint8_t>((value << 4U) | (value >> 4U));
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_bitplane_swap(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    std::uint64_t plane_a = 0U;
    std::uint64_t plane_b = 0U;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    if (auto error = get_u64(instance, "plane_a", plane_a); error.has_value()) return error;
    if (auto error = get_u64(instance, "plane_b", plane_b); error.has_value()) return error;
    if (plane_a > 7U || plane_b > 7U) return "bitplane indices must be in the inclusive range 0..7";
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    output = input;
    const std::uint8_t mask_a = static_cast<std::uint8_t>(1U << static_cast<unsigned int>(plane_a));
    const std::uint8_t mask_b = static_cast<std::uint8_t>(1U << static_cast<unsigned int>(plane_b));
    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[channel]) continue;
            std::uint8_t value = input.bytes[offset + channel];
            const bool bit_a = (value & mask_a) != 0U;
            const bool bit_b = (value & mask_b) != 0U;
            if (bit_a != bit_b) value = static_cast<std::uint8_t>(value ^ mask_a ^ mask_b);
            output.bytes[offset + channel] = value;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_stuck_bits(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view channel_text;
    std::uint64_t zero_mask = 0U;
    std::uint64_t one_mask = 0U;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    if (auto error = get_u64(instance, "zero_mask", zero_mask); error.has_value()) return error;
    if (auto error = get_u64(instance, "one_mask", one_mask); error.has_value()) return error;
    if (zero_mask > 0xffU || one_mask > 0xffU) return "stuck-bit masks must be in the inclusive byte range 0..255";
    if ((zero_mask & one_mask) != 0U) return "zero_mask and one_mask must not overlap";
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    output = input;
    const std::uint8_t zero = static_cast<std::uint8_t>(zero_mask);
    const std::uint8_t one = static_cast<std::uint8_t>(one_mask);
    const std::uint8_t keep = static_cast<std::uint8_t>(~zero);
    const std::size_t pixels = static_cast<std::size_t>(pixel_count(input));
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[channel]) continue;
            output.bytes[offset + channel] = static_cast<std::uint8_t>((input.bytes[offset + channel] & keep) | one);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::uint64_t ceil_div_u64(const std::uint64_t value, const std::uint64_t divisor) noexcept {
    return value / divisor + (value % divisor == 0U ? 0U : 1U);
}

[[nodiscard]] std::optional<std::string> execute_bit_burst(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::uint64_t block_width = 0U;
    std::uint64_t block_height = 0U;
    std::uint64_t burst_count = 0U;
    std::uint64_t xor_mask = 0U;
    std::string_view channel_text;
    if (auto error = get_u64(instance, "block_width", block_width); error.has_value()) return error;
    if (auto error = get_u64(instance, "block_height", block_height); error.has_value()) return error;
    if (auto error = get_u64(instance, "burst_count", burst_count); error.has_value()) return error;
    if (auto error = get_u64(instance, "xor_mask", xor_mask); error.has_value()) return error;
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) return error;
    if (block_width == 0U || block_height == 0U ||
        block_width > std::numeric_limits<std::uint32_t>::max() ||
        block_height > std::numeric_limits<std::uint32_t>::max()) {
        return "block dimensions must be in the inclusive range 1..UINT32_MAX";
    }
    if (burst_count > 65536U) return "burst_count exceeds the canonical 65536-burst work cap";
    if (xor_mask > 0xffU) return "xor_mask must be in the inclusive byte range 0..255";
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) return error;

    const std::uint64_t visit_width = std::min<std::uint64_t>(block_width, input.width);
    const std::uint64_t visit_height = std::min<std::uint64_t>(block_height, input.height);
    const std::uint64_t pixels_per_burst = visit_width * visit_height;
    constexpr std::uint64_t kMaximumPixelVisits = 64ULL * 1024ULL * 1024ULL;
    if (burst_count != 0U && pixels_per_burst > kMaximumPixelVisits / burst_count) {
        return "bit-burst parameters exceed the canonical 64M pixel-visit work cap";
    }

    output = input;
    if (burst_count == 0U || xor_mask == 0U) return std::nullopt;
    const std::uint64_t grid_width = ceil_div_u64(input.width, block_width);
    const std::uint64_t grid_height = ceil_div_u64(input.height, block_height);
    const std::uint64_t block_count = grid_width * grid_height;
    const std::uint8_t mask = static_cast<std::uint8_t>(xor_mask);

    for (std::uint64_t burst = 0U; burst < burst_count; ++burst) {
        const std::array<std::uint64_t, 5> identity{
            burst, grid_width, grid_height, block_width, block_height};
        auto stream = make_named_stream(
            root_seed,
            instance.instance_id,
            "bit-burst-block",
            identity);
        const std::uint64_t block = stream.uniform_below(block_count);
        const std::uint64_t block_x = (block % grid_width) * block_width;
        const std::uint64_t block_y = (block / grid_width) * block_height;
        const std::uint64_t end_x = std::min<std::uint64_t>(input.width, block_x + block_width);
        const std::uint64_t end_y = std::min<std::uint64_t>(input.height, block_y + block_height);
        for (std::uint64_t y = block_y; y < end_y; ++y) {
            for (std::uint64_t x = block_x; x < end_x; ++x) {
                const std::size_t offset = static_cast<std::size_t>((y * input.width + x) * 4U);
                for (std::size_t channel = 0U; channel < 4U; ++channel) {
                    if (channels[channel]) output.bytes[offset + channel] ^= mask;
                }
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] MutationMetadata signed_range_metadata(
    const std::int64_t minimum,
    const std::int64_t maximum,
    const std::int64_t step) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::signed_range;
    metadata.signed_min = minimum;
    metadata.signed_max = maximum;
    metadata.signed_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata unsigned_range_metadata(
    const std::uint64_t minimum,
    const std::uint64_t maximum,
    const std::uint64_t step) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::unsigned_range;
    metadata.unsigned_min = minimum;
    metadata.unsigned_max = maximum;
    metadata.unsigned_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata choice_metadata(std::vector<std::string> choices) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::choice;
    metadata.choices = std::move(choices);
    return metadata;
}

[[nodiscard]] MutationMetadata bitmask_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::bitmask;
    return metadata;
}

[[nodiscard]] ParameterDescriptor parameter(
    const char* name,
    const ParameterKind kind,
    MutationMetadata mutation) {
    return ParameterDescriptor{name, kind, true, std::move(mutation)};
}

[[nodiscard]] ParameterDescriptor channels_parameter() {
    return parameter(
        "channels",
        ParameterKind::text,
        choice_metadata({"r", "g", "b", "a", "rgb", "rgba", "rb", "gb", "ra", "ba"}));
}

[[nodiscard]] ParameterDescriptor boundary_parameter() {
    return parameter(
        "boundary",
        ParameterKind::text,
        choice_metadata({"wrap", "clamp", "fill"}));
}

void register_required(
    FaultRegistry& registry,
    OperatorDescriptor descriptor,
    const OperatorExecutor executor) {
    std::string error;
    if (!registry.register_operator(std::move(descriptor), executor, &error)) {
        throw std::runtime_error("failed to register representation/bit fault operator: " + error);
    }
}

}  // namespace

void register_representation_bit_faults(FaultRegistry& registry) {
    register_required(
        registry,
        OperatorDescriptor{
            kFaultChannelRoute, 1U, 1U,
            {parameter(
                "routes", ParameterKind::text,
                choice_metadata({
                    "rgba", "rgab", "rbga", "rbag", "ragb", "rabg",
                    "grba", "grab", "gbra", "gbar", "garb", "gabr",
                    "brga", "brag", "bgra", "bgar", "barg", "bagr",
                    "argb", "arbg", "agrb", "agbr", "abrg", "abgr",
                    "rrra", "ggga", "bbba", "aaa1", "rr01", "rgb1"}))}},
        execute_channel_route);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultChannelOffset, 1U, 1U,
            {
                channels_parameter(),
                parameter("dx", ParameterKind::signed_integer, signed_range_metadata(-4096, 4096, 1)),
                parameter("dy", ParameterKind::signed_integer, signed_range_metadata(-4096, 4096, 1)),
                boundary_parameter(),
            }},
        execute_channel_offset);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultWordLanes, 1U, 1U,
            {
                parameter("word_bytes", ParameterKind::unsigned_integer, unsigned_range_metadata(2U, 4U, 2U)),
                parameter("mode", ParameterKind::text, choice_metadata({"reverse", "rotate-left", "rotate-right"})),
            }},
        execute_word_lanes);

    const std::vector<std::string> packed_formats{"rgb565", "bgr565", "rgba4444", "argb1555"};
    const std::vector<std::string> endianness{"little", "big"};
    register_required(
        registry,
        OperatorDescriptor{
            kFaultPackedReinterpret, 1U, 1U,
            {
                parameter("source_format", ParameterKind::text, choice_metadata(packed_formats)),
                parameter("source_endian", ParameterKind::text, choice_metadata(endianness)),
                parameter("interpret_format", ParameterKind::text, choice_metadata(packed_formats)),
                parameter("interpret_endian", ParameterKind::text, choice_metadata(endianness)),
            }},
        execute_packed_reinterpret);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultPlanarLayout, 1U, 1U,
            {parameter(
                "mode", ParameterKind::text,
                choice_metadata({"interleaved-as-planar", "planar-as-interleaved"}))}},
        execute_planar_layout);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultSignedByte, 1U, 1U,
            {
                channels_parameter(),
                parameter(
                    "mode", ParameterKind::text,
                    choice_metadata({"bias-flip", "absolute-signed", "clamp-negative"})),
            }},
        execute_signed_byte);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultBitShift, 1U, 1U,
            {
                channels_parameter(),
                parameter("direction", ParameterKind::text, choice_metadata({"left", "right"})),
                parameter("amount", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 8U, 1U)),
            }},
        execute_bit_shift);

    register_required(
        registry,
        OperatorDescriptor{kFaultNibbleSwap, 1U, 1U, {channels_parameter()}},
        execute_nibble_swap);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultBitplaneSwap, 1U, 1U,
            {
                channels_parameter(),
                parameter("plane_a", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 7U, 1U)),
                parameter("plane_b", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 7U, 1U)),
            }},
        execute_bitplane_swap);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultStuckBits, 1U, 1U,
            {
                channels_parameter(),
                parameter("zero_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                parameter("one_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
            }},
        execute_stuck_bits);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultBitBurst, 1U, 1U,
            {
                parameter("block_width", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 1024U, 1U)),
                parameter("block_height", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 1024U, 1U)),
                parameter("burst_count", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 4096U, 1U)),
                parameter("xor_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                channels_parameter(),
            }},
        execute_bit_burst);
}

}  // namespace faultmine::core
