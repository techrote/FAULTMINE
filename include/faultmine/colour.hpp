#pragma once

#include "faultmine/determinism.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kPaletteSchemaVersion = 1U;
inline constexpr std::uint32_t kLutSchemaVersion = 1U;
inline constexpr std::size_t kMaximumPaletteEntries = 256U;

struct Rgba8 {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255U};

    bool operator==(const Rgba8&) const = default;
};

struct Palette {
    std::uint32_t schema_version{kPaletteSchemaVersion};
    std::vector<Rgba8> entries;

    bool operator==(const Palette&) const = default;
};

struct Lut256 {
    std::uint32_t schema_version{kLutSchemaVersion};
    std::array<std::array<std::uint8_t, 256U>, 4U> channels{};

    bool operator==(const Lut256&) const = default;
};

enum class ColourAssetErrorCode {
    syntax_error,
    duplicate_field,
    missing_field,
    unexpected_field,
    wrong_type,
    invalid_value,
    unsupported_version,
};

struct ColourAssetError {
    ColourAssetErrorCode code{ColourAssetErrorCode::invalid_value};
    std::string path;
    std::string message;
    std::optional<std::size_t> byte_offset;
};

struct PaletteParseResult {
    std::optional<Palette> palette;
    std::optional<ColourAssetError> error;

    [[nodiscard]] bool ok() const noexcept {
        return palette.has_value() && !error.has_value();
    }
};

struct LutParseResult {
    std::optional<Lut256> lut;
    std::optional<ColourAssetError> error;

    [[nodiscard]] bool ok() const noexcept {
        return lut.has_value() && !error.has_value();
    }
};

struct PaletteGenerationSpec {
    Rgba8 start{};
    Rgba8 end{255U, 255U, 255U, 255U};
    std::uint32_t count{2U};
    std::uint8_t jitter{};
};

[[nodiscard]] std::string rgba8_to_hex(Rgba8 colour);
[[nodiscard]] std::optional<Rgba8> parse_rgba8_hex(std::string_view text) noexcept;

[[nodiscard]] std::string serialize_palette_canonical(const Palette& palette);
[[nodiscard]] PaletteParseResult parse_palette(std::string_view text);
[[nodiscard]] std::string palette_identity_hex(const Palette& palette);

[[nodiscard]] Lut256 make_identity_lut();
[[nodiscard]] std::string serialize_lut_canonical(const Lut256& lut);
[[nodiscard]] LutParseResult parse_lut(std::string_view text);
[[nodiscard]] std::string lut_identity_hex(const Lut256& lut);

[[nodiscard]] std::optional<Palette> generate_palette_ramp(
    const PaletteGenerationSpec& spec,
    RootSeed root_seed,
    InstanceId instance_id,
    std::string* error = nullptr);

}  // namespace faultmine::core
