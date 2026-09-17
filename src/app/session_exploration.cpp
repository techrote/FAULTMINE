#include "faultmine/session.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace faultmine::app {
namespace {

[[nodiscard]] std::string exploration_pipeline_error(const core::PipelineError& error) {
    std::string text{"operator "};
    text += std::to_string(error.operator_index);
    if (!error.operator_type.empty()) text += " (" + error.operator_type + ")";
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

[[nodiscard]] LockState retained_locks(
    const LockState& source,
    const core::Genome& genome,
    const core::OperatorRegistry& registry) {
    LockState retained;
    for (const core::InstanceId id : source.operators) {
        if (find_instance(genome, id) != nullptr) retained.operators.push_back(id);
    }
    for (const ParameterLock& lock : source.parameters) {
        const core::OperatorInstance* instance = find_instance(genome, lock.instance_id);
        if (instance == nullptr) continue;
        const core::OperatorDescriptor* descriptor = registry.find(instance->type_id);
        if (descriptor == nullptr) continue;
        const bool declared = std::any_of(
            descriptor->parameters.begin(), descriptor->parameters.end(),
            [&lock](const core::ParameterDescriptor& parameter) { return parameter.name == lock.parameter; });
        if (declared) retained.parameters.push_back(lock);
    }
    return retained;
}

[[nodiscard]] core::CrossoverLocks to_crossover_locks(const LockState& locks) {
    core::CrossoverLocks converted;
    converted.operators = locks.operators;
    converted.parameters.reserve(locks.parameters.size());
    for (const ParameterLock& lock : locks.parameters) {
        converted.parameters.push_back(core::CrossoverParameterLock{lock.instance_id, lock.parameter});
    }
    return converted;
}

[[nodiscard]] LockState from_crossover_locks(const core::CrossoverLocks& locks) {
    LockState converted;
    converted.operators = locks.operators;
    converted.parameters.reserve(locks.parameters.size());
    for (const core::CrossoverParameterLock& lock : locks.parameters) {
        converted.parameters.push_back(ParameterLock{lock.instance_id, lock.parameter});
    }
    return converted;
}

}  // namespace

const LineageGraph& SessionModel::lineage() const noexcept { return lineage_; }
const std::vector<std::string>& SessionModel::crossover_parent_selection() const noexcept {
    return crossover_parent_selection_;
}

bool SessionModel::promote_exploration_genome(
    const core::Genome& genome,
    std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "no canonical source is loaded";
        return false;
    }
    core::RenderResult full = core::render_pipeline(*source_, genome, editor_.registry());
    if (!full.ok()) {
        if (error != nullptr) {
            *error = full.error.has_value()
                ? exploration_pipeline_error(*full.error)
                : std::string{"full canonical promotion render failed"};
        }
        return false;
    }

    const LockState locks = retained_locks(editor_.locks(), genome, editor_.registry().schema_registry());
    EditResult replaced = editor_.replace_state(genome, locks, true);
    if (!replaced.ok()) {
        if (error != nullptr) *error = replaced.error->message;
        return false;
    }
    SpecimenDerivation root;
    root.kind = DerivationKind::manual_root;
    if (!lineage_.retain(
            source_identity_, genome, locks, std::move(root), false,
            editor_.registry().schema_registry(), error)) {
        return false;
    }
    (void)lineage_.set_active(core::genome_identity_hex(genome));
    editor_.mark_external_change();
    exploration_dirty_ = true;
    crossover_parent_selection_.clear();
    selected_instance_id_.clear();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    mark_preview_dirty();
    return true;
}

bool SessionModel::retain_mutation_specimen(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    const bool favourite,
    std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "no canonical source is loaded";
        return false;
    }
    if (lineage_.find(provenance.parent_genome_identity) == nullptr) {
        if (provenance.parent_genome_identity != editor_.genome_identity()) {
            if (error != nullptr) *error = "mutation provenance parent is not retained and is not the active manual state";
            return false;
        }
        SpecimenDerivation root;
        root.kind = DerivationKind::manual_root;
        if (!lineage_.retain(
                source_identity_, editor_.genome(), editor_.locks(), std::move(root), false,
                editor_.registry().schema_registry(), error)) {
            return false;
        }
        (void)lineage_.set_active(editor_.genome_identity());
    }
    const LockState child_locks = retained_locks(editor_.locks(), genome, editor_.registry().schema_registry());
    if (!lineage_.retain(
            source_identity_, genome, child_locks, mutation_derivation(provenance), favourite,
            editor_.registry().schema_registry(), error)) {
        return false;
    }
    exploration_dirty_ = true;
    return true;
}

