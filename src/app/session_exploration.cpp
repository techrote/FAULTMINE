#include "faultmine/session.hpp"

#include <algorithm>
#include <string>

namespace faultmine::app {
namespace {

[[nodiscard]] std::string exploration_pipeline_error(const core::PipelineError& error) {
    std::string text{"operator "};
    text += std::to_string(error.operator_index);
    if (!error.operator_type.empty()) {
        text += " (" + error.operator_type + ")";
    }
    text += ": " + error.message;
    return text;
}

[[nodiscard]] const core::OperatorInstance* find_instance(
    const core::Genome& genome,
    const core::InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const core::OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

}  // namespace

bool SessionModel::promote_exploration_genome(
    const core::Genome& genome,
    std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "no canonical source is loaded";
        return false;
    }

    // Promotion deliberately renders from the full canonical source. The tray
    // preview is evidence for selection only and is never adopted as pixels.
    core::RenderResult full = core::render_pipeline(*source_, genome, editor_.registry());
    if (!full.ok()) {
        if (error != nullptr) {
            *error = full.error.has_value()
                ? exploration_pipeline_error(*full.error)
                : std::string{"full canonical promotion render failed"};
        }
        return false;
    }

    LockState retained;
    for (const core::InstanceId id : editor_.locks().operators) {
        if (find_instance(genome, id) != nullptr) retained.operators.push_back(id);
    }
    for (const ParameterLock& lock : editor_.locks().parameters) {
        const core::OperatorInstance* instance = find_instance(genome, lock.instance_id);
        if (instance == nullptr) continue;
        const core::OperatorDescriptor* descriptor = editor_.registry().schema_registry().find(instance->type_id);
        if (descriptor == nullptr) continue;
        const auto parameter = std::find_if(
            descriptor->parameters.begin(), descriptor->parameters.end(),
            [&lock](const core::ParameterDescriptor& candidate) {
                return candidate.name == lock.parameter;
            });
        if (parameter != descriptor->parameters.end()) retained.parameters.push_back(lock);
    }

    EditResult replaced = editor_.replace_state(genome, std::move(retained), true);
    if (!replaced.ok()) {
        if (error != nullptr) *error = replaced.error->message;
        return false;
    }
    editor_.mark_external_change();
    selected_instance_id_.clear();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    mark_preview_dirty();
    return true;
}

}  // namespace faultmine::app
