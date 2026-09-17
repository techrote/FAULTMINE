#include "faultmine/image.hpp"

#include "faultmine/sha256.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace faultmine::core {
namespace {

constexpr std::uint64_t kBytesPerPixel = 4U;
constexpr std::string_view kSourceIdentityDomain = "FAULTMINE-SOURCE-RGBA8-v1";

[[nodiscard]] ImageError make_error(const ImageErrorCode code, std::string message) {
    return ImageError{code, std::move(message)};
}

void append_u32_le(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
}

[[nodiscard]] bool exceeds_resource_limit(const std::uint64_t bytes) noexcept {
    return bytes > static_cast<std::uint64_t>(kMaxCanonicalImageBytes);
}

}  // namespace

std::optional<std::size_t> canonical_rgba8_byte_size(
    const std::uint32_t width,
    const std::uint32_t height) noexcept {
    if (width == 0U || height == 0U) {
        return std::nullopt;
    }

    const std::uint64_t stride = static_cast<std::uint64_t>(width) * kBytesPerPixel;
    if (stride > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return std::nullopt;
    }
    if (static_cast<std::uint64_t>(height) >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) / stride) {
        return std::nullopt;
    }

    const std::uint64_t total = stride * static_cast<std::uint64_t>(height);
    if (total > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        exceeds_resource_limit(total)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(total);
}

ImageCreateResult make_rgba8_image(const std::uint32_t width, const std::uint32_t height) {
    if (width == 0U || height == 0U) {
        return ImageCreateResult{
            std::nullopt,
            make_error(ImageErrorCode::invalid_dimensions, "canonical image width and height must both be non-zero")};
    }

    const std::uint64_t stride = static_cast<std::uint64_t>(width) * kBytesPerPixel;
    if (stride != 0U && static_cast<std::uint64_t>(height) <= std::numeric_limits<std::uint64_t>::max() / stride) {
        const std::uint64_t total = stride * static_cast<std::uint64_t>(height);
        if (exceeds_resource_limit(total)) {
            return ImageCreateResult{
                std::nullopt,
                make_error(
                    ImageErrorCode::resource_limit,
                    "canonical RGBA8 image exceeds the 512 MiB v1 allocation limit; use a smaller source or an explicit proxy for exploration")};
        }
    }

    const auto byte_size = canonical_rgba8_byte_size(width, height);
    if (!byte_size.has_value()) {
        return ImageCreateResult{
            std::nullopt,
            make_error(ImageErrorCode::size_overflow, "image dimensions overflow the canonical RGBA8 size contract")};
    }

    ImageBuffer image;
    image.width = width;
    image.height = height;
    image.format = PixelFormat::rgba8_unorm;
    image.row_stride = static_cast<std::uint64_t>(width) * kBytesPerPixel;
    image.bytes.assign(*byte_size, std::uint8_t{0});
    return ImageCreateResult{std::move(image), std::nullopt};
}

ImageCreateResult make_rgba8_image(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::span<const std::uint8_t> bytes) {
    auto result = make_rgba8_image(width, height);
    if (!result.ok()) {
        return result;
    }
    if (result.image->bytes.size() != bytes.size()) {
        return ImageCreateResult{
            std::nullopt,
            make_error(ImageErrorCode::byte_count_mismatch, "source byte count does not match width * height * 4")};
    }
    std::copy(bytes.begin(), bytes.end(), result.image->bytes.begin());
    return result;
}

std::optional<ImageError> validate_canonical_image(const ImageBuffer& image) {
    if (image.format != PixelFormat::rgba8_unorm) {
        return make_error(ImageErrorCode::unsupported_format, "canonical image format must be RGBA8 UNORM");
    }
    if (image.width == 0U || image.height == 0U) {
        return make_error(ImageErrorCode::invalid_dimensions, "canonical image dimensions must be non-zero");
    }
    const std::uint64_t stride = static_cast<std::uint64_t>(image.width) * kBytesPerPixel;
    if (stride != 0U && static_cast<std::uint64_t>(image.height) <= std::numeric_limits<std::uint64_t>::max() / stride) {
        const std::uint64_t total = stride * static_cast<std::uint64_t>(image.height);
        if (exceeds_resource_limit(total)) {
            return make_error(ImageErrorCode::resource_limit, "canonical RGBA8 image exceeds the 512 MiB v1 allocation limit");
        }
    }
    const auto byte_size = canonical_rgba8_byte_size(image.width, image.height);
    if (!byte_size.has_value()) {
        return make_error(ImageErrorCode::invalid_dimensions, "canonical image dimensions overflow the size contract");
    }
    const std::uint64_t expected_stride = static_cast<std::uint64_t>(image.width) * kBytesPerPixel;
    if (image.row_stride != expected_stride) {
        return make_error(ImageErrorCode::invalid_stride, "canonical RGBA8 row stride must equal width * 4");
    }
    if (image.bytes.size() != *byte_size) {
        return make_error(ImageErrorCode::byte_count_mismatch, "canonical RGBA8 byte buffer length is inconsistent with dimensions");
    }
    return std::nullopt;
}

std::string source_identity_hex(const ImageBuffer& image) {
    if (const auto error = validate_canonical_image(image); error.has_value()) {
        throw std::invalid_argument("cannot identify invalid canonical image: " + error->message);
    }

    std::vector<std::uint8_t> material;
    material.reserve(kSourceIdentityDomain.size() + 8U + image.bytes.size());
    material.insert(material.end(), kSourceIdentityDomain.begin(), kSourceIdentityDomain.end());
    append_u32_le(material, image.width);
    append_u32_le(material, image.height);
    material.insert(material.end(), image.bytes.begin(), image.bytes.end());
    return sha256_hex(material);
}

}  // namespace faultmine::core
