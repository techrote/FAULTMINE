#pragma once

#include <cstdint>
#include <string_view>

namespace faultmine::core {

[[nodiscard]] std::string_view product_name() noexcept;
[[nodiscard]] std::uint32_t bootstrap_contract_version() noexcept;

}  // namespace faultmine::core