bool SessionModel::promote_mutation_specimen(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    std::string* error) {
    if (!retain_mutation_specimen(genome, provenance, false, error)) return false;
    return activate_lineage_specimen(core::genome_identity_hex(genome), error);
}

bool SessionModel::set_mutation_specimen_favourite(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    const bool favourite,
    std::string* error) {
    if (!retain_mutation_specimen(genome, provenance, favourite, error)) return false;
    const std::string id = core::genome_identity_hex(genome);
    if (!lineage_.set_favourite(id, favourite)) {
        if (error != nullptr) *error = "retained specimen disappeared before favourite update";
        return false;
    }
    exploration_dirty_ = true;
    return true;
}

bool SessionModel::toggle_crossover_parent(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    bool* selected,
    std::string* error) {
    if (!retain_mutation_specimen(genome, provenance, false, error)) return false;
    const std::string id = core::genome_identity_hex(genome);
    const auto found = std::find(crossover_parent_selection_.begin(), crossover_parent_selection_.end(), id);
    if (found != crossover_parent_selection_.end()) {
        crossover_parent_selection_.erase(found);
        if (selected != nullptr) *selected = false;
        return true;
    }
    if (crossover_parent_selection_.size() >= core::kMaximumCrossoverParents) {
        if (error != nullptr) *error = "crossover parent selection is limited to eight retained specimens";
        return false;
    }
    crossover_parent_selection_.push_back(id);
    std::sort(crossover_parent_selection_.begin(), crossover_parent_selection_.end());
    if (selected != nullptr) *selected = true;
    return true;
}

bool SessionModel::breed_selected(const core::RootSeed crossover_seed, std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "no canonical source is loaded";
        return false;
    }
    if (crossover_parent_selection_.size() < 2U) {
        if (error != nullptr) *error = "select at least two retained specimens as crossover parents";
        return false;
    }
    std::vector<core::CrossoverParent> parents;
    parents.reserve(crossover_parent_selection_.size());
    for (const std::string& id : crossover_parent_selection_) {
        const SpecimenRecord* record = lineage_.find(id);
        if (record == nullptr || record->source_identity != source_identity_) {
            if (error != nullptr) *error = "selected crossover parent is unavailable or belongs to a different source";
            return false;
        }
        parents.push_back(core::CrossoverParent{record->genome, to_crossover_locks(record->locks)});
    }

    core::CrossoverRequest request;
    request.crossover_seed = crossover_seed;
    const core::CrossoverResult crossed = core::crossover_genomes(
        parents, editor_.registry().schema_registry(), request);
    if (!crossed.ok()) {
        if (error != nullptr) *error = crossed.error.has_value()
            ? crossed.error->message
            : std::string{"crossover failed without a structured error"};
        return false;
    }
    if (std::find(
            crossed.provenance.parent_genome_identities.begin(),
            crossed.provenance.parent_genome_identities.end(),
            crossed.provenance.child_genome_identity) != crossed.provenance.parent_genome_identities.end()) {
        if (error != nullptr) *error = "crossover produced an unchanged parent identity; choose another crossover seed";
        return false;
    }

    core::RenderResult full = core::render_pipeline(*source_, *crossed.genome, editor_.registry());
    if (!full.ok()) {
        if (error != nullptr) {
            *error = full.error.has_value()
                ? exploration_pipeline_error(*full.error)
                : std::string{"full canonical crossover render failed"};
        }
        return false;
    }

    SpecimenDerivation derivation;
    derivation.kind = DerivationKind::crossover;
    derivation.policy_version = crossed.provenance.crossover_policy_version;
    derivation.seed = crossed.provenance.crossover_seed;
    derivation.mutation_radius = "none";
    derivation.parent_specimen_ids = crossed.provenance.parent_genome_identities;
    const LockState child_locks = from_crossover_locks(crossed.inherited_locks);
    if (!lineage_.retain(
            source_identity_, *crossed.genome, child_locks, std::move(derivation), false,
            editor_.registry().schema_registry(), error)) {
        return false;
    }
    if (!lineage_.set_active(crossed.provenance.child_genome_identity)) {
        if (error != nullptr) *error = "crossover child was retained but could not be activated";
        return false;
    }
    EditResult replaced = editor_.replace_state(*crossed.genome, child_locks, true);
    if (!replaced.ok()) {
        if (error != nullptr) *error = replaced.error->message;
        return false;
    }
    editor_.mark_external_change();
    exploration_dirty_ = true;
    crossover_parent_selection_.clear();
    selected_instance_id_.clear();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    mark_preview_dirty();
    return true;
}

