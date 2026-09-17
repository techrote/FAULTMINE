#include "faultmine/genome.hpp"

#include <algorithm>
#include <utility>

namespace faultmine::core {

const std::vector<OperatorDescriptor>& OperatorRegistry::descriptors() const noexcept {
    return descriptors_;
}

bool OperatorRegistry::update_mutation_metadata(
    const std::string_view type_id,
    const std::string_view parameter_name,
    MutationMetadata metadata,
    std::string* error) {
    const auto set_error = [error](const std::string_view message) {
        if (error != nullptr) {
            *error = std::string{message};
        }
    };

    auto operator_it = std::find_if(
        descriptors_.begin(), descriptors_.end(),
        [type_id](const OperatorDescriptor& descriptor) { return descriptor.type_id == type_id; });
    if (operator_it == descriptors_.end()) {
        set_error("operator type is not registered");
        return false;
    }
    auto parameter_it = std::find_if(
        operator_it->parameters.begin(), operator_it->parameters.end(),
        [parameter_name](const ParameterDescriptor& parameter) { return parameter.name == parameter_name; });
    if (parameter_it == operator_it->parameters.end()) {
        set_error("operator parameter is not registered");
        return false;
    }
    if (metadata.policy_version == 0U) {
        set_error("mutation metadata policy_version must be non-zero");
        return false;
    }
    parameter_it->mutation = std::move(metadata);
    return true;
}

}  // namespace faultmine::core
