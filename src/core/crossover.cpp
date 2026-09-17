#include "faultmine/crossover.hpp"

#include "faultmine/determinism.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

[[nodiscard]] CrossoverResult fail_result(
    const CrossoverErrorCode code,
    std::string message,
    CrossoverProvenance provenance) {
    CrossoverResult result;
    result.error = CrossoverError{code, std::move(message)};
    result.provenance = std::move(provenance);
    return result;
}

[[nodiscard]] bool operator_locked(const MutationLocks& locks, const InstanceId id) noexcept {
    return std::find(locks.operators.begin(), locks.operators.end(), id) != locks.operators.end();
}

[[nodiscard]] bool parameter_locked(
    const MutationLocks& locks,
    const InstanceId id,
    const std::string_view name) noexcept {
    return std::any_of(
        locks.parameters.begin(), locks.parameters.end(),
        [id, name](const MutationParameterLock& lock) {
            return lock.instance_id == id && lock.parameter == name;
        });
}

[[nodiscard]] const OperatorInstance* find_instance(const Genome& genome, const InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

[[nodiscard]] bool parameter_declared(
    const OperatorRegistry& registry,
    const OperatorInstance& instance,
    const std::string_view parameter) noexcept {
    const OperatorDescriptor* descriptor = registry.find(instance.type_id);
    return descriptor != nullptr && std::any_of(
        descriptor->parameters.begin(), descriptor->parameters.end(),
        [parameter](const ParameterDescriptor& candidate) { return candidate.name == parameter; });
}

[[nodiscard]] std::optional<std::string> validate_locks(
    const OperatorRegistry& registry,
    const CrossoverParent& parent) {
    for (const InstanceId id : parent.locks.operators) {
        if (find_instance(parent.genome, id) == nullptr) {
            return std::string{"whole-operator crossover lock targets an absent instance"};
        }
    }
    for (const MutationParameterLock& lock : parent.locks.parameters) {
        const OperatorInstance* instance = find_instance(parent.genome, lock.instance_id);
        if (instance == nullptr) {
            return std::string{"parameter crossover lock targets an absent instance"};
        }
        if (!parameter_declared(registry, *instance, lock.parameter)) {
            return std::string{"parameter crossover lock targets an undeclared parameter"};
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> compatible_index(
    const Genome& parent,
    const OperatorInstance& primary,
    const std::size_t primary_index,
    const std::vector<bool>& used) {
    for (std::size_t index = 0U; index < parent.operators.size(); ++index) {
        if (!used[index] && parent.operators[index].instance_id == primary.instance_id &&
            parent.operators[index].type_id == primary.type_id) {
            return index;
        }
    }

    std::optional<std::size_t> best;
    std::size_t best_distance = 0U;
    for (std::size_t index = 0U; index < parent.operators.size(); ++index) {
        if (used[index] || parent.operators[index].type_id != primary.type_id) continue;
        const std::size_t distance = index > primary_index ? index - primary_index : primary_index - index;
        if (!best.has_value() || distance < best_distance) {
            best = index;
            best_distance = distance;
        }
    }
    return best;
}

[[nodiscard]] bool instance_id_exists(const Genome& genome, const InstanceId id) noexcept {
    return find_instance(genome, id) != nullptr;
}

[[nodiscard]] InstanceId unique_child_id(
    const Genome& genome,
    const RootSeed seed,
    const InstanceId source_id,
    const std::uint64_t base_ordinal) noexcept {
    for (std::uint64_t attempt = 0U;; ++attempt) {
        const std::uint64_t ordinal = base_ordinal + attempt;
        const InstanceId candidate = derive_instance_id(seed, source_id, "crossover-unmatched", ordinal);
        if (!instance_id_exists(genome, candidate)) return candidate;
    }
}

[[nodiscard]] std::size_t select_candidate(
    const RootSeed seed,
    const InstanceId primary_id,
    const std::string_view purpose,
    const std::size_t primary_index,
    const std::uint64_t parameter_tag,
    const std::size_t candidate_count) {
    const std::uint64_t words[]{
        static_cast<std::uint64_t>(primary_index),
        parameter_tag,
        static_cast<std::uint64_t>(candidate_count),
    };
    DeterministicStream stream = make_named_stream(seed, primary_id, purpose, words);
    return static_cast<std::size_t>(stream.uniform_below(static_cast<std::uint64_t>(candidate_count)));
}

}  // namespace

CrossoverResult crossover_genomes(
    const OperatorRegistry& registry,
    const CrossoverRequest& request) {
    CrossoverProvenance provenance;
    provenance.crossover_seed = request.crossover_seed;

    if (request.parents.size() < 2U || request.parents.size() > kMaximumCrossoverParents) {
        return fail_result(
            CrossoverErrorCode::invalid_parent_count,
            "crossover policy v1 requires 2..8 ordered parents",
            std::move(provenance));
    }

    provenance.parent_genome_identities.reserve(request.parents.size());
    for (std::size_t parent_index = 0U; parent_index < request.parents.size(); ++parent_index) {
        const CrossoverParent& parent = request.parents[parent_index];
        if (const auto validation = validate_genome(parent.genome, registry); validation.has_value()) {
            return fail_result(
                CrossoverErrorCode::invalid_parent,
                "crossover parent " + std::to_string(parent_index) + " is invalid: " + validation->message,
                std::move(provenance));
        }
        if (const auto lock_error = validate_locks(registry, parent); lock_error.has_value()) {
            return fail_result(
                CrossoverErrorCode::invalid_lock,
                "crossover parent " + std::to_string(parent_index) + " has invalid protection state: " + *lock_error,
                std::move(provenance));
        }
        provenance.parent_genome_identities.push_back(genome_identity_hex(parent.genome));
    }

    const CrossoverParent& primary_parent = request.parents.front();
    Genome child = primary_parent.genome;

    std::vector<std::vector<bool>> used;
    used.reserve(request.parents.size());
    used.emplace_back(primary_parent.genome.operators.size(), true);
    for (std::size_t parent_index = 1U; parent_index < request.parents.size(); ++parent_index) {
        used.emplace_back(request.parents[parent_index].genome.operators.size(), false);
    }

    for (std::size_t primary_index = 0U; primary_index < primary_parent.genome.operators.size(); ++primary_index) {
        const OperatorInstance& primary = primary_parent.genome.operators[primary_index];
        OperatorInstance& output = child.operators[primary_index];
        if (operator_locked(primary_parent.locks, primary.instance_id)) {
            continue;
        }

        std::vector<const OperatorInstance*> candidates;
        candidates.push_back(&primary);
        for (std::size_t parent_index = 1U; parent_index < request.parents.size(); ++parent_index) {
            const auto match = compatible_index(
                request.parents[parent_index].genome,
                primary,
                primary_index,
                used[parent_index]);
            if (!match.has_value()) continue;
            used[parent_index][*match] = true;
            candidates.push_back(&request.parents[parent_index].genome.operators[*match]);
        }

        if (candidates.size() > 1U) {
            const std::size_t enabled_source = select_candidate(
                request.crossover_seed,
                primary.instance_id,
                "crossover-enabled",
                primary_index,
                0U,
                candidates.size());
            output.enabled = candidates[enabled_source]->enabled;
        }

        const OperatorDescriptor* descriptor = registry.find(primary.type_id);
        if (descriptor == nullptr) {
            return fail_result(
                CrossoverErrorCode::invalid_parent,
                "primary crossover operator is absent from the registry",
                std::move(provenance));
        }
        for (const ParameterDescriptor& parameter : descriptor->parameters) {
            if (parameter_locked(primary_parent.locks, primary.instance_id, parameter.name)) continue;
            std::vector<const ParameterValue*> values;
            for (const OperatorInstance* candidate : candidates) {
                const auto found = candidate->parameters.find(parameter.name);
                if (found != candidate->parameters.end() && parameter_kind(found->second) == parameter.kind) {
                    values.push_back(&found->second);
                }
            }
            if (values.size() <= 1U) continue;
            const std::size_t source = select_candidate(
                request.crossover_seed,
                primary.instance_id,
                "crossover-parameter",
                primary_index,
                stable_tag_hash(parameter.name),
                values.size());
            output.parameters[parameter.name] = *values[source];
        }
    }

    // Unmatched operators are never inserted between primary operators. This
    // keeps every primary whole-operator lock at its exact stack index. Each
    // unmatched secondary operator is independently included by seed; a
    // whole-operator lock on that secondary forces inclusion as an indivisible
    // unit. Inserted instances receive deterministic child IDs.
    for (std::size_t parent_index = 1U; parent_index < request.parents.size(); ++parent_index) {
        const CrossoverParent& parent = request.parents[parent_index];
        for (std::size_t source_index = 0U; source_index < parent.genome.operators.size(); ++source_index) {
            if (used[parent_index][source_index]) continue;
            const OperatorInstance& source = parent.genome.operators[source_index];
            const bool forced = operator_locked(parent.locks, source.instance_id);
            const std::uint64_t words[]{
                static_cast<std::uint64_t>(parent_index),
                static_cast<std::uint64_t>(source_index),
            };
            DeterministicStream stream = make_named_stream(
                request.crossover_seed,
                source.instance_id,
                "crossover-unmatched-include",
                words);
            const bool include = forced || stream.uniform_below(2U) != 0U;
            if (!include) continue;

            if (child.operators.size() >= kMaximumMutationOperators) {
                if (forced) {
                    return fail_result(
                        CrossoverErrorCode::invalid_candidate,
                        "a protected unmatched operator cannot be inherited without exceeding the 64-operator limit",
                        std::move(provenance));
                }
                continue;
            }

            OperatorInstance inherited = source;
            const std::uint64_t ordinal =
                (static_cast<std::uint64_t>(parent_index) << 32U) |
                static_cast<std::uint64_t>(source_index);
            inherited.instance_id = unique_child_id(
                child, request.crossover_seed, source.instance_id, ordinal);
            child.operators.push_back(std::move(inherited));
        }
    }

    if (const auto validation = validate_genome(child, registry); validation.has_value()) {
        return fail_result(
            CrossoverErrorCode::invalid_candidate,
            "typed crossover produced a registry-invalid candidate: " + validation->message,
            std::move(provenance));
    }

    CrossoverResult result;
    result.changed = child != primary_parent.genome;
    result.topology_changed = child.operators.size() != primary_parent.genome.operators.size();
    result.genome = std::move(child);
    result.provenance = std::move(provenance);
    return result;
}

}  // namespace faultmine::core
