#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace faultmine::core {

enum class BoundaryPolicy {
    wrap,
    clamp,
    fill,
};

[[nodiscard]] std::optional<BoundaryPolicy> parse_boundary_policy(std::string_view text) noexcept;
[[nodiscard]] std::string_view boundary_policy_name(BoundaryPolicy policy) noexcept;

[[nodiscard]] bool checked_add_i64(std::int64_t left, std::int64_t right, std::int64_t& result) noexcept;
[[nodiscard]] bool checked_sub_i64(std::int64_t left, std::int64_t right, std::int64_t& result) noexcept;
[[nodiscard]] bool checked_mul_i64(std::int64_t left, std::int64_t right, std::int64_t& result) noexcept;

[[nodiscard]] std::optional<std::uint64_t> resolve_logical_index(
    std::int64_t logical,
    std::uint64_t extent,
    BoundaryPolicy policy) noexcept;

[[nodiscard]] std::optional<std::uint64_t> resolve_logical_index(
    std::uint64_t logical,
    std::uint64_t extent,
    BoundaryPolicy policy) noexcept;

// Resolve logical_source = base - displacement without invoking signed overflow.
// Positive displacement therefore moves visible content toward larger destination indices.
[[nodiscard]] std::optional<std::uint64_t> resolve_displaced_index(
    std::uint64_t base,
    std::int64_t displacement,
    std::uint64_t extent,
    BoundaryPolicy policy) noexcept;

}  // namespace faultmine::core
