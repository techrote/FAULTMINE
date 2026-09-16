#include "faultmine/logical_address.hpp"

#include <limits>

namespace faultmine::core {

std::optional<BoundaryPolicy> parse_boundary_policy(const std::string_view text) noexcept {
    if (text == "wrap") {
        return BoundaryPolicy::wrap;
    }
    if (text == "clamp") {
        return BoundaryPolicy::clamp;
    }
    if (text == "fill") {
        return BoundaryPolicy::fill;
    }
    return std::nullopt;
}

std::string_view boundary_policy_name(const BoundaryPolicy policy) noexcept {
    switch (policy) {
        case BoundaryPolicy::wrap: return "wrap";
        case BoundaryPolicy::clamp: return "clamp";
        case BoundaryPolicy::fill: return "fill";
    }
    return "fill";
}

bool checked_add_i64(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept {
    if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
        return false;
    }
    if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
        return false;
    }
    result = left + right;
    return true;
}

bool checked_sub_i64(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept {
    if (right > 0 && left < std::numeric_limits<std::int64_t>::min() + right) {
        return false;
    }
    if (right < 0 && left > std::numeric_limits<std::int64_t>::max() + right) {
        return false;
    }
    result = left - right;
    return true;
}

bool checked_mul_i64(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& result) noexcept {
    if (left == 0 || right == 0) {
        result = 0;
        return true;
    }
    if (left == -1 && right == std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    if (right == -1 && left == std::numeric_limits<std::int64_t>::min()) {
        return false;
    }
    if (left > 0) {
        if (right > 0) {
            if (left > std::numeric_limits<std::int64_t>::max() / right) {
                return false;
            }
        } else if (right < std::numeric_limits<std::int64_t>::min() / left) {
            return false;
        }
    } else if (right > 0) {
        if (left < std::numeric_limits<std::int64_t>::min() / right) {
            return false;
        }
    } else if (left < std::numeric_limits<std::int64_t>::max() / right) {
        return false;
    }
    result = left * right;
    return true;
}

std::optional<std::uint64_t> resolve_logical_index(
    const std::int64_t logical,
    const std::uint64_t extent,
    const BoundaryPolicy policy) noexcept {
    if (extent == 0U || extent > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    const std::int64_t signed_extent = static_cast<std::int64_t>(extent);
    if (logical >= 0 && logical < signed_extent) {
        return static_cast<std::uint64_t>(logical);
    }
    switch (policy) {
        case BoundaryPolicy::wrap: {
            std::int64_t wrapped = logical % signed_extent;
            if (wrapped < 0) {
                wrapped += signed_extent;
            }
            return static_cast<std::uint64_t>(wrapped);
        }
        case BoundaryPolicy::clamp:
            return logical < 0 ? std::uint64_t{0} : extent - 1U;
        case BoundaryPolicy::fill:
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> resolve_logical_index(
    const std::uint64_t logical,
    const std::uint64_t extent,
    const BoundaryPolicy policy) noexcept {
    if (extent == 0U) {
        return std::nullopt;
    }
    if (logical < extent) {
        return logical;
    }
    switch (policy) {
        case BoundaryPolicy::wrap:
            return logical % extent;
        case BoundaryPolicy::clamp:
            return extent - 1U;
        case BoundaryPolicy::fill:
            return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::uint64_t> resolve_displaced_index(
    const std::uint64_t base,
    const std::int64_t displacement,
    const std::uint64_t extent,
    const BoundaryPolicy policy) noexcept {
    if (extent == 0U || extent > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        base > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }

    if (policy == BoundaryPolicy::wrap) {
        const std::int64_t signed_extent = static_cast<std::int64_t>(extent);
        const std::int64_t reduced_displacement = displacement % signed_extent;
        const std::int64_t reduced_base = static_cast<std::int64_t>(base % extent);
        std::int64_t logical = 0;
        if (!checked_sub_i64(reduced_base, reduced_displacement, logical)) {
            return std::nullopt;
        }
        return resolve_logical_index(logical, extent, policy);
    }

    std::int64_t logical = 0;
    if (checked_sub_i64(static_cast<std::int64_t>(base), displacement, logical)) {
        return resolve_logical_index(logical, extent, policy);
    }
    if (policy == BoundaryPolicy::clamp) {
        return displacement < 0 ? extent - 1U : std::uint64_t{0};
    }
    return std::nullopt;
}

}  // namespace faultmine::core