bool SessionModel::activate_lineage_specimen(
    const std::string_view specimen_id,
    std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "no canonical source is loaded";
        return false;
    }
    const SpecimenRecord* record = lineage_.find(specimen_id);
    if (record == nullptr) {
        if (error != nullptr) *error = "requested lineage specimen is not retained";
        return false;
    }
    core::RenderResult full = core::render_pipeline(*source_, record->genome, editor_.registry());
    if (!full.ok()) {
        if (error != nullptr) {
            *error = full.error.has_value()
                ? exploration_pipeline_error(*full.error)
                : std::string{"full canonical lineage render failed"};
        }
        return false;
    }
    EditResult replaced = editor_.replace_state(record->genome, record->locks, true);
    if (!replaced.ok()) {
        if (error != nullptr) *error = replaced.error->message;
        return false;
    }
    if (!lineage_.set_active(specimen_id)) {
        if (error != nullptr) *error = "lineage activation failed";
        return false;
    }
    editor_.mark_external_change();
    exploration_dirty_ = true;
    selected_instance_id_.clear();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    mark_preview_dirty();
    return true;
}

bool SessionModel::navigate_lineage_parent(std::string* error) {
    const std::string active_id = core::genome_identity_hex(editor_.genome());
    const std::vector<std::string> parents = lineage_.parents_of(active_id);
    if (parents.empty()) {
        if (error != nullptr) *error = "active specimen has no retained parent";
        return false;
    }
    return activate_lineage_specimen(parents.front(), error);
}

bool SessionModel::navigate_lineage_child(std::string* error) {
    const std::string active_id = core::genome_identity_hex(editor_.genome());
    const std::vector<std::string> children = lineage_.children_of(active_id);
    if (children.empty()) {
        if (error != nullptr) *error = "active specimen has no retained child";
        return false;
    }
    return activate_lineage_specimen(children.front(), error);
}

std::string SessionModel::active_provenance_summary() const {
    const std::string active_id = core::genome_identity_hex(editor_.genome());
    const SpecimenRecord* record = lineage_.find(active_id);
    if (record == nullptr) {
        return "manual edit state; it becomes a new lineage root when retained or saved";
    }
    std::string text = "specimen " + active_id.substr(0U, 12U) + "...";
    text += record->favourite ? " [favourite]" : "";
    text += " | source " + record->source_identity.substr(0U, 12U) + "...";
    if (!record->derivations.empty()) {
        const SpecimenDerivation& derivation = record->derivations.back();
        text += " | " + std::string{derivation_kind_name(derivation.kind)};
        if (derivation.kind == DerivationKind::mutation) {
            text += " policy " + std::to_string(derivation.policy_version);
            text += " seed " + derivation.seed.to_string();
            text += " descendant " + std::to_string(derivation.descendant_index);
        } else if (derivation.kind == DerivationKind::crossover) {
            text += " policy " + std::to_string(derivation.policy_version);
            text += " seed " + derivation.seed.to_string();
            text += " parents " + std::to_string(derivation.parent_specimen_ids.size());
        }
    }
    text += " | parents " + std::to_string(lineage_.parents_of(active_id).size());
    text += " | children " + std::to_string(lineage_.children_of(active_id).size());
    return text;
}

}  // namespace faultmine::app
