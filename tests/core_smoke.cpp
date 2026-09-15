#include <faultmine/core.hpp>

#include <cstdlib>
#include <iostream>
#include <string_view>

int main() {
    if (faultmine::core::product_name() != std::string_view{"FAULTMINE"}) {
        std::cerr << "product_name contract mismatch\n";
        return EXIT_FAILURE;
    }

    if (faultmine::core::bootstrap_contract_version() != 1U) {
        std::cerr << "bootstrap contract version mismatch\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
