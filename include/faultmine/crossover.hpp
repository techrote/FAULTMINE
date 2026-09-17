#pragma once

#include "faultmine/mutation.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kCrossoverPolicyVersion = 1U;
inline constexpr std::size_t kMaximumCrossoverParents = 8U;

// Parent order is semantic in crossover policy v1. The first parent is the
// primary topology scaffold; later parents contribute compatible genes and
// conservative unmatched operators.
struct CrossoverParent {
    Genome genome;
    MutationLocks locks;
};

struct CrossoverRequest {
    RootSeed crossover_seed{};
    std::vector<CrossoverParent> parents;
};

struct CrossoverProvenance {
    std::uint32_t crossover_policy_version{kCrossoverPolicyVersion};
    std::vector<std::string> parent_genome_identities;
    RootSeed crossover_seed{};
};

enum class CrossoverErrorCode {
    invalid_parent_count,
    invalid_parent,
    invalid_lock,
    invalid_candidate,
};

struct CrossoverError {
    CrossoverErrorCode code{CrossoverErrorCode::invalid_candidate};
    std::string message;
};

struct CrossoverResult {
    std::optional<Genome> genome;
    std::optional<CrossoverError> error;
    CrossoverProvenance provenance;
    bool changed{};
    bool topology_changed{};

    [[nodiscard]] bool ok() const noexcept {
        return genome.has_value() && !error.has_value();
    }
};

// Deterministic conservative typed crossover. No serialized JSON bytes are
// spliced: alignment and inheritance operate on validated Genome structures.
[[nodiscard]] CrossoverResult crossover_genomes(
    const OperatorRegistry& registry,
    const CrossoverRequest& request);

}  // namespace faultmine::core
