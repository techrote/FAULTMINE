#pragma once

#include "faultmine/genome.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kCrossoverPolicyVersion = 1U;
inline constexpr std::size_t kMaximumCrossoverParents = 8U;

struct CrossoverParameterLock {
    InstanceId instance_id{};
    std::string parameter;

    bool operator==(const CrossoverParameterLock&) const = default;
};

struct CrossoverLocks {
    std::vector<InstanceId> operators;
    std::vector<CrossoverParameterLock> parameters;

    bool operator==(const CrossoverLocks&) const = default;
};

struct CrossoverParent {
    Genome genome;
    CrossoverLocks locks;
};

struct CrossoverRequest {
    RootSeed crossover_seed{};
    std::uint32_t policy_version{kCrossoverPolicyVersion};
};

struct CrossoverProvenance {
    std::uint32_t crossover_policy_version{kCrossoverPolicyVersion};
    RootSeed crossover_seed{};
    std::vector<std::string> parent_genome_identities;
    std::string child_genome_identity;
};

enum class CrossoverErrorCode {
    invalid_parent_count,
    duplicate_parent,
    invalid_parent,
    invalid_lock,
    protected_conflict,
    operator_limit,
    invalid_child,
    unsupported_policy,
};

struct CrossoverError {
    CrossoverErrorCode code{CrossoverErrorCode::invalid_child};
    std::string message;
};

struct CrossoverResult {
    std::optional<Genome> genome;
    std::optional<CrossoverError> error;
    CrossoverProvenance provenance;
    CrossoverLocks inherited_locks;

    [[nodiscard]] bool ok() const noexcept {
        return genome.has_value() && !error.has_value();
    }
};

// Crossover policy v1 normalizes parent order by canonical genome identity.
// Stable instance/type matches are preferred; otherwise same-type operators are
// aligned conservatively by order. Protected values must agree when multiple
// parents protect the same aligned semantic slot.
[[nodiscard]] CrossoverResult crossover_genomes(
    const std::vector<CrossoverParent>& parents,
    const OperatorRegistry& registry,
    const CrossoverRequest& request);

}  // namespace faultmine::core
