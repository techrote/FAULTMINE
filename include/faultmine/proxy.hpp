#pragma once

#include "faultmine/image.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace faultmine::core {

inline constexpr std::uint32_t kProxyMethodVersion = 1;

struct ProxySpec {
    std::uint32_t max_width{1280};
    std::uint32_t max_height{960};
    std::uint32_t method_version{kProxyMethodVersion};

    bool operator==(const ProxySpec&) const = default;
};

struct ProxyImage {
    ImageBuffer image;
    std::string cache_key;
    bool is_proxy{};
};

struct ProxyError {
    std::string message;
};

struct ProxyResult {
    std::optional<ProxyImage> proxy;
    std::optional<ProxyError> error;

    [[nodiscard]] bool ok() const noexcept {
        return proxy.has_value() && !error.has_value();
    }
};

[[nodiscard]] std::string proxy_cache_key(
    const std::string& source_identity,
    const ProxySpec& spec);

[[nodiscard]] ProxyResult make_nearest_proxy(
    const ImageBuffer& source,
    const std::string& source_identity,
    const ProxySpec& spec = {});

}  // namespace faultmine::core
