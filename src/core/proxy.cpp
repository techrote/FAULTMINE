#include "faultmine/proxy.hpp"

#include "faultmine/sha256.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>

namespace faultmine::core {
namespace {

[[nodiscard]] std::uint32_t scaled_dimension(
    const std::uint32_t source_dimension,
    const std::uint32_t source_other,
    const std::uint32_t target_other) noexcept {
    if (source_other == 0U) {
        return 1U;
    }
    const std::uint64_t product =
        static_cast<std::uint64_t>(source_dimension) * static_cast<std::uint64_t>(target_other);
    const std::uint64_t scaled = product / source_other;
    return static_cast<std::uint32_t>(std::max<std::uint64_t>(1U, scaled));
}

}  // namespace

std::string proxy_cache_key(
    const std::string& source_identity,
    const ProxySpec& spec) {
    std::string bytes{"FAULTMINE-PROXY-v1:"};
    bytes += source_identity;
    bytes += ':';
    bytes += std::to_string(spec.max_width);
    bytes += ':';
    bytes += std::to_string(spec.max_height);
    bytes += ':';
    bytes += std::to_string(spec.method_version);
    bytes += ":nearest";
    return sha256_hex(bytes);
}

ProxyResult make_nearest_proxy(
    const ImageBuffer& source,
    const std::string& source_identity,
    const ProxySpec& spec) {
    if (const auto validation = validate_canonical_image(source); validation.has_value()) {
        return ProxyResult{std::nullopt, ProxyError{validation->message}};
    }
    if (source_identity.empty()) {
        return ProxyResult{std::nullopt, ProxyError{"source identity must not be empty"}};
    }
    if (spec.max_width == 0U || spec.max_height == 0U) {
        return ProxyResult{std::nullopt, ProxyError{"proxy bounds must be non-zero"}};
    }
    if (spec.method_version != kProxyMethodVersion) {
        return ProxyResult{std::nullopt, ProxyError{"unsupported proxy method version"}};
    }

    const std::string key = proxy_cache_key(source_identity, spec);
    if (source.width <= spec.max_width && source.height <= spec.max_height) {
        return ProxyResult{ProxyImage{source, key, false}, std::nullopt};
    }

    std::uint32_t target_width = spec.max_width;
    std::uint32_t target_height = spec.max_height;
    const std::uint64_t width_pressure =
        static_cast<std::uint64_t>(source.width) * static_cast<std::uint64_t>(spec.max_height);
    const std::uint64_t height_pressure =
        static_cast<std::uint64_t>(source.height) * static_cast<std::uint64_t>(spec.max_width);

    if (width_pressure >= height_pressure) {
        target_width = spec.max_width;
        target_height = scaled_dimension(source.height, source.width, target_width);
    } else {
        target_height = spec.max_height;
        target_width = scaled_dimension(source.width, source.height, target_height);
    }

    auto created = make_rgba8_image(target_width, target_height);
    if (!created.ok()) {
        return ProxyResult{std::nullopt, ProxyError{created.error->message}};
    }
    ImageBuffer output = std::move(*created.image);

    for (std::uint32_t y = 0U; y < target_height; ++y) {
        const std::uint32_t source_y = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(y) * source.height) / target_height);
        for (std::uint32_t x = 0U; x < target_width; ++x) {
            const std::uint32_t source_x = static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(x) * source.width) / target_width);
            const std::size_t source_offset =
                static_cast<std::size_t>(source_y) * source.row_stride +
                static_cast<std::size_t>(source_x) * 4U;
            const std::size_t target_offset =
                static_cast<std::size_t>(y) * output.row_stride +
                static_cast<std::size_t>(x) * 4U;
            for (std::size_t channel = 0U; channel < 4U; ++channel) {
                output.bytes[target_offset + channel] = source.bytes[source_offset + channel];
            }
        }
    }

    return ProxyResult{ProxyImage{std::move(output), key, true}, std::nullopt};
}

}  // namespace faultmine::core
