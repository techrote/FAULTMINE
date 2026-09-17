#include "faultmine/lineage.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::app {
namespace {

[[nodiscard]] const core::OperatorInstance* find_instance(
    const core::Genome& genome,
    const core::InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const core::OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

[[nodiscard]] bool lock_state_valid(
    const core::Genome& genome,
    const LockState& locks,
    const core::OperatorRegistry& registry) noexcept {
    std::vector<core::InstanceId> seen_operators;
    for (const core::InstanceId id : locks.operators) {
        if (find_instance(genome, id) == nullptr ||
            std::find(seen_operators.begin(), seen_operators.end(), id) != seen_operators.end()) {
            return false;
        }
        seen_operators.push_back(id);
    }
    std::vector<std::string> seen_parameters;
    for (const ParameterLock& lock : locks.parameters) {
        const core::OperatorInstance* instance = find_instance(genome, lock.instance_id);
        if (instance == nullptr) return false;
        const core::OperatorDescriptor* descriptor = registry.find(instance->type_id);
        if (descriptor == nullptr) return false;
        const bool declared = std::any_of(
            descriptor->parameters.begin(), descriptor->parameters.end(),
            [&lock](const core::ParameterDescriptor& parameter) { return parameter.name == lock.parameter; });
        if (!declared) return false;
        const std::string key = lock.instance_id.to_string() + ":" + lock.parameter;
        if (std::find(seen_parameters.begin(), seen_parameters.end(), key) != seen_parameters.end()) return false;
        seen_parameters.push_back(key);
    }
    return true;
}

[[nodiscard]] bool valid_identity(std::string_view text) noexcept {
    if (text.size() != 64U) return false;
    return std::all_of(text.begin(), text.end(), [](const char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

void normalize_parents(std::vector<std::string>& parents) {
    std::sort(parents.begin(), parents.end());
    parents.erase(std::unique(parents.begin(), parents.end()), parents.end());
}

}  // namespace

std::string_view derivation_kind_name(const DerivationKind kind) noexcept {
    switch (kind) {
        case DerivationKind::manual_root: return "manual-root";
        case DerivationKind::mutation: return "mutation";
        case DerivationKind::crossover: return "crossover";
        case DerivationKind::imported_genome: return "imported-genome";
        case DerivationKind::legacy_project_root: return "legacy-project-root";
    }
    return "manual-root";
}

std::optional<DerivationKind> parse_derivation_kind(const std::string_view text) noexcept {
    if (text == "manual-root") return DerivationKind::manual_root;
    if (text == "mutation") return DerivationKind::mutation;
    if (text == "crossover") return DerivationKind::crossover;
    if (text == "imported-genome") return DerivationKind::imported_genome;
    if (text == "legacy-project-root") return DerivationKind::legacy_project_root;
    return std::nullopt;
}

bool LineageGraph::reset_root(
    std::string source_identity,
    const core::Genome& genome,
    const LockState& locks,
    const DerivationKind kind,
    std::string* error) {
    if (!valid_identity(source_identity)) {
        if (error != nullptr) *error = "lineage root requires a lower-case 64-digit source identity";
        return false;
    }
    SpecimenRecord record;
    record.specimen_id = core::genome_identity_hex(genome);
    record.source_identity = std::move(source_identity);
    record.genome = genome;
    record.locks = locks;
    record.derivations.push_back(SpecimenDerivation{kind, 0U, core::RootSeed{}, 0U, "none", {}});
    record.creation_ordinal = 0U;
    state_.specimens = {std::move(record)};
    state_.active_specimen_id = state_.specimens.front().specimen_id;
    return true;
}

bool LineageGraph::retain(
    std::string source_identity,
    const core::Genome& genome,
    const LockState& locks,
    SpecimenDerivation derivation,
    const bool favourite,
    const core::OperatorRegistry& registry,
    std::string* error) {
    if (const auto validation = core::validate_genome(genome, registry); validation.has_value()) {
        if (error != nullptr) *error = "retained specimen genome is invalid: " + validation->message;
        return false;
    }
    if (!valid_identity(source_identity)) {
        if (error != nullptr) *error = "retained specimen has an invalid source identity";
        return false;
    }
    if (!lock_state_valid(genome, locks, registry)) {
        if (error != nullptr) *error = "retained specimen has invalid lock state";
        return false;
    }
    normalize_parents(derivation.parent_specimen_ids);
    const std::string specimen_id = core::genome_identity_hex(genome);
    for (const std::string& parent : derivation.parent_specimen_ids) {
        if (parent == specimen_id) {
            if (error != nullptr) *error = "lineage derivation cannot parent a specimen to itself";
            return false;
        }
        if (find(parent) == nullptr) {
            if (error != nullptr) *error = "lineage derivation references a parent that is not retained";
            return false;
        }
    }
    if (would_create_cycle(specimen_id, derivation.parent_specimen_ids)) {
        if (error != nullptr) *error = "lineage derivation would create a cycle";
        return false;
    }

    if (SpecimenRecord* existing = find_mutable(specimen_id); existing != nullptr) {
        if (existing->source_identity != source_identity || existing->genome != genome) {
            if (error != nullptr) *error = "duplicate specimen identity conflicts with retained canonical data";
            return false;
        }
        existing->favourite = existing->favourite || favourite;
        existing->locks = locks;
        if (std::find(existing->derivations.begin(), existing->derivations.end(), derivation) == existing->derivations.end()) {
            existing->derivations.push_back(std::move(derivation));
        }
        return true;
    }

    SpecimenRecord record;
    record.specimen_id = specimen_id;
    record.source_identity = std::move(source_identity);
    record.genome = genome;
    record.locks = locks;
    record.derivations.push_back(std::move(derivation));
    record.favourite = favourite;
    record.creation_ordinal = next_creation_ordinal();
    state_.specimens.push_back(std::move(record));
    return true;
}

bool LineageGraph::replace_state(
    LineageState state,
    const core::OperatorRegistry& registry,
    const std::string_view expected_source_identity,
    std::string* error) {
    if (const auto validation = validate_state(state, registry, expected_source_identity); validation.has_value()) {
        if (error != nullptr) *error = validation->message;
        return false;
    }
    state_ = std::move(state);
    return true;
}

const LineageState& LineageGraph::state() const noexcept { return state_; }

const SpecimenRecord* LineageGraph::find(const std::string_view specimen_id) const noexcept {
    const auto found = std::find_if(
        state_.specimens.begin(), state_.specimens.end(),
        [specimen_id](const SpecimenRecord& record) { return record.specimen_id == specimen_id; });
    return found == state_.specimens.end() ? nullptr : &*found;
}

SpecimenRecord* LineageGraph::find_mutable(const std::string_view specimen_id) noexcept {
    const auto found = std::find_if(
        state_.specimens.begin(), state_.specimens.end(),
        [specimen_id](const SpecimenRecord& record) { return record.specimen_id == specimen_id; });
    return found == state_.specimens.end() ? nullptr : &*found;
}

const SpecimenRecord* LineageGraph::active() const noexcept { return find(state_.active_specimen_id); }

bool LineageGraph::set_active(const std::string_view specimen_id) noexcept {
    if (find(specimen_id) == nullptr) return false;
    state_.active_specimen_id = std::string{specimen_id};
    return true;
}

bool LineageGraph::set_favourite(const std::string_view specimen_id, const bool favourite) noexcept {
    SpecimenRecord* record = find_mutable(specimen_id);
    if (record == nullptr) return false;
    record->favourite = favourite;
    return true;
}

std::vector<std::string> LineageGraph::parents_of(const std::string_view specimen_id) const {
    std::vector<std::string> result;
    const SpecimenRecord* record = find(specimen_id);
    if (record == nullptr) return result;
    for (const SpecimenDerivation& derivation : record->derivations) {
        result.insert(result.end(), derivation.parent_specimen_ids.begin(), derivation.parent_specimen_ids.end());
    }
    normalize_parents(result);
    return result;
}

std::vector<std::string> LineageGraph::children_of(const std::string_view specimen_id) const {
    std::vector<std::string> result;
    for (const SpecimenRecord& record : state_.specimens) {
        for (const SpecimenDerivation& derivation : record.derivations) {
            if (std::find(
                    derivation.parent_specimen_ids.begin(), derivation.parent_specimen_ids.end(), specimen_id) !=
                derivation.parent_specimen_ids.end()) {
                result.push_back(record.specimen_id);
                break;
            }
        }
    }
    normalize_parents(result);
    return result;
}

std::vector<std::string> LineageGraph::favourites() const {
    std::vector<std::string> result;
    for (const SpecimenRecord& record : state_.specimens) {
        if (record.favourite) result.push_back(record.specimen_id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::optional<LineageError> LineageGraph::validate_state(
    const LineageState& state,
    const core::OperatorRegistry& registry,
    const std::string_view expected_source_identity) {
    if (!valid_identity(expected_source_identity)) {
        return LineageError{LineageErrorCode::source_mismatch, "expected source identity is invalid"};
    }
    if (state.specimens.empty()) {
        return LineageError{LineageErrorCode::invalid_specimen, "lineage must retain at least one specimen"};
    }
    std::map<std::string, const SpecimenRecord*, std::less<>> records;
    for (const SpecimenRecord& record : state.specimens) {
        if (record.specimen_id != core::genome_identity_hex(record.genome)) {
            return LineageError{LineageErrorCode::invalid_specimen, "specimen id does not match canonical genome identity"};
        }
        if (record.source_identity != expected_source_identity) {
            return LineageError{LineageErrorCode::source_mismatch, "retained specimen source identity does not match project source"};
        }
        if (records.contains(record.specimen_id)) {
            return LineageError{LineageErrorCode::duplicate_conflict, "lineage contains duplicate specimen identities"};
        }
        if (const auto validation = core::validate_genome(record.genome, registry); validation.has_value()) {
            return LineageError{LineageErrorCode::invalid_specimen, "retained specimen genome is invalid: " + validation->message};
        }
        if (!lock_state_valid(record.genome, record.locks, registry)) {
            return LineageError{LineageErrorCode::invalid_lock, "retained specimen lock state is invalid"};
        }
        if (record.derivations.empty()) {
            return LineageError{LineageErrorCode::invalid_specimen, "retained specimen has no provenance record"};
        }
        records.emplace(record.specimen_id, &record);
    }
    if (!records.contains(state.active_specimen_id)) {
        return LineageError{LineageErrorCode::invalid_active, "active lineage specimen is not retained"};
    }

    std::map<std::string, std::vector<std::string>, std::less<>> children;
    for (const SpecimenRecord& record : state.specimens) {
        for (const SpecimenDerivation& derivation : record.derivations) {
            std::set<std::string> seen_parents;
            for (const std::string& parent : derivation.parent_specimen_ids) {
                if (parent == record.specimen_id) {
                    return LineageError{LineageErrorCode::cycle, "lineage contains a self-parent cycle"};
                }
                if (!records.contains(parent)) {
                    return LineageError{LineageErrorCode::missing_parent, "lineage provenance references an absent parent"};
                }
                if (!seen_parents.insert(parent).second) {
                    return LineageError{LineageErrorCode::invalid_specimen, "one derivation repeats the same parent"};
                }
                children[parent].push_back(record.specimen_id);
            }
        }
    }

    enum class Mark { visiting, done };
    std::map<std::string, Mark, std::less<>> marks;
    std::function<bool(const std::string&)> visit = [&](const std::string& id) {
        const auto mark = marks.find(id);
        if (mark != marks.end()) return mark->second == Mark::visiting;
        marks.emplace(id, Mark::visiting);
        for (const std::string& child : children[id]) {
            if (visit(child)) return true;
        }
        marks[id] = Mark::done;
        return false;
    };
    for (const auto& entry : records) {
        if (!marks.contains(entry.first) && visit(entry.first)) {
            return LineageError{LineageErrorCode::cycle, "lineage graph contains a directed cycle"};
        }
    }
    return std::nullopt;
}

bool LineageGraph::would_create_cycle(
    const std::string_view child,
    const std::vector<std::string>& parents) const {
    if (find(child) == nullptr) return false;
    std::set<std::string, std::less<>> wanted(parents.begin(), parents.end());
    std::vector<std::string> queue{std::string{child}};
    std::set<std::string, std::less<>> seen;
    while (!queue.empty()) {
        const std::string current = std::move(queue.back());
        queue.pop_back();
        if (!seen.insert(current).second) continue;
        if (wanted.contains(current) && current != child) return true;
        const auto children = children_of(current);
        queue.insert(queue.end(), children.begin(), children.end());
    }
    return false;
}

std::uint64_t LineageGraph::next_creation_ordinal() const noexcept {
    std::uint64_t next = 0U;
    for (const SpecimenRecord& record : state_.specimens) {
        if (record.creation_ordinal >= next) next = record.creation_ordinal + 1U;
    }
    return next;
}

SpecimenDerivation mutation_derivation(const core::DescendantProvenance& provenance) {
    SpecimenDerivation derivation;
    derivation.kind = DerivationKind::mutation;
    derivation.policy_version = provenance.mutation_policy_version;
    derivation.seed = provenance.mutation_seed;
    derivation.descendant_index = provenance.descendant_index;
    derivation.mutation_radius = std::string{core::mutation_radius_name(provenance.radius)};
    derivation.parent_specimen_ids = {provenance.parent_genome_identity};
    return derivation;
}

}  // namespace faultmine::app
