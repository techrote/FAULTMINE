#include "faultmine/fault_catalogue.hpp"

#include "faultmine/memory_addressing_operators.hpp"
#include "faultmine/starter_operators.hpp"

namespace faultmine::core {

FaultRegistry make_default_fault_registry() {
    FaultRegistry registry;
    register_starter_faults(registry);
    register_memory_addressing_faults(registry);
    return registry;
}

}  // namespace faultmine::core
