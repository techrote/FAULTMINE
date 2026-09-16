#include "faultmine/starter_operators.hpp"

#include "faultmine/logical_address.hpp"

#include <array>
#include <cstdint>
#include <limits>
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

[[nodiscard]] std::optional<std::string> execute_row_offset(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::int64_t amount = 0;
    BoundaryPolicy boundary{};
    if (auto error = get_i64(instance, "amount", amount); error.has_value()) {
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
            const auto source_x = resolve_displaced_index(x, amount, input.width, boundary);
            if (source_x.has_value()) {
                copy_pixel(
                    input,
                    static_cast<std::uint64_t>(y) * input.width + *source_x,
                    output,
                    static_cast<std::uint64_t>(y) * input.width + x);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_stride_delta(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::int64_t delta = 0;
    BoundaryPolicy boundary{};
    if (auto error = get_i64(instance, "delta_bytes", delta); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::int64_t physical_stride = static_cast<std::int64_t>(input.row_stride);
    std::int64_t logical_stride = 0;
    if (!checked_add_i64(physical_stride, delta, logical_stride)) {
        return "delta_bytes overflows the signed logical-stride contract";
    }
    const std::uint64_t extent = static_cast<std::uint64_t>(input.bytes.size());
    std::int64_t row_start = 0;

    for (std::uint32_t y = 0U; y < input.height; ++y) {
        for (std::uint32_t x = 0U; x < input.width; ++x) {
            const std::int64_t pixel_offset = static_cast<std::int64_t>(x) * 4;
            for (std::int64_t channel = 0; channel < 4; ++channel) {
                std::int64_t address = 0;
                std::int64_t address_with_channel = 0;
                if (!checked_add_i64(row_start, pixel_offset, address) ||
                    !checked_add_i64(address, channel, address_with_channel)) {
                    return "logical stride addressing overflowed int64";
                }
                const auto source_byte = resolve_logical_index(address_with_channel, extent, boundary);
                if (source_byte.has_value()) {
                    const std::size_t destination =
                        (static_cast<std::size_t>(y) * input.width + x) * 4U +
                        static_cast<std::size_t>(channel);
                    output.bytes[destination] = input.bytes[static_cast<std::size_t>(*source_byte)];
                }
            }
        }
        if (y + 1U < input.height) {
            std::int64_t next_row = 0;
            if (!checked_add_i64(row_start, logical_stride, next_row)) {
                return "logical row origin overflowed int64";
            }
            row_start = next_row;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_address_xor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t mask = 0U;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "mask", mask); error.has_value()) {
        return error;
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::uint64_t count = static_cast<std::uint64_t>(input.width) * input.height;
    for (std::uint64_t destination = 0U; destination < count; ++destination) {
        const auto source = resolve_logical_index(destination ^ mask, count, boundary);
        if (source.has_value()) {
            copy_pixel(input, *source, output, destination);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_channel_permute(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::string_view order;
    if (auto error = get_text(instance, "order", order); error.has_value()) {
        return error;
    }
    if (order.size() != 4U) {
        return "order must contain exactly four channel letters";
    }

    std::array<std::size_t, 4> source_channels{};
    std::array<bool, 4> seen{false, false, false, false};
    for (std::size_t index = 0U; index < 4U; ++index) {
        std::size_t source = 0U;
        switch (order[index]) {
            case 'r': source = 0U; break;
            case 'g': source = 1U; break;
            case 'b': source = 2U; break;
            case 'a': source = 3U; break;
            default: return "order may contain only r, g, b, a";
        }
        if (seen[source]) {
            return "order must contain each channel exactly once";
        }
        seen[source] = true;
        source_channels[index] = source;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::size_t count = static_cast<std::size_t>(input.width) * input.height;
    for (std::size_t pixel = 0U; pixel < count; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            output.bytes[offset + channel] = input.bytes[offset + source_channels[channel]];
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_byte_xor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t mask = 0U;
    std::string_view channel_text;
    if (auto error = get_u64(instance, "mask", mask); error.has_value()) {
        return error;
    }
    if (mask > 0xffU) {
        return "mask must be in the inclusive byte range 0..255";
    }
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) {
        return error;
    }
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) {
        return error;
    }

    output = input;
    const std::uint8_t byte_mask = static_cast<std::uint8_t>(mask);
    const std::size_t count = static_cast<std::size_t>(input.width) * input.height;
    for (std::size_t pixel = 0U; pixel < count; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (channels[channel]) {
                output.bytes[offset + channel] ^= byte_mask;
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::uint8_t rotate_left_byte(
    const std::uint8_t value,
    const std::uint32_t amount) noexcept {
    if (amount == 0U) {
        return value;
    }
    const std::uint32_t wide = value;
    return static_cast<std::uint8_t>(((wide << amount) | (wide >> (8U - amount))) & 0xffU);
}

[[nodiscard]] std::optional<std::string> execute_bit_rotate(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed) {
    std::uint64_t amount = 0U;
    std::string_view channel_text;
    if (auto error = get_u64(instance, "amount", amount); error.has_value()) {
        return error;
    }
    if (amount > 7U) {
        return "amount must be in the inclusive range 0..7";
    }
    if (auto error = get_text(instance, "channels", channel_text); error.has_value()) {
        return error;
    }
    std::array<bool, 4> channels{};
    if (auto error = parse_channels(channel_text, channels); error.has_value()) {
        return error;
    }

    output = input;
    const std::uint32_t rotation = static_cast<std::uint32_t>(amount);
    const std::size_t count = static_cast<std::size_t>(input.width) * input.height;
    for (std::size_t pixel = 0U; pixel < count; ++pixel) {
        const std::size_t offset = pixel * 4U;
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            if (channels[channel]) {
                output.bytes[offset + channel] = rotate_left_byte(input.bytes[offset + channel], rotation);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> execute_scanline_jitter(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    const RootSeed root_seed) {
    std::uint64_t maximum = 0U;
    BoundaryPolicy boundary{};
    if (auto error = get_u64(instance, "max_shift", maximum); error.has_value()) {
        return error;
    }
    if (maximum > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() / 2)) {
        return "max_shift exceeds the signed shift contract";
    }
    if (auto error = get_boundary(instance, boundary); error.has_value()) {
        return error;
    }
    if (auto error = allocate_like(input, output); error.has_value()) {
        return error;
    }

    const std::uint64_t span = maximum * 2U + 1U;
    for (std::uint32_t y = 0U; y < input.height; ++y) {
        const std::array<std::uint64_t, 1> identity{static_cast<std::uint64_t>(y)};
        auto stream = make_named_stream(root_seed, instance.instance_id, "scanline-jitter-row", identity);
        const std::int64_t shift =
            static_cast<std::int64_t>(stream.uniform_below(span)) - static_cast<std::int64_t>(maximum);

        for (std::uint32_t x = 0U; x < input.width; ++x) {
            const auto source_x = resolve_displaced_index(x, shift, input.width, boundary);
            if (source_x.has_value()) {
                copy_pixel(
                    input,
                    static_cast<std::uint64_t>(y) * input.width + *source_x,
                    output,
                    static_cast<std::uint64_t>(y) * input.width + x);
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] ParameterDescriptor i64_parameter(const char* name) {
    return ParameterDescriptor{name, ParameterKind::signed_integer, true, MutationMetadata{}};
}

[[nodiscard]] ParameterDescriptor u64_parameter(const char* name) {
    return ParameterDescriptor{name, ParameterKind::unsigned_integer, true, MutationMetadata{}};
}

[[nodiscard]] ParameterDescriptor text_parameter(const char* name) {
    return ParameterDescriptor{name, ParameterKind::text, true, MutationMetadata{}};
}

void register_required(
    FaultRegistry& registry,
    OperatorDescriptor descriptor,
    const OperatorExecutor executor) {
    std::string error;
    if (!registry.register_operator(std::move(descriptor), executor, &error)) {
        throw std::runtime_error("failed to register starter fault operator: " + error);
    }
}

}  // namespace

void register_starter_faults(FaultRegistry& registry) {
    register_required(
        registry,
        OperatorDescriptor{kFaultRowOffset, 1U, 1U, {i64_parameter("amount"), text_parameter("boundary")}},
        execute_row_offset);
    register_required(
        registry,
        OperatorDescriptor{kFaultStrideDelta, 1U, 1U, {i64_parameter("delta_bytes"), text_parameter("boundary")}},
        execute_stride_delta);
    register_required(
        registry,
        OperatorDescriptor{kFaultAddressXor, 1U, 1U, {u64_parameter("mask"), text_parameter("boundary")}},
        execute_address_xor);
    register_required(
        registry,
        OperatorDescriptor{kFaultChannelPermute, 1U, 1U, {text_parameter("order")}},
        execute_channel_permute);
    register_required(
        registry,
        OperatorDescriptor{kFaultByteXor, 1U, 1U, {u64_parameter("mask"), text_parameter("channels")}},
        execute_byte_xor);
    register_required(
        registry,
        OperatorDescriptor{kFaultBitRotate, 1U, 1U, {u64_parameter("amount"), text_parameter("channels")}},
        execute_bit_rotate);
    register_required(
        registry,
        OperatorDescriptor{kFaultScanlineJitter, 1U, 1U, {u64_parameter("max_shift"), text_parameter("boundary")}},
        execute_scanline_jitter);
}

FaultRegistry make_starter_fault_registry() {
    FaultRegistry registry;
    register_starter_faults(registry);
    return registry;
}

}  // namespace faultmine::core
