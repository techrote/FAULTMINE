#include "faultmine/memory_addressing_operators.hpp"

#include "faultmine/logical_address.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

namespace faultmine::core {
namespace {

[[nodiscard]] const ParameterValue* find_parameter(
    const OperatorInstance& instance,
    const std::string_view name) noexcept {
    const auto iterator = instance.parameters.find(name);
    return iterator == instance.parameters.end() ? nullptr : &iterator->second;
}

[[nodiscard]] std::optional<std::string> get_bool(
    const OperatorInstance& instance,
    const std::string_view name,
    bool& output) {
    const ParameterValue* value = find_parameter(instance, name);
    if (value == nullptr) {
        return "missing boolean parameter '" + std::string{name} + "'";
    }
    const auto* typed = std::get_if<bool>(value);
    if (typed == nullptr) {
        return "parameter '" + std::string{name} + "' must be bool";
    }
    output = *typed;
    return std::nullopt;
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

void copy_pixel(
    const ImageBuffer& input,
    const std::uint64_t source_pixel,
    ImageBuffer& output,
    const std::uint64_t destination_pixel) {
    const std::size_t source_offset = static_cast<std::size_t>(source_pixel * 4U);
    const std::size_t destination_offset = static_cast<std::size_t>(destination_pixel * 4U);
    for (std::size_t channel = 0U; channel < 4U; ++channel) {
        output.bytes[destination_offset + channel] = input.bytes[source_offset + channel];
    }
}

[[nodiscard]] std::uint64_t pixel_count(const ImageBuffer& image) noexcept {
    return static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height);
}

[[nodiscard]] std::uint64_t ceil_div_u64(const std::uint64_t value, const std::uint64_t divisor) noexcept {
    return value / divisor + (value % divisor == 0U ? 0U : 1U);
}

[[nodiscard]] std::uint64_t add_mod(
    const std::uint64_t left,
    const std::uint64_t right,
    const std::uint64_t modulus) noexcept {
    if (left >= modulus - right) {
        return left - (modulus - right);
    }
    return left + right;
}

[[nodiscard]] std::uint64_t multiply_mod(
    std::uint64_t left,
    std::uint64_t right,
    const std::uint64_t modulus) noexcept {
    if (modulus == 1U) {
        return 0U;
    }
    left %= modulus;
    std::uint64_t result = 0U;
    while (right != 0U) {
        if ((right & 1U) != 0U) {
            result = add_mod(result, left, modulus);
        }
        right >>= 1U;
        if (right != 0U) {
            left = add_mod(left, left, modulus);
        }
    }
    return result;
}

[[nodiscard]] std::optional<std::string> execute_address_offset(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::int64_t offset = 0;
    BoundaryPolicy boundary{};
    if (auto error = get_i64(instance, "offset_pixels", offset); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::uint64_t count = pixel_count(input);
    for (std::uint64_t destination = 0U; destination < count; ++destination) {
        const auto source = resolve_displaced_index(destination, offset, count, boundary);
        if (source.has_value()) {
            copy_pixel(input, *source, output, destination);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_address_mask(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t xor_mask = 0U;
    std::uint64_t and_mask = 0U;
    std::uint64_t or_mask = 0U;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "xor_mask", xor_mask); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "and_mask", and_mask); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "or_mask", or_mask); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::uint64_t count = pixel_count(input);
    for (std::uint64_t destination = 0U; destination < count; ++destination) {
        const std::uint64_t logical = ((destination ^ xor_mask) & and_mask) | or_mask;
        const auto source = resolve_logical_index(logical, count, boundary);
        if (source.has_value()) {
            copy_pixel(input, *source, output, destination);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_coordinate_remap(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::int64_t x_offset = 0;
    std::int64_t y_offset = 0;
    std::uint64_t x_xor_mask = 0U;
    std::uint64_t y_xor_mask = 0U;
    bool swap_xy = false;
    BoundaryPolicy boundary{};
    if (auto error = get_i64(instance, "x_offset", x_offset); error.has_value()) {
        return error;
    }
    if (auto error = get_i64(instance, "y_offset", y_offset); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "x_xor_mask", x_xor_mask); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "y_xor_mask", y_xor_mask); error.has_value()) {
        return error;
    }
    if (x_xor_mask > std::numeric_limits<std::uint32_t>::max() ||
        y_xor_mask > std::numeric_limits<std::uint32_t>::max()) {
        return "coordinate XOR masks must fit uint32";
    }
    if (auto error = get_bool(instance, "swap_xy", swap_xy); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            std::uint64_t base_x = swap_xy ? static_cast<std::uint64_t>(y) : static_cast<std::uint64_t>(x);
            std::uint64_t base_y = swap_xy ? static_cast<std::uint64_t>(x) : static_cast<std::uint64_t>(y);
            base_x ^= x_xor_mask;
            base_y ^= y_xor_mask;
            const auto source_x = resolve_displaced_index(base_x, x_offset, input.width, boundary);
            const auto source_y = resolve_displaced_index(base_y, y_offset, input.height, boundary);
            if (source_x.has_value() && source_y.has_value()) {
                const std::uint64_t source = *source_y * input.width + *source_x;
                const std::uint64_t destination = static_cast<std::uint64_t>(y) * input.width + x;
                copy_pixel(input, source, output, destination);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_tile_permute(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::uint64_t tile_width = 0U;
    std::uint64_t tile_height = 0U;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "tile_width", tile_width); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "tile_height", tile_height); error.has_value()) {
        return error;
    }
    if (tile_width == 0U || tile_height == 0U ||
        tile_width > std::numeric_limits<std::uint32_t>::max() ||
        tile_height > std::numeric_limits<std::uint32_t>::max()) {
        return "tile dimensions must be in the inclusive range 1..UINT32_MAX";
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::uint64_t tiles_x = ceil_div_u64(input.width, tile_width);
    const std::uint64_t tiles_y = ceil_div_u64(input.height, tile_height);
    const std::uint64_t tile_count = tiles_x * tiles_y;
    if (tile_count <= 1U) {
        output = input;
        return std::nullopt;
    }

    const std::array<std::uint64_t, 4> identity{tile_width, tile_height, tiles_x, tiles_y};
    auto stream = make_named_stream(root_seed, instance.instance_id, "tile-affine-permutation", identity);
    std::uint64_t multiplier = stream.uniform_below(tile_count);
    if (multiplier == 0U) {
        multiplier = 1U;
    }
    while (std::gcd(multiplier, tile_count) != 1U) {
        ++multiplier;
        if (multiplier == tile_count) {
            multiplier = 1U;
        }
    }
    const std::uint64_t addend = stream.uniform_below(tile_count);

    for (std::uint64_t destination_tile = 0U; destination_tile < tile_count; ++destination_tile) {
        const std::uint64_t source_tile =
            add_mod(multiply_mod(destination_tile, multiplier, tile_count), addend, tile_count);
        const std::uint64_t destination_tile_x = destination_tile % tiles_x;
        const std::uint64_t destination_tile_y = destination_tile / tiles_x;
        const std::uint64_t source_tile_x = source_tile % tiles_x;
        const std::uint64_t source_tile_y = source_tile / tiles_x;

        for (std::uint64_t local_y = 0U; local_y < tile_height; ++local_y) {
            const std::uint64_t destination_y = destination_tile_y * tile_height + local_y;
            if (destination_y >= input.height) {
                break;
            }
            for (std::uint64_t local_x = 0U; local_x < tile_width; ++local_x) {
                const std::uint64_t destination_x = destination_tile_x * tile_width + local_x;
                if (destination_x >= input.width) {
                    break;
                }
                const std::uint64_t logical_source_x = source_tile_x * tile_width + local_x;
                const std::uint64_t logical_source_y = source_tile_y * tile_height + local_y;
                const auto source_x = resolve_logical_index(logical_source_x, input.width, boundary);
                const auto source_y = resolve_logical_index(logical_source_y, input.height, boundary);
                if (source_x.has_value() && source_y.has_value()) {
                    const std::uint64_t source = *source_y * input.width + *source_x;
                    const std::uint64_t destination = destination_y * input.width + destination_x;
                    copy_pixel(input, source, output, destination);
                }
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_band_repeat(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t band_height = 0U;
    std::uint64_t every = 0U;
    std::int64_t source_delta = 0;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "band_height", band_height); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "every", every); error.has_value()) {
        return error;
    }
    if (auto error = get_i64(instance, "source_delta_bands", source_delta); error.has_value()) {
        return error;
    }
    if (band_height == 0U || every == 0U || band_height > std::numeric_limits<std::uint32_t>::max()) {
        return "band_height and every must be non-zero; band_height must fit uint32";
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    output = input;

    const std::uint64_t band_count = ceil_div_u64(input.height, band_height);
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        const std::uint64_t destination_band = static_cast<std::uint64_t>(y) / band_height;
        if ((destination_band + 1U) % every != 0U) {
            continue;
        }
        const auto source_band = resolve_displaced_index(
            destination_band,
            source_delta,
            band_count,
            boundary);
        if (!source_band.has_value()) {
            const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(input.row_stride);
            std::fill_n(output.bytes.begin() + static_cast<std::ptrdiff_t>(row_offset), static_cast<std::size_t>(input.row_stride), std::uint8_t{0});
            continue;
        }
        const std::uint64_t local_y = static_cast<std::uint64_t>(y) % band_height;
        const std::uint64_t logical_source_y = *source_band * band_height + local_y;
        const auto source_y = resolve_logical_index(logical_source_y, input.height, boundary);
        if (!source_y.has_value()) {
            const std::size_t row_offset = static_cast<std::size_t>(y) * static_cast<std::size_t>(input.row_stride);
            std::fill_n(output.bytes.begin() + static_cast<std::ptrdiff_t>(row_offset), static_cast<std::size_t>(input.row_stride), std::uint8_t{0});
            continue;
        }
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            copy_pixel(
                input,
                *source_y * input.width + x,
                output,
                static_cast<std::uint64_t>(y) * input.width + x);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_address_burst(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::uint64_t burst_count = 0U;
    std::uint64_t burst_length = 0U;
    std::uint64_t maximum_offset = 0U;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "burst_count", burst_count); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "burst_length_pixels", burst_length); error.has_value()) {
        return error;
    }
    if (auto error = get_u64(instance, "max_offset_pixels", maximum_offset); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }

    const std::uint64_t count = pixel_count(input);
    constexpr std::uint64_t kMaximumBursts = 65536U;
    if (burst_count > kMaximumBursts) {
        return "burst_count exceeds the canonical safety limit of 65536";
    }
    if (burst_length > count) {
        return "burst_length_pixels must not exceed the canonical pixel count";
    }
    if (maximum_offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 2)) {
        return "max_offset_pixels exceeds the signed offset contract";
    }

    output = input;
    if (burst_count == 0U || burst_length == 0U) {
        return std::nullopt;
    }

    const std::uint64_t span = maximum_offset * 2U + 1U;
    for (std::uint64_t burst = 0U; burst < burst_count; ++burst) {
        const std::array<std::uint64_t, 1> identity{burst};
        auto stream = make_named_stream(root_seed, instance.instance_id, "address-burst", identity);
        const std::uint64_t start = stream.uniform_below(count);
        const std::int64_t shift =
            static_cast<std::int64_t>(stream.uniform_below(span)) - static_cast<std::int64_t>(maximum_offset);

        for (std::uint64_t index = 0U; index < burst_length && start + index < count; ++index) {
            const std::uint64_t destination = start + index;
            const auto source = resolve_displaced_index(destination, shift, count, boundary);
            const std::size_t destination_offset = static_cast<std::size_t>(destination * 4U);
            if (source.has_value()) {
                copy_pixel(input, *source, output, destination);
            } else {
                std::fill_n(output.bytes.begin() + static_cast<std::ptrdiff_t>(destination_offset), 4U, std::uint8_t{0});
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

[[nodiscard]] MutationMetadata bitmask_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::bitmask;
    metadata.unsigned_min = 0U;
    metadata.unsigned_max = std::numeric_limits<std::uint64_t>::max();
    metadata.unsigned_step = 1U;
    return metadata;
}

[[nodiscard]] MutationMetadata choice_metadata(std::vector<std::string> choices) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::choice;
    metadata.choices = std::move(choices);
    return metadata;
}

[[nodiscard]] MutationMetadata toggle_metadata() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::toggle;
    return metadata;
}

[[nodiscard]] ParameterDescriptor parameter(
    const char* name,
    const ParameterKind kind,
    MutationMetadata mutation) {
    return ParameterDescriptor{name, kind, true, std::move(mutation)};
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
        throw std::runtime_error("failed to register memory/addressing fault operator: " + error);
    }
}

}  // namespace

void register_memory_addressing_faults(FaultRegistry& registry) {
    constexpr std::int64_t kLocalSignedRange = 1LL << 20;
    constexpr std::uint64_t kLocalUnsignedRange = 1ULL << 20;

    register_required(
        registry,
        OperatorDescriptor{
            kFaultAddressOffset,
            1U,
            1U,
            {
                parameter("offset_pixels", ParameterKind::signed_integer, signed_range_metadata(-kLocalSignedRange, kLocalSignedRange, 1)),
                boundary_parameter(),
            }},
        execute_address_offset);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultAddressMask,
            1U,
            1U,
            {
                parameter("xor_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                parameter("and_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                parameter("or_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                boundary_parameter(),
            }},
        execute_address_mask);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultCoordinateRemap,
            1U,
            1U,
            {
                parameter("x_offset", ParameterKind::signed_integer, signed_range_metadata(-4096, 4096, 1)),
                parameter("y_offset", ParameterKind::signed_integer, signed_range_metadata(-4096, 4096, 1)),
                parameter("x_xor_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                parameter("y_xor_mask", ParameterKind::unsigned_integer, bitmask_metadata()),
                parameter("swap_xy", ParameterKind::boolean, toggle_metadata()),
                boundary_parameter(),
            }},
        execute_coordinate_remap);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultTilePermute,
            1U,
            1U,
            {
                parameter("tile_width", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 1024U, 1U)),
                parameter("tile_height", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 1024U, 1U)),
                boundary_parameter(),
            }},
        execute_tile_permute);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultBandRepeat,
            1U,
            1U,
            {
                parameter("band_height", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 4096U, 1U)),
                parameter("every", ParameterKind::unsigned_integer, unsigned_range_metadata(1U, 1024U, 1U)),
                parameter("source_delta_bands", ParameterKind::signed_integer, signed_range_metadata(-1024, 1024, 1)),
                boundary_parameter(),
            }},
        execute_band_repeat);

    register_required(
        registry,
        OperatorDescriptor{
            kFaultAddressBurst,
            1U,
            1U,
            {
                parameter("burst_count", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, 65536U, 1U)),
                parameter("burst_length_pixels", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, kLocalUnsignedRange, 1U)),
                parameter("max_offset_pixels", ParameterKind::unsigned_integer, unsigned_range_metadata(0U, kLocalUnsignedRange, 1U)),
                boundary_parameter(),
            }},
        execute_address_burst);
}

}  // namespace faultmine::core
