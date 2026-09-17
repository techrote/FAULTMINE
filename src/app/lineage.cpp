#include "faultmine/lineage.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::app {
namespace {

[[nodiscard]] bool valid_hex_identity(const std::string_view text) noexcept {
    return text.size() == 64U && std::all_of(text.begin(), text.end(), [](const char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

[[nodiscard]] const SpecimenRecord* find_record(
    const std::vector<SpecimenRecord>& records,
    const std::string_view identity) noexcept {
    const auto found = std::find_if(
        records.begin(), records.end(),
        [identity](const SpecimenRecord& record) { return record.genome_identity == identity; });
    return found == records.end() ? nullptr : &*found;
}

[[nodiscard]] SpecimenRecord* find_record(
    std::vector<SpecimenRecord>& records,
    const std::string_view identity) noexcept {
    const auto found = std::find_if(
        records.begin(), records.end(),
        [identity](const SpecimenRecord& record) { return record.genome_identity == identity; });
    return found == records.end() ? nullptr : &*found;
}

[[nodiscard]] bool validate_derivation_shape(const SpecimenRecord& record, std::string& error) {
    const auto& parents = record.derivation.parent_genome_identities;
    if (std::adjacent_find(parents.begin(), parents.end()) != parents.end()) {
        error = "lineage derivation contains duplicate adjacent parent identities";
        return false;
    }
    std::vector<std::string> sorted = parents;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
        error = "lineage derivation contains duplicate parent identities";
        return false;
    }
    if (std::find(parents.begin(), parents.end(), record.genome_identity) != parents.end()) {
        error = "a lineage specimen cannot list itself as a parent";
        return false;
    }

    switch (record.derivation.kind) {
        case DerivationKind::manual_root:
        case DerivationKind::imported_genome:
        case DerivationKind::migrated_project:
            if (!parents.empty() || record.derivation.policy_version != 0U ||
                record.derivation.seed.has_value() || record.derivation.descendant_index.has_value() ||
                record.derivation.mutation_radius.has_value()) {
                error = "root/import derivation must not fabricate parent or policy state";
                return false;
            }
            break;
        case DerivationKind::mutation:
            if (parents.size() != 1U || record.derivation.policy_version != core::kMutationPolicyVersion ||
                !record.derivation.seed.has_value() || !record.derivation.descendant_index.has_value() ||
                !record.derivation.mutation_radius.has_value()) {
                error = "mutation derivation is missing its exact v1 parent/seed/index/radius provenance";
                return false;
            }
            break;
        case DerivationKind::crossover:
            if (parents.size() < 2U || record.derivation.policy_version != core::kCrossoverPolicyVersion ||
                !record.derivation.seed.has_value() || record.derivation.descendant_index.has_value() ||
                record.derivation.mutation_radius.has_value()) {
                error = "crossover derivation is missing its exact v1 ordered parents/seed provenance";
                return false;
            }
            break;
    }
    return true;
}

[[nodiscard]] bool visit_cycle(
    const std::vector<SpecimenRecord>& records,
    const std::size_t index,
    std::vector<std::uint8_t>& marks) {
    if (marks[index] == 1U) return true;
    if (marks[index] == 2U) return false;
    marks[index] = 1U;
    for (const std::string& parent_identity : records[index].derivation.parent_genome_identities) {
        for (std::size_t parent_index = 0U; parent_index < records.size(); ++parent_index) {
            if (records[parent_index].genome_identity == parent_identity &&
                visit_cycle(records, parent_index, marks)) {
                return true;
            }
        }
    }
    marks[index] = 2U;
    return false;
}

[[nodiscard]] bool validate_state(
    const LineageState& state,
    const core::OperatorRegistry& registry,
    const std::string_view expected_source_identity,
    std::string& error) {
    if (state.specimens.empty()) {
        error = "lineage must retain at least one specimen";
        return false;
    }
    if (!valid_hex_identity(expected_source_identity)) {
        error = "lineage expected source identity must be 64 lower-case hexadecimal digits";
        return false;
    }

    std::vector<std::string> identities;
    std::vector<std::uint64_t> ordinals;
    identities.reserve(state.specimens.size());
    ordinals.reserve(state.specimens.size());
    for (const SpecimenRecord& record : state.specimens) {
        if (!valid_hex_identity(record.genome_identity) ||
            record.genome_identity != core::genome_identity_hex(record.genome)) {
            error = "lineage specimen identity does not match its canonical genome";
            return false;
        }
        if (record.source_identity != expected_source_identity) {
            error = "lineage specimen source identity differs from the project source";
            return false;
        }
        if (const auto validation = core::validate_genome(record.genome, registry); validation.has_value()) {
            error = "lineage specimen contains an invalid genome: " + validation->message;
            return false;
        }
        std::string derivation_error;
        if (!validate_derivation_shape(record, derivation_error)) {
            error = std::move(derivation_error);
            return false;
        }
        identities.push_back(record.genome_identity);
        ordinals.push_back(record.creation_ordinal);
    }

    std::sort(identities.begin(), identities.end());
    if (std::adjacent_find(identities.begin(), identities.end()) != identities.end()) {
        error = "lineage contains duplicate canonical genome identities";
        return false;
    }
    std::sort(ordinals.begin(), ordinals.end());
    if (std::adjacent_find(ordinals.begin(), ordinals.end()) != ordinals.end()) {
        error = "lineage contains duplicate creation ordinals";
        return false;
    }

    for (const SpecimenRecord& record : state.specimens) {
        for (const std::string& parent : record.derivation.parent_genome_identities) {
            if (find_record(state.specimens, parent) == nullptr) {
                error = "lineage derivation references a missing retained parent";
                return false;
            }
        }
    }

    std::vector<std::uint8_t> marks(state.specimens.size(), 0U);
    for (std::size_t index = 0U; index < state.specimens.size(); ++index) {
        if (visit_cycle(state.specimens, index, marks)) {
            error = "lineage parent links contain a cycle";
            return false;
        }
    }

    if (find_record(state.specimens, state.active_genome_identity) == nullptr) {
        error = "lineage active specimen is not retained";
        return false;
    }
    return true;
}

}  // namespace

std::string_view derivation_kind_name(const DerivationKind kind) noexcept {
    switch (kind) {
        case DerivationKind::manual_root: return "manual-root";
        case DerivationKind::mutation: return "mutation";
        case DerivationKind::crossover: return "crossover";
        case DerivationKind::imported_genome: return "imported-genome";
        case DerivationKind::migrated_project: return "migrated-project";
    }
    return "manual-root";
}

std::optional<DerivationKind> parse_derivation_kind(const std::string_view name) noexcept {
    if (name == "manual-root") return DerivationKind::manual_root;
    if (name == "mutation") return DerivationKind::mutation;
    if (name == "crossover") return DerivationKind::crossover;
    if (name == "imported-genome") return DerivationKind::imported_genome;
    if (name == "migrated-project") return DerivationKind::migrated_project;
    return std::nullopt;
}

bool LineageGraph::load(
    LineageState state,
    const core::OperatorRegistry& registry,
    const std::string_view expected_source_identity,
    std::string* error) {
    std::string validation_error;
    if (!validate_state(state, registry, expected_source_identity, validation_error)) {
        if (error != nullptr) *error = std::move(validation_error);
        return false;
    }
    std::sort(
        state.specimens.begin(), state.specimens.end(),
        [](const SpecimenRecord& left, const SpecimenRecord& right) {
            if (left.creation_ordinal != right.creation_ordinal) {
                return left.creation_ordinal < right.creation_ordinal;
            }
            return left.genome_identity < right.genome_identity;
        });
    specimens_ = std::move(state.specimens);
    active_genome_identity_ = std::move(state.active_genome_identity);
    return true;
}

bool LineageGraph::reset_root(
    std::string source_identity,
    const core::Genome& genome,
    const DerivationKind kind,
    const core::OperatorRegistry& registry,
    std::string* error) {
    SpecimenRecord root;
    root.genome = genome;
    root.genome_identity = core::genome_identity_hex(genome);
    root.source_identity = std::move(source_identity);
    root.derivation = make_manual_root_derivation(kind);
    root.creation_ordinal = 0U;
    LineageState state;
    state.active_genome_identity = root.genome_identity;
    state.specimens.push_back(std::move(root));
    return load(std::move(state), registry, state.specimens.empty() ? std::string_view{} : state.specimens.front().source_identity, error);
}

bool LineageGraph::retain(
    SpecimenRecord record,
    const core::OperatorRegistry& registry,
    bool* added,
    std::string* error) {
    if (added != nullptr) *added = false;
    record.genome_identity = core::genome_identity_hex(record.genome);

    if (SpecimenRecord* existing = find_record(specimens_, record.genome_identity); existing != nullptr) {
        if (existing->genome != record.genome || existing->source_identity != record.source_identity) {
            if (error != nullptr) *error = "duplicate genome identity conflicts with retained specimen content/provenance source";
            return false;
        }
        if (record.favourite) existing->favourite = true;
        return true;
    }

    if (!specimens_.empty() && record.source_identity != specimens_.front().source_identity) {
        if (error != nullptr) *error = "retained specimen belongs to a different source identity";
        return false;
    }
    for (const std::string& parent : record.derivation.parent_genome_identities) {
        if (parent == record.genome_identity) {
            if (error != nullptr) *error = "retained specimen cannot be its own parent";
            return false;
        }
        if (find_record(specimens_, parent) == nullptr) {
            if (error != nullptr) *error = "retained specimen references a parent that is not present in lineage";
            return false;
        }
    }
    if (const auto validation = core::validate_genome(record.genome, registry); validation.has_value()) {
        if (error != nullptr) *error = "retained specimen genome is invalid: " + validation->message;
        return false;
    }
    std::string derivation_error;
    if (!validate_derivation_shape(record, derivation_error)) {
        if (error != nullptr) *error = std::move(derivation_error);
        return false;
    }

    record.creation_ordinal = next_creation_ordinal();
    specimens_.push_back(std::move(record));
    if (added != nullptr) *added = true;
    return true;
}

bool LineageGraph::set_active(const std::string_view genome_identity, std::string* error) {
    if (find(genome_identity) == nullptr) {
        if (error != nullptr) *error = "requested active lineage specimen is not retained";
        return false;
    }
    active_genome_identity_ = std::string{genome_identity};
    return true;
}

bool LineageGraph::set_favourite(
    const std::string_view genome_identity,
    const bool favourite,
    std::string* error) {
    SpecimenRecord* record = find_record(specimens_, genome_identity);
    if (record == nullptr) {
        if (error != nullptr) *error = "requested favourite specimen is not retained";
        return false;
    }
    record->favourite = favourite;
    return true;
}

const SpecimenRecord* LineageGraph::find(const std::string_view genome_identity) const noexcept {
    return find_record(specimens_, genome_identity);
}

const SpecimenRecord* LineageGraph::active() const noexcept {
    return find(active_genome_identity_);
}

std::vector<std::string> LineageGraph::parents(const std::string_view genome_identity) const {
    const SpecimenRecord* record = find(genome_identity);
    return record == nullptr ? std::vector<std::string>{} : record->derivation.parent_genome_identities;
}

std::vector<std::string> LineageGraph::children(const std::string_view genome_identity) const {
    std::vector<std::string> output;
    for (const SpecimenRecord& record : specimens_) {
        if (std::find(
                record.derivation.parent_genome_identities.begin(),
                record.derivation.parent_genome_identities.end(),
                genome_identity) != record.derivation.parent_genome_identities.end()) {
            output.push_back(record.genome_identity);
        }
    }
    return output;
}

const std::vector<SpecimenRecord>& LineageGraph::records() const noexcept { return specimens_; }
const std::string& LineageGraph::active_identity() const noexcept { return active_genome_identity_; }

LineageState LineageGraph::state() const {
    return LineageState{specimens_, active_genome_identity_};
}

std::uint64_t LineageGraph::next_creation_ordinal() const noexcept {
    std::uint64_t next = 0U;
    for (const SpecimenRecord& record : specimens_) {
        if (record.creation_ordinal >= next && record.creation_ordinal != std::numeric_limits<std::uint64_t>::max()) {
            next = record.creation_ordinal + 1U;
        }
    }
    return next;
}

bool LineageGraph::empty() const noexcept { return specimens_.empty(); }

SpecimenDerivation make_manual_root_derivation(const DerivationKind kind) {
    SpecimenDerivation derivation;
    derivation.kind = kind;
    return derivation;
}

SpecimenDerivation make_mutation_derivation(const core::DescendantProvenance& provenance) {
    SpecimenDerivation derivation;
    derivation.kind = DerivationKind::mutation;
    derivation.parent_genome_identities = {provenance.parent_genome_identity};
    derivation.policy_version = provenance.mutation_policy_version;
    derivation.seed = provenance.mutation_seed;
    derivation.descendant_index = provenance.descendant_index;
    derivation.mutation_radius = provenance.radius;
    return derivation;
}

SpecimenDerivation make_crossover_derivation(const core::CrossoverProvenance& provenance) {
    SpecimenDerivation derivation;
    derivation.kind = DerivationKind::crossover;
    derivation.parent_genome_identities = provenance.parent_genome_identities;
    derivation.policy_version = provenance.crossover_policy_version;
    derivation.seed = provenance.crossover_seed;
    return derivation;
}

std::string specimen_provenance_summary(const SpecimenRecord& record) {
    std::string text{derivation_kind_name(record.derivation.kind)};
    text += " | genome " + record.genome_identity;
    text += " | source " + record.source_identity;
    if (!record.derivation.parent_genome_identities.empty()) {
        text += " | parents ";
        for (std::size_t index = 0U; index < record.derivation.parent_genome_identities.size(); ++index) {
            if (index != 0U) text += ',';
            text += record.derivation.parent_genome_identities[index];
        }
    }
    if (record.derivation.seed.has_value()) {
        text += " | seed " + record.derivation.seed->to_string();
    }
    if (record.derivation.descendant_index.has_value()) {
        text += " | descendant " + std::to_string(*record.derivation.descendant_index);
    }
    if (record.derivation.mutation_radius.has_value()) {
        text += " | radius ";
        text += core::mutation_radius_name(*record.derivation.mutation_radius);
    }
    if (record.favourite) text += " | favourite";
    return text;
}

}  // namespace faultmine::app
