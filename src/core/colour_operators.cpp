#include "faultmine/colour_operators.hpp"

#include "faultmine/colour.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::core {
namespace {

[[nodiscard]] const ParameterValue* find_parameter(
    const OperatorInstance& instance,
    const std::string_view name) noexcept {
    const auto iterator = instance.parameters.find(name);
    return iterator == instance.parameters.end() ? nullptr : &iterator->second;
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
        return "failed to allocate canonical colour operator output";
    }
    output = std::move(*created.image);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> parse_channels(
    const std::string_view text,
    std::array<bool, 4U>& selected) {
    selected = {false, false, false, false};
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
        if (selected[index]) {
            return "channels must not contain duplicates";
        }
        selected[index] = true;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> parse_alpha_mode(
    const OperatorInstance& instance,
    bool& map_alpha) {
    std::string_view mode;
    if (auto error = get_text(instance, "alpha_mode", mode); error.has_value()) {
        return error;
    }
    if (mode == "preserve") {
        map_alpha = false;
        return std::nullopt;
    }
    if (mode == "map") {
        map_alpha = true;
        return std::nullopt;
    }
    return "alpha_mode must be preserve or map";
}

[[nodiscard]] std::optional<std::string> parse_palette_parameter(
    const OperatorInstance& instance,
    Palette& palette) {
    std::string_view text;
    if (auto error = get_text(instance, "palette", text); error.has_value()) {
        return error;
    }
    const PaletteParseResult parsed = parse_palette(text);
    if (!parsed.ok()) {
        return "invalid embedded palette: " + (parsed.error.has_value() ? parsed.error->message : std::string{"unknown parse failure"});
    }
    palette = *parsed.palette;
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> parse_lut_parameter(
    const OperatorInstance& instance,
    Lut256& lut) {
    std::string_view text;
    if (auto error = get_text(instance, "lut", text); error.has_value()) {
        return error;
    }
    const LutParseResult parsed = parse_lut(text);
    if (!parsed.ok()) {
        return "invalid embedded LUT: " + (parsed.error.has_value() ? parsed.error->message : std::string{"unknown parse failure"});
    }
    lut = *parsed.lut;
    return std::nullopt;
}

[[nodiscard]] std::uint32_t square_difference(
    const std::uint8_t left,
    const std::uint8_t right) noexcept {
    const std::int32_t delta = static_cast<std::int32_t>(left) - static_cast<std::int32_t>(right);
    return static_cast<std::uint32_t>(delta * delta);
}

[[nodiscard]] Rgba8 nearest_colour(
    const Rgba8 input,
    const Palette& palette,
    const bool map_alpha) noexcept {
    std::size_t best_index = 0U;
    std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t index = 0U; index < palette.entries.size(); ++index) {
        const Rgba8 candidate = palette.entries[index];
        std::uint32_t distance =
            square_difference(input.r, candidate.r) +
            square_difference(input.g, candidate.g) +
            square_difference(input.b, candidate.b);
        if (map_alpha) {
            distance += square_difference(input.a, candidate.a);
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
        }
    }
    Rgba8 output = palette.entries[best_index];
    if (!map_alpha) {
        output.a = input.a;
    }
    return output;
}

[[nodiscard]] std::uint8_t luminance_u8(const Rgba8 input) noexcept {
    const std::uint32_t weighted =
        54U * static_cast<std::uint32_t>(input.r) +
        183U * static_cast<std::uint32_t>(input.g) +
        19U * static_cast<std::uint32_t>(input.b) +
        128U;
    return static_cast<std::uint8_t>(weighted >> 8U);
}

[[nodiscard]] std::uint8_t interpolate_255(
    const std::uint8_t left,
    const std::uint8_t right,
    const std::uint32_t remainder) noexcept {
    const std::uint32_t value =
        static_cast<std::uint32_t>(left) * (255U - remainder) +
        static_cast<std::uint32_t>(right) * remainder +
        127U;
    return static_cast<std::uint8_t>(value / 255U);
}

[[nodiscard]] Rgba8 gradient_colour(
    const Rgba8 input,
    const Palette& palette,
    const bool map_alpha) noexcept {
    if (palette.entries.size() == 1U) {
        Rgba8 output = palette.entries.front();
        if (!map_alpha) {
            output.a = input.a;
        }
        return output;
    }

    const std::uint32_t luminance = luminance_u8(input);
    const std::uint32_t segment_count = static_cast<std::uint32_t>(palette.entries.size() - 1U);
    const std::uint32_t scaled = luminance * segment_count;
    const std::uint32_t segment = scaled / 255U;
    const std::uint32_t remainder = scaled % 255U;
    if (segment >= segment_count) {
        Rgba8 output = palette.entries.back();
        if (!map_alpha) {
            output.a = input.a;
        }
        return output;
    }

    const Rgba8 left = palette.entries[segment];
    const Rgba8 right = palette.entries[segment + 1U];
    Rgba8 output{
        interpolate_255(left.r, right.r, remainder),
        interpolate_255(left.g, right.g, remainder),
        interpolate_255(left.b, right.b, remainder),
        interpolate_255(left.a, right.a, remainder)};
    if (!map_alpha) {
        output.a = input.a;
    }
    return output;
}

void write_pixel(ImageBuffer& image, const std::size_t offset, const Rgba8 colour) {
    image.bytes[offset] = colour.r;
    image.bytes[offset + 1U] = colour.g;
    image.bytes[offset + 2U] = colour.b;
    image.bytes[offset + 3U] = colour.a;
}

[[nodiscard]] Rgba8 read_pixel(const ImageBuffer& image, const std::size_t offset) noexcept {
    return Rgba8{
        image.bytes[offset],
        image.bytes[offset + 1U],
        image.bytes[offset + 2U],
        image.bytes[offset + 3U]};
}

[[nodiscard]] std::optional<std::string> execute_palette_nearest(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    Palette palette;
    bool map_alpha = false;
    if (auto error = parse_palette_parameter(instance, palette); error.has_value()) {
        return error;
    }
    if (auto error = parse_alpha_mode(instance, map_alpha); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }
    for (std::size_t offset = 0U; offset < input.bytes.size(); offset += 4U) {
        write_pixel(output, offset, nearest_colour(read_pixel(input, offset), palette, map_alpha));
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_gradient_map(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    Palette palette;
    bool map_alpha = false;
    if (auto error = parse_palette_parameter(instance, palette); error.has_value()) {
        return error;
    }
    if (auto error = parse_alpha_mode(instance, map_alpha); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }
    for (std::size_t offset = 0U; offset < input.bytes.size(); offset += 4U) {
        write_pixel(output, offset, gradient_colour(read_pixel(input, offset), palette, map_alpha));
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_lut(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    Lut256 lut;
    if (auto error = parse_lut_parameter(instance, lut); error.has_value()) {
        return error;
    }
    output = input;
    for (std::size_t offset = 0U; offset < output.bytes.size(); offset += 4U) {
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            output.bytes[offset + channel] = lut.channels[channel][input.bytes[offset + channel]];
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::uint32_t quantization_index(
    const std::uint8_t value,
    const std::uint32_t levels) noexcept {
    const std::uint32_t maximum = levels - 1U;
    return (static_cast<std::uint32_t>(value) * maximum + 127U) / 255U;
}

[[nodiscard]] std::uint8_t quantized_value(
    const std::uint32_t index,
    const std::uint32_t levels) noexcept {
    const std::uint32_t maximum = levels - 1U;
    return static_cast<std::uint8_t>((index * 255U + maximum / 2U) / maximum);
}

[[nodiscard]] std::optional<std::string> parse_quantizer(
    const OperatorInstance& instance,
    std::uint32_t& levels,
    std::array<bool, 4U>& channels) {
    std::uint64_t requested_levels = 0U;
    std::string_view channel_text;
    if (auto error = get_u64(instance, "levels", requested_levels); error.has_value()) {
        return error;
    }
    if (requested_levels < 2U || requested_levels > 256U) {
        return "levels must be in the inclusive range 2..256";
    }
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) {
        return error;
    }
    if (auto error = parse_channels(channel_text, channels); error.has_value()) {
        return error;
    }
    levels = static_cast<std::uint32_t>(requested_levels);
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_quantize(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint32_t levels = 0U;
    std::array<bool, 4U> channels{};
    if (auto error = parse_quantizer(instance, levels, channels); error.has_value()) {
        return error;
    }
    output = input;
    for (std::size_t offset = 0U; offset < output.bytes.size(); offset += 4U) {
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (channels[channel]) {
                const std::uint32_t index = quantization_index(input.bytes[offset + channel], levels);
                output.bytes[offset + channel] = quantized_value(index, levels);
            }
        }
    }
    return std::nullopt;
}

constexpr std::array<std::uint8_t, 4U> kBayer2{
    0U, 2U,
    3U, 1U};

constexpr std::array<std::uint8_t, 16U> kBayer4{
    0U, 8U, 2U, 10U,
    12U, 4U, 14U, 6U,
    3U, 11U, 1U, 9U,
    15U, 7U, 13U, 5U};

constexpr std::array<std::uint8_t, 64U> kBayer8{
    0U, 32U, 8U, 40U, 2U, 34U, 10U, 42U,
    48U, 16U, 56U, 24U, 50U, 18U, 58U, 26U,
    12U, 44U, 4U, 36U, 14U, 46U, 6U, 38U,
    60U, 28U, 52U, 20U, 62U, 30U, 54U, 22U,
    3U, 35U, 11U, 43U, 1U, 33U, 9U, 41U,
    51U, 19U, 59U, 27U, 49U, 17U, 57U, 25U,
    15U, 47U, 7U, 39U, 13U, 45U, 5U, 37U,
    63U, 31U, 55U, 23U, 61U, 29U, 53U, 21U};

[[nodiscard]] std::optional<std::uint32_t> bayer_size(const std::string_view matrix) noexcept {
    if (matrix == "bayer2") {
        return 2U;
    }
    if (matrix == "bayer4") {
        return 4U;
    }
    if (matrix == "bayer8") {
        return 8U;
    }
    return std::nullopt;
}

[[nodiscard]] std::uint32_t bayer_value(
    const std::uint32_t size,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    const std::size_t index = static_cast<std::size_t>(y % size) * size + (x % size);
    if (size == 2U) {
        return kBayer2[index];
    }
    if (size == 4U) {
        return kBayer4[index];
    }
    return kBayer8[index];
}

[[nodiscard]] std::uint32_t dithered_index(
    const std::uint8_t value,
    const std::uint32_t levels,
    const std::uint32_t threshold) noexcept {
    const std::uint32_t maximum = levels - 1U;
    const std::uint32_t scaled = static_cast<std::uint32_t>(value) * maximum;
    std::uint32_t index = scaled / 255U;
    const std::uint32_t remainder = scaled % 255U;
    if (remainder > threshold && index < maximum) {
        ++index;
    }
    return index;
}

[[nodiscard]] std::optional<std::string> execute_dither_ordered(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint32_t levels = 0U;
    std::array<bool, 4U> channels{};
    std::string_view matrix_text;
    if (auto error = parse_quantizer(instance, levels, channels); error.has_value()) {
        return error;
    }
    if (auto error = get_text(instance, "matrix", matrix_text); error.has_value()) {
        return error;
    }
    const auto matrix_size = bayer_size(matrix_text);
    if (!matrix_size.has_value()) {
        return "matrix must be bayer2, bayer4, or bayer8";
    }

    output = input;
    const std::uint32_t cells = *matrix_size * *matrix_size;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * input.width + x) * 4U;
            const std::uint32_t rank = bayer_value(*matrix_size, x, y);
            const std::uint32_t threshold = ((rank * 2U + 1U) * 255U) / (cells * 2U);
            for (std::size_t channel = 0U; channel < 4U; ++channel) {
                if (channels[channel]) {
                    output.bytes[offset + channel] = quantized_value(
                        dithered_index(input.bytes[offset + channel], levels, threshold),
                        levels);
                }
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_dither_noise(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::uint32_t levels = 0U;
    std::array<bool, 4U> channels{};
    if (auto error = parse_quantizer(instance, levels, channels); error.has_value()) {
        return error;
    }

    output = input;
    const std::uint64_t pixel_count = static_cast<std::uint64_t>(input.width) * input.height;
    for (std::uint64_t pixel = 0U; pixel < pixel_count; ++pixel) {
        const std::size_t offset = static_cast<std::size_t>(pixel * 4U);
        for (std::uint64_t channel = 0U; channel < 4U; ++channel) {
            if (!channels[static_cast<std::size_t>(channel)]) {
                continue;
            }
            const std::array<std::uint64_t, 3U> identity{
                pixel,
                channel,
                static_cast<std::uint64_t>(levels)};
            auto stream = make_named_stream(root_seed, instance.instance_id, "colour-noise-dither", identity);
            const std::uint32_t threshold = static_cast<std::uint32_t>(stream.uniform_below(255U));
            output.bytes[offset + static_cast<std::size_t>(channel)] = quantized_value(
                dithered_index(input.bytes[offset + static_cast<std::size_t>(channel)], levels, threshold),
                levels);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_generated_palette_map(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::string_view start_text;
    std::string_view end_text;
    std::uint64_t count = 0U;
    std::uint64_t jitter = 0U;
    bool map_alpha = false;
    if (auto error = get_text(instance, "start", start_text); error.has_value()) {
        return error;
    }
    if (auto error = get_text(instance, "end", end_text); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "count", count); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "jitter", jitter); error.has_value()) {
        return error;
    }
    if (auto error = parse_alpha_mode(instance, map_alpha); error.has_value()) {
        return error;
    }
    const auto start = parse_rgba8_hex(start_text);
    const auto end = parse_rgba8_hex(end_text);
    if (!start.has_value() || !end.has_value()) {
        return "start and end must be exactly rrggbbaa hexadecimal";
    }
    if (count == 0U || count > kMaximumPaletteEntries) {
        return "count must be in the inclusive range 1..256";
    }
    if (jitter > 255U) {
        return "jitter must be in the inclusive range 0..255";
    }
    PaletteGenerationSpec spec{
        *start,
        *end,
        static_cast<std::uint32_t>(count),
        static_cast<std::uint8_t>(jitter)};
    std::string generation_error;
    const auto palette = generate_palette_ramp(spec, root_seed, instance.instance_id, &generation_error);
    if (!palette.has_value()) {
        return generation_error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }
    for (std::size_t offset = 0U; offset < input.bytes.size(); offset += 4U) {
        write_pixel(output, offset, nearest_colour(read_pixel(input, offset), *palette, map_alpha));
    }
    return std::nullopt;
}

[[nodiscard]] MutationMetadata choice_metadata(std::vector<std::string> choices) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::choice;
    metadata.choices = std::move(choices);
    return metadata;
}

[[nodiscard]] MutationMetadata unsigned_range_metadata(
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

[[nodiscard]] MutationMetadata palette_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::palette;
    return metadata;
}

[[nodiscard]] MutationMetadata lut_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::lut;
    return metadata;
}

[[nodiscard]] MutationMetadata colour_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::colour_rgba;
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
        choice_metadata({"rgb", "rgba", "r", "g", "b", "a", "rg", "rb", "gb"}));
}

[[nodiscard]] ParameterDescriptor alpha_mode_parameter() {
    return parameter("alpha_mode", ParameterKind::text, choice_metadata({"preserve", "map"}));
}

void register_required(
    FaultRegistry& registry,
    OperatorDescriptor descriptor,
    const OperatorExecutor executor) {
    std::string error;
    if (!registry.register_operator(std::move(descriptor), executor, &error)) {
        throw std::runtime_error("failed to register colour fault operator: " + error);
    }
}

}  // namespace

void register_colour_faults(FaultRegistry& registry) {
    register_required(
        registry,
        OperatorDescriptor{
            kColourPaletteNearest,
            1U,
            1U,
            {
                parameter("palette", ParameterKind::text, palette_metadata()),
                alpha_mode_parameter(),
            }},
        execute_palette_nearest);

    register_required(
        registry,
        OperatorDescriptor{
            kColourGradientMap,
            1U,
            1U,
            {
                parameter("palette", ParameterKind::text, palette_metadata()),
                alpha_mode_parameter(),
            }},
        execute_gradient_map);

    register_required(
        registry,
        OperatorDescriptor{
            kColourLut,
            1U,
            1U,
            {parameter("lut", ParameterKind::text, lut_metadata())}},
        execute_lut);

    register_required(
        registry,
        OperatorDescriptor{
            kColourQuantize,
            1U,
            1U,
            {
                parameter("levels", ParameterKind::unsigned_integer, unsigned_range_metadata(2U, 256U)),
                channels_parameter(),
            }},
        execute_quantize);

    register_required(
        registry,
        OperatorDescriptor{
            kColourDitherOrdered,
            1U,
            1U,
            {
                parameter("levels", ParameterKind::unsigned_integer, unsigned_range_metadata(2U, 256U)),
                channels_parameter(),
                parameter("matrix", ParameterKind::text, choice_metadata({"bayer2", "bayer4", "bayer8"})),
            }},
        execute_dither_ordered);

    register_required(
        registry,
        OperatorDescriptor{
            kColourDitherNoise,
            1U,
            1U,
            {
                parameter("levels", ParameterKind::unsigned_integer, unsigned_range_metadata(2U, 256U)),
                channels_parameter(),
            }},
        execute_dither_noise);

    register_required(
        registry,
        OperatorDescriptor{
            kColourGeneratedPaletteMap,
            1U,
            1U,
            {
                parameter("start", ParameterKind::text, colour_metadata()),
                parameter("end", ParameterKind::text, colour_metadata()),
                parameter("count", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 256U)),
                parameter("jitter", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 255U)),
                alpha_mode_parameter(),
            }},
        execute_generated_palette_map);
}

}  // namespace faultmine::core
