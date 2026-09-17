#pragma once

#include "faultmine/editor.hpp"
#include "faultmine/mutation.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::app {

enum class DerivationKind : std::uint8_t {
    manual_root,
    mutation,
    crossover,
    imported_genome,
    legacy_project_root,
};

[[nodiscard]] std::string_view derivation_kind_name(DerivationKind kind) noexcept;
[[nodiscard]] std::optional<DerivationKind> parse_derivation_kind(std::string_view text) noexcept;

struct SpecimenDerivation {
    DerivationKind kind{DerivationKind::manual_root};
    std::uint32_t policy_version{};
    core::RootSeed seed{};
    std::uint64_t descendant_index{};
    std::string mutation_radius{"none"};
    std::vector<std::string> parent_specimen_ids;

    bool operator==(const SpecimenDerivation&) const = default;
};

struct SpecimenRecord {
    std::string specimen_id;
    std::string source_identity;
    core::Genome genome;
    LockState locks;
    std::vector<SpecimenDerivation> derivations;
    bool favourite{};
    std::uint64_t creation_ordinal{};

    bool operator==(const SpecimenRecord&) const = default;
};

struct LineageState {
    std::vector<SpecimenRecord> specimens;
    std::string active_specimen_id;

    bool operator==(const LineageState&) const = default;
};

enum class LineageErrorCode {
    invalid_specimen,
    duplicate_conflict,
    missing_parent,
    cycle,
    source_mismatch,
    invalid_active,
    invalid_lock,
};

struct LineageError {
    LineageErrorCode code{LineageErrorCode::invalid_specimen};
    std::string message;
};

class LineageGraph {
public:
    [[nodiscard]] bool reset_root(
        std::string source_identity,
        const core::Genome& genome,
        const LockState& locks,
        DerivationKind kind = DerivationKind::manual_root,
        std::string* error = nullptr);

    // Genome identity is the durable specimen key. Retaining the same canonical
    // genome again merges provenance/favourite state rather than creating an
    // indistinguishable duplicate node.
    [[nodiscard]] bool retain(
        std::string source_identity,
        const core::Genome& genome,
        const LockState& locks,
        SpecimenDerivation derivation,
        bool favourite,
        const core::OperatorRegistry& registry,
        std::string* error = nullptr);

    [[nodiscard]] bool replace_state(
        LineageState state,
        const core::OperatorRegistry& registry,
        std::string_view expected_source_identity,
        std::string* error = nullptr);

    [[nodiscard]] const LineageState& state() const noexcept;
    [[nodiscard]] const SpecimenRecord* find(std::string_view specimen_id) const noexcept;
    [[nodiscard]] SpecimenRecord* find_mutable(std::string_view specimen_id) noexcept;
    [[nodiscard]] const SpecimenRecord* active() const noexcept;
    [[nodiscard]] bool set_active(std::string_view specimen_id) noexcept;
    [[nodiscard]] bool set_favourite(std::string_view specimen_id, bool favourite) noexcept;
    [[nodiscard]] std::vector<std::string> parents_of(std::string_view specimen_id) const;
    [[nodiscard]] std::vector<std::string> children_of(std::string_view specimen_id) const;
    [[nodiscard]] std::vector<std::string> favourites() const;

    [[nodiscard]] static std::optional<LineageError> validate_state(
        const LineageState& state,
        const core::OperatorRegistry& registry,
        std::string_view expected_source_identity);

private:
    [[nodiscard]] bool would_create_cycle(
        std::string_view child,
        const std::vector<std::string>& parents) const;
    [[nodiscard]] std::uint64_t next_creation_ordinal() const noexcept;

    LineageState state_;
};

[[nodiscard]] SpecimenDerivation mutation_derivation(const core::DescendantProvenance& provenance);

}  // namespace faultmine::app
