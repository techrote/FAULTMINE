#include <faultmine/core.hpp>

namespace faultmine::core {

std::string_view product_name() noexcept {
    return "FAULTMINE";
}

std::uint32_t bootstrap_contract_version() noexcept {
    return 1U;
}

}  // namespace faultmine::core
