#pragma once

#include "faultmine/genome.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::core {

inline constexpr std::uint32_t kMutationPolicyVersion = 1U;
inline constexpr std::size_t kMaximumMutationOperators = kMaximumGenomeOperators;

// Radius is deliberately categorical. Each band has typed semantics rather than
// multiplying every parameter by one generic strength scalar.
enum class MutationRadius : std::uint8_t {
    low = 0U,
    medium = 1U,
    high = 2U,
};

[[nodiscard]] std::string_view mutation_radius_name(MutationRadius radius) noexcept;

struct MutationParameterLock {
    InstanceId instance_id{};
    std::string parameter;

    bool operator==(const MutationParameterLock&) const = default;
};

struct MutationLocks {
    std::vector<InstanceId> operators;
    std::vector<MutationParameterLock> parameters;
};

struct MutationRequest {
    RootSeed mutation_seed{};
    std::uint64_t descendant_index{};
    MutationRadius radius{MutationRadius::medium};
    MutationLocks locks;
};

struct DescendantProvenance {
    std::uint32_t mutation_policy_version{kMutationPolicyVersion};
    std::string parent_genome_identity;
    RootSeed mutation_seed{};
    std::uint64_t descendant_index{};
    MutationRadius radius{MutationRadius::medium};
};

enum class MutationErrorCode {
    invalid_parent,
    invalid_descriptor,
    invalid_lock,
    invalid_parameter,
    invalid_descendant,
};

struct MutationError {
    MutationErrorCode code{MutationErrorCode::invalid_parameter};
    std::string message;
};

struct MutationResult {
    std::optional<Genome> genome;
    std::optional<MutationError> error;
    DescendantProvenance provenance;
    bool changed{};
    bool topology_changed{};
    std::string no_change_reason;

    [[nodiscard]] bool ok() const noexcept {
        return genome.has_value() && !error.has_value();
    }
};

[[nodiscard]] std::optional<std::string> validate_mutation_descriptors(
    const OperatorRegistry& registry);

[[nodiscard]] MutationResult generate_descendant(
    const Genome& parent,
    const OperatorRegistry& registry,
    const MutationRequest& request);

}  // namespace faultmine::core
