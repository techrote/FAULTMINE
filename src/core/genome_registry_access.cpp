#include "faultmine/genome.hpp"

namespace faultmine::core {

const std::vector<OperatorDescriptor>& OperatorRegistry::descriptors() const noexcept {
    return descriptors_;
}

}  // namespace faultmine::core
