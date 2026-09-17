#pragma once

#include "faultmine/crossover.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/mutation.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::app {

enum class DerivationKind {
    manual_root,
    mutation,
    crossover,
    imported_genome,
    migrated_project,
};

[[nodiscard]] std::string_view derivation_kind_name(DerivationKind kind) noexcept;
[[nodiscard]] std::optional<DerivationKind> parse_derivation_kind(std::string_view name) noexcept;

struct SpecimenDerivation {
    DerivationKind kind{DerivationKind::manual_root};
    std::vector<std::string> parent_genome_identities;
    std::uint32_t policy_version{};
    std::optional<core::RootSeed> seed;
    std::optional<std::uint64_t> descendant_index;
    std::optional<core::MutationRadius> mutation_radius;

    bool operator==(const SpecimenDerivation&) const = default;
};

struct SpecimenRecord {
    std::string genome_identity;
    std::string source_identity;
    core::Genome genome;
    SpecimenDerivation derivation;
    bool favourite{};
    std::uint64_t creation_ordinal{};

    bool operator==(const SpecimenRecord&) const = default;
};

struct LineageState {
    std::vector<SpecimenRecord> specimens;
    std::string active_genome_identity;

    bool operator==(const LineageState&) const = default;
};

enum class LineageErrorCode {
    invalid_record,
    duplicate_identity,
    missing_parent,
    cycle,
    source_mismatch,
    unknown_active,
};

struct LineageError {
    LineageErrorCode code{LineageErrorCode::invalid_record};
    std::string message;
};

class LineageGraph {
public:
    [[nodiscard]] bool load(
        LineageState state,
        const core::OperatorRegistry& registry,
        std::string_view expected_source_identity,
        std::string* error = nullptr);

    [[nodiscard]] bool reset_root(
        std::string source_identity,
        const core::Genome& genome,
        DerivationKind kind,
        const core::OperatorRegistry& registry,
        std::string* error = nullptr);

    // Duplicate genome identities deliberately collapse to the existing node.
    // The first accepted derivation remains authoritative; favourite=true may
    // promote the retained node to a favourite. Conflicting content is rejected.
    [[nodiscard]] bool retain(
        SpecimenRecord record,
        const core::OperatorRegistry& registry,
        bool* added = nullptr,
        std::string* error = nullptr);

    [[nodiscard]] bool set_active(std::string_view genome_identity, std::string* error = nullptr);
    [[nodiscard]] bool set_favourite(std::string_view genome_identity, bool favourite, std::string* error = nullptr);

    [[nodiscard]] const SpecimenRecord* find(std::string_view genome_identity) const noexcept;
    [[nodiscard]] const SpecimenRecord* active() const noexcept;
    [[nodiscard]] std::vector<std::string> parents(std::string_view genome_identity) const;
    [[nodiscard]] std::vector<std::string> children(std::string_view genome_identity) const;
    [[nodiscard]] const std::vector<SpecimenRecord>& records() const noexcept;
    [[nodiscard]] const std::string& active_identity() const noexcept;
    [[nodiscard]] LineageState state() const;
    [[nodiscard]] std::uint64_t next_creation_ordinal() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

private:
    std::vector<SpecimenRecord> specimens_;
    std::string active_genome_identity_;
};

[[nodiscard]] SpecimenDerivation make_manual_root_derivation(DerivationKind kind = DerivationKind::manual_root);
[[nodiscard]] SpecimenDerivation make_mutation_derivation(const core::DescendantProvenance& provenance);
[[nodiscard]] SpecimenDerivation make_crossover_derivation(const core::CrossoverProvenance& provenance);
[[nodiscard]] std::string specimen_provenance_summary(const SpecimenRecord& record);

}  // namespace faultmine::app
