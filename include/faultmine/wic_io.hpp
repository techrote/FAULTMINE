#pragma once

#include "faultmine/image.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace faultmine::io {

enum class WicErrorCode {
    com_initialization_failed,
    factory_creation_failed,
    open_failed,
    decode_failed,
    invalid_dimensions,
    normalization_failed,
    encode_failed,
};

struct WicError {
    WicErrorCode code{WicErrorCode::decode_failed};
    std::string operation;
    std::int32_t hresult{};
    std::string message;
};

struct LoadedSource {
    core::ImageBuffer image;
    std::string source_identity;
    std::filesystem::path path;
};

struct WicLoadResult {
    std::optional<LoadedSource> source;
    std::optional<WicError> error;

    [[nodiscard]] bool ok() const noexcept {
        return source.has_value() && !error.has_value();
    }
};

[[nodiscard]] WicLoadResult load_wic_image(const std::filesystem::path& path);
[[nodiscard]] std::optional<WicError> save_wic_png(
    const core::ImageBuffer& image,
    const std::filesystem::path& path);

}  // namespace faultmine::io
