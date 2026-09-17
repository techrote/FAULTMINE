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

[[nodiscard]] core::MutationLocks to_mutation_locks(const LockState& locks) {
    core::MutationLocks converted;
    converted.operators = locks.operators;
    converted.parameters.reserve(locks.parameters.size());
    for (const ParameterLock& lock : locks.parameters) {
        converted.parameters.push_back(core::MutationParameterLock{lock.instance_id, lock.parameter});
    }
    return converted;
}

}  // namespace

bool SessionModel::adopt_exploration_genome(
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
                : std::string{"full canonical exploration render failed"};
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
            [&lock](const core::ParameterDescriptor& candidate) { return candidate.name == lock.parameter; });
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

bool SessionModel::ensure_current_lineage_root(std::string* error) {
    if (!source_.has_value()) {
        if (error != nullptr) *error = "lineage requires a loaded canonical source";
        return false;
    }
    const std::string current_identity = editor_.genome_identity();
    if (!lineage_detached_ && !lineage_.empty() && lineage_.active_identity() == current_identity) return true;

    if (lineage_.empty()) {
        if (!lineage_.reset_root(
                source_identity_, editor_.genome(), DerivationKind::manual_root,
                editor_.registry().schema_registry(), error)) {
            return false;
        }
    } else {
        SpecimenRecord record;
        record.genome = editor_.genome();
        record.genome_identity = current_identity;
        record.source_identity = source_identity_;
        record.derivation = make_manual_root_derivation();
        if (!lineage_.retain(std::move(record), editor_.registry().schema_registry(), nullptr, error) ||
            !lineage_.set_active(current_identity, error)) {
            return false;
        }
    }
    lineage_detached_ = false;
    return true;
}

bool SessionModel::promote_exploration_genome(
    const core::Genome& genome,
    std::string* error) {
    if (!ensure_current_lineage_root(error)) return false;

    LineageGraph next = lineage_;
    SpecimenRecord record;
    record.genome = genome;
    record.genome_identity = core::genome_identity_hex(genome);
    record.source_identity = source_identity_;
    record.derivation = make_manual_root_derivation();
    const std::string identity = record.genome_identity;
    if (!next.retain(std::move(record), editor_.registry().schema_registry(), nullptr, error) ||
        !next.set_active(identity, error)) {
        return false;
    }
    if (!adopt_exploration_genome(genome, error)) return false;
    lineage_ = std::move(next);
    lineage_detached_ = false;
    return true;
}

bool SessionModel::retain_mutation_specimen(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    const bool favourite,
    std::string* error) {
    if (!ensure_current_lineage_root(error)) return false;
    if (lineage_.find(provenance.parent_genome_identity) == nullptr) {
        if (error != nullptr) *error = "mutation provenance parent is not retained in the current lineage";
        return false;
    }

    SpecimenRecord record;
    record.genome = genome;
    record.genome_identity = core::genome_identity_hex(genome);
    record.source_identity = source_identity_;
    record.derivation = make_mutation_derivation(provenance);
    record.favourite = favourite;
    bool added = false;
    if (!lineage_.retain(std::move(record), editor_.registry().schema_registry(), &added, error)) return false;
    if (favourite) {
        const std::string identity = core::genome_identity_hex(genome);
        if (!lineage_.set_favourite(identity, true, error)) return false;
    }
    if (added || favourite) editor_.mark_external_change();
    return true;
}

bool SessionModel::promote_mutation_specimen(
    const core::Genome& genome,
    const core::DescendantProvenance& provenance,
    const bool favourite,
    std::string* error) {
    if (!ensure_current_lineage_root(error)) return false;
    if (lineage_.find(provenance.parent_genome_identity) == nullptr) {
        if (error != nullptr) *error = "mutation provenance parent is not retained in the current lineage";
        return false;
    }

    LineageGraph next = lineage_;
    SpecimenRecord record;
    record.genome = genome;
    record.genome_identity = core::genome_identity_hex(genome);
    record.source_identity = source_identity_;
    record.derivation = make_mutation_derivation(provenance);
    record.favourite = favourite;
    const std::string identity = record.genome_identity;
    if (!next.retain(std::move(record), editor_.registry().schema_registry(), nullptr, error) ||
        !next.set_favourite(identity, favourite, error) ||
        !next.set_active(identity, error)) {
        return false;
    }
    if (!adopt_exploration_genome(genome, error)) return false;
    lineage_ = std::move(next);
    lineage_detached_ = false;
    return true;
}

bool SessionModel::breed_and_promote(
    const std::vector<core::Genome>& ordered_parents,
    const core::RootSeed crossover_seed,
    std::string* error) {
    if (!ensure_current_lineage_root(error)) return false;
    if (ordered_parents.size() < 2U || ordered_parents.size() > core::kMaximumCrossoverParents) {
        if (error != nullptr) *error = "breeding requires 2..8 retained parents";
        return false;
    }

    core::CrossoverRequest request;
    request.crossover_seed = crossover_seed;
    request.parents.reserve(ordered_parents.size());
    const std::string current_identity = editor_.genome_identity();
    for (const core::Genome& genome : ordered_parents) {
        const std::string identity = core::genome_identity_hex(genome);
        if (lineage_.find(identity) == nullptr) {
            if (error != nullptr) *error = "every crossover parent must be retained before breeding";
            return false;
        }
        core::CrossoverParent parent;
        parent.genome = genome;
        if (identity == current_identity) parent.locks = to_mutation_locks(editor_.locks());
        request.parents.push_back(std::move(parent));
    }

    core::CrossoverResult crossed = core::crossover_genomes(editor_.registry().schema_registry(), request);
    if (!crossed.ok()) {
        if (error != nullptr) {
            *error = crossed.error.has_value()
                ? crossed.error->message
                : std::string{"typed crossover failed without a structured error"};
        }
        return false;
    }

    LineageGraph next = lineage_;
    SpecimenRecord record;
    record.genome = *crossed.genome;
    record.genome_identity = core::genome_identity_hex(*crossed.genome);
    record.source_identity = source_identity_;
    record.derivation = make_crossover_derivation(crossed.provenance);
    const std::string identity = record.genome_identity;
    if (!next.retain(std::move(record), editor_.registry().schema_registry(), nullptr, error) ||
        !next.set_active(identity, error)) {
        return false;
    }
    if (!adopt_exploration_genome(*crossed.genome, error)) return false;
    lineage_ = std::move(next);
    lineage_detached_ = false;
    return true;
}

bool SessionModel::activate_lineage_specimen(
    const std::string_view genome_identity,
    std::string* error) {
    const SpecimenRecord* record = lineage_.find(genome_identity);
    if (record == nullptr) {
        if (error != nullptr) *error = "requested lineage specimen is not retained";
        return false;
    }
    const core::Genome genome = record->genome;
    if (!adopt_exploration_genome(genome, error)) return false;
    if (!lineage_.set_active(genome_identity, error)) return false;
    lineage_detached_ = false;
    return true;
}

bool SessionModel::set_specimen_favourite(
    const std::string_view genome_identity,
    const bool favourite,
    std::string* error) {
    if (!lineage_.set_favourite(genome_identity, favourite, error)) return false;
    editor_.mark_external_change();
    return true;
}

const LineageGraph& SessionModel::lineage() const noexcept { return lineage_; }

std::string SessionModel::active_provenance_summary() const {
    const SpecimenRecord* record = lineage_.active();
    if (record == nullptr || lineage_detached_) {
        return "manual genome not yet retained as a lineage node";
    }
    return specimen_provenance_summary(*record);
}

}  // namespace faultmine::app
