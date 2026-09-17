#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace faultmine::core {

inline constexpr std::size_t kMaxCanonicalImageBytes = 512U * 1024U * 1024U;

enum class PixelFormat : std::uint32_t {
    rgba8_unorm = 1,
};

struct ImageBuffer {
    std::uint32_t width{};
    std::uint32_t height{};
    PixelFormat format{PixelFormat::rgba8_unorm};
    std::uint64_t row_stride{};
    std::vector<std::uint8_t> bytes;

    bool operator==(const ImageBuffer&) const = default;
};

enum class ImageErrorCode {
    invalid_dimensions,
    size_overflow,
    resource_limit,
    invalid_stride,
    byte_count_mismatch,
    unsupported_format,
};

struct ImageError {
    ImageErrorCode code{ImageErrorCode::invalid_dimensions};
    std::string message;
};

struct ImageCreateResult {
    std::optional<ImageBuffer> image;
    std::optional<ImageError> error;

    [[nodiscard]] bool ok() const noexcept {
        return image.has_value() && !error.has_value();
    }
};

[[nodiscard]] std::optional<std::size_t> canonical_rgba8_byte_size(
    std::uint32_t width,
    std::uint32_t height) noexcept;

[[nodiscard]] ImageCreateResult make_rgba8_image(
    std::uint32_t width,
    std::uint32_t height);

[[nodiscard]] ImageCreateResult make_rgba8_image(
    std::uint32_t width,
    std::uint32_t height,
    std::span<const std::uint8_t> bytes);

[[nodiscard]] std::optional<ImageError> validate_canonical_image(const ImageBuffer& image);

// SHA-256 identity of a domain-separated canonical RGBA8 source representation.
// Path, timestamps and other convenience metadata are intentionally excluded.
[[nodiscard]] std::string source_identity_hex(const ImageBuffer& image);

}  // namespace faultmine::core
