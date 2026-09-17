#include "faultmine/crossover.hpp"

#include "faultmine/determinism.hpp"
#include "faultmine/mutation.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

struct ParentView {
    const CrossoverParent* parent{};
    std::string identity;
};

[[nodiscard]] const OperatorInstance* find_instance(
    const Genome& genome,
    const InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
}

[[nodiscard]] bool contains_id(const std::vector<InstanceId>& ids, const InstanceId id) noexcept {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

[[nodiscard]] bool whole_locked(const CrossoverLocks& locks, const InstanceId id) noexcept {
    return contains_id(locks.operators, id);
}

[[nodiscard]] bool parameter_locked(
    const CrossoverLocks& locks,
    const InstanceId id,
    const std::string_view parameter) noexcept {
    return std::any_of(
        locks.parameters.begin(), locks.parameters.end(),
        [id, parameter](const CrossoverParameterLock& lock) {
            return lock.instance_id == id && lock.parameter == parameter;
        });
}

[[nodiscard]] const ParameterDescriptor* find_parameter_descriptor(
    const OperatorRegistry& registry,
    const OperatorInstance& instance,
    const std::string_view name) noexcept {
    const OperatorDescriptor* descriptor = registry.find(instance.type_id);
    if (descriptor == nullptr) return nullptr;
    const auto found = std::find_if(
        descriptor->parameters.begin(), descriptor->parameters.end(),
        [name](const ParameterDescriptor& parameter) { return parameter.name == name; });
    return found == descriptor->parameters.end() ? nullptr : &*found;
}

[[nodiscard]] std::optional<CrossoverError> validate_locks(
    const CrossoverParent& parent,
    const OperatorRegistry& registry) {
    for (const InstanceId id : parent.locks.operators) {
        if (find_instance(parent.genome, id) == nullptr) {
            return CrossoverError{CrossoverErrorCode::invalid_lock, "whole-operator crossover lock targets an absent instance"};
        }
    }
    for (const CrossoverParameterLock& lock : parent.locks.parameters) {
        const OperatorInstance* instance = find_instance(parent.genome, lock.instance_id);
        if (instance == nullptr || find_parameter_descriptor(registry, *instance, lock.parameter) == nullptr) {
            return CrossoverError{CrossoverErrorCode::invalid_lock, "parameter crossover lock targets an absent instance or parameter"};
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool same_operator_semantics(
    const OperatorInstance& left,
    const OperatorInstance& right) noexcept {
    return left.type_id == right.type_id &&
        left.type_version == right.type_version &&
        left.enabled == right.enabled &&
        left.parameters == right.parameters;
}

[[nodiscard]] std::size_t choose_index(
    const RootSeed seed,
    const InstanceId anchor,
    const std::string_view purpose,
    const std::uint64_t slot,
    const std::uint64_t detail,
    const std::size_t count) {
    const std::array<std::uint64_t, 3U> words{
        slot,
        detail,
        static_cast<std::uint64_t>(count),
    };
    DeterministicStream stream = make_named_stream(seed, anchor, purpose, words);
    return static_cast<std::size_t>(stream.uniform_below(static_cast<std::uint64_t>(count)));
}

[[nodiscard]] InstanceId unique_derived_id(
    const RootSeed seed,
    const InstanceId anchor,
    const std::string_view purpose,
    const std::uint64_t slot,
    const std::vector<OperatorInstance>& existing) noexcept {
    for (std::uint64_t attempt = 0U;; ++attempt) {
        const std::uint64_t ordinal = mix64(slot ^ (attempt * 0x9e3779b97f4a7c15ULL));
        const InstanceId candidate = derive_instance_id(seed, anchor, purpose, ordinal);
        const bool collision = std::any_of(
            existing.begin(), existing.end(),
            [candidate](const OperatorInstance& instance) { return instance.instance_id == candidate; });
        if (!collision) return candidate;
    }
}

[[nodiscard]] bool id_in_child(const Genome& child, const InstanceId id) noexcept {
    return std::any_of(
        child.operators.begin(), child.operators.end(),
        [id](const OperatorInstance& instance) { return instance.instance_id == id; });
}

[[nodiscard]] bool parameter_lock_in_result(
    const CrossoverLocks& locks,
    const InstanceId id,
    const std::string_view name) noexcept {
    return std::any_of(
        locks.parameters.begin(), locks.parameters.end(),
        [id, name](const CrossoverParameterLock& lock) {
            return lock.instance_id == id && lock.parameter == name;
        });
}

void add_parameter_lock(CrossoverLocks& locks, const InstanceId id, const std::string& name) {
    if (!parameter_lock_in_result(locks, id, name)) {
        locks.parameters.push_back(CrossoverParameterLock{id, name});
    }
}

[[nodiscard]] CrossoverResult fail(
    const CrossoverErrorCode code,
    std::string message,
    CrossoverProvenance provenance = {}) {
    CrossoverResult result;
    result.error = CrossoverError{code, std::move(message)};
    result.provenance = std::move(provenance);
    return result;
}

}  // namespace

CrossoverResult crossover_genomes(
    const std::vector<CrossoverParent>& parents,
    const OperatorRegistry& registry,
    const CrossoverRequest& request) {
    CrossoverProvenance provenance;
    provenance.crossover_policy_version = request.policy_version;
    provenance.crossover_seed = request.crossover_seed;

    if (request.policy_version != kCrossoverPolicyVersion) {
        return fail(CrossoverErrorCode::unsupported_policy, "unsupported crossover policy version", std::move(provenance));
    }
    if (parents.size() < 2U || parents.size() > kMaximumCrossoverParents) {
        return fail(CrossoverErrorCode::invalid_parent_count, "crossover requires 2..8 parents", std::move(provenance));
    }

    std::vector<ParentView> ordered;
    ordered.reserve(parents.size());
    for (const CrossoverParent& parent : parents) {
        if (const auto error = validate_genome(parent.genome, registry); error.has_value()) {
            return fail(CrossoverErrorCode::invalid_parent, "crossover parent is not registry-valid: " + error->message, std::move(provenance));
        }
        if (const auto lock_error = validate_locks(parent, registry); lock_error.has_value()) {
            return fail(lock_error->code, lock_error->message, std::move(provenance));
        }
        ordered.push_back(ParentView{&parent, genome_identity_hex(parent.genome)});
    }
    std::sort(ordered.begin(), ordered.end(), [](const ParentView& left, const ParentView& right) {
        return left.identity < right.identity;
    });
    for (std::size_t index = 1U; index < ordered.size(); ++index) {
        if (ordered[index - 1U].identity == ordered[index].identity) {
            return fail(CrossoverErrorCode::duplicate_parent, "crossover parent set contains the same canonical genome more than once", std::move(provenance));
        }
    }
    for (const ParentView& parent : ordered) provenance.parent_genome_identities.push_back(parent.identity);

    Genome child;
    child.schema_version = ordered.front().parent->genome.schema_version;
    child.engine_contract_version = ordered.front().parent->genome.engine_contract_version;
    child.root_seed = request.crossover_seed;

    std::vector<std::vector<bool>> used;
    used.reserve(ordered.size());
    for (const ParentView& parent : ordered) {
        used.emplace_back(parent.parent->genome.operators.size(), false);
    }

    CrossoverLocks inherited;
    const Genome& backbone = ordered.front().parent->genome;
    for (std::size_t slot = 0U; slot < backbone.operators.size(); ++slot) {
        const OperatorInstance& anchor = backbone.operators[slot];
        struct Candidate {
            std::size_t parent_index{};
            std::size_t operator_index{};
            const OperatorInstance* instance{};
        };
        std::vector<Candidate> candidates;
        candidates.push_back(Candidate{0U, slot, &anchor});
        used[0U][slot] = true;

        for (std::size_t parent_index = 1U; parent_index < ordered.size(); ++parent_index) {
            const Genome& genome = ordered[parent_index].parent->genome;
            std::optional<std::size_t> match;
            for (std::size_t op = 0U; op < genome.operators.size(); ++op) {
                const OperatorInstance& candidate = genome.operators[op];
                if (!used[parent_index][op] && candidate.instance_id == anchor.instance_id &&
                    candidate.type_id == anchor.type_id && candidate.type_version == anchor.type_version) {
                    match = op;
                    break;
                }
            }
            if (!match.has_value() && slot < genome.operators.size()) {
                const OperatorInstance& candidate = genome.operators[slot];
                if (!used[parent_index][slot] && candidate.type_id == anchor.type_id &&
                    candidate.type_version == anchor.type_version) {
                    match = slot;
                }
            }
            if (!match.has_value()) {
                for (std::size_t op = 0U; op < genome.operators.size(); ++op) {
                    const OperatorInstance& candidate = genome.operators[op];
                    if (!used[parent_index][op] && candidate.type_id == anchor.type_id &&
                        candidate.type_version == anchor.type_version) {
                        match = op;
                        break;
                    }
                }
            }
            if (match.has_value()) {
                used[parent_index][*match] = true;
                candidates.push_back(Candidate{parent_index, *match, &genome.operators[*match]});
            }
        }

        std::vector<const Candidate*> whole_protected;
        for (const Candidate& candidate : candidates) {
            if (whole_locked(ordered[candidate.parent_index].parent->locks, candidate.instance->instance_id)) {
                whole_protected.push_back(&candidate);
            }
        }

        OperatorInstance output;
        bool preserve_locked_identity = false;
        if (!whole_protected.empty()) {
            const OperatorInstance& protected_value = *whole_protected.front()->instance;
            for (const Candidate* candidate : whole_protected) {
                if (!same_operator_semantics(protected_value, *candidate->instance)) {
                    return fail(CrossoverErrorCode::protected_conflict, "aligned whole-operator locks disagree and cannot both be preserved", std::move(provenance));
                }
            }
            output = protected_value;
            preserve_locked_identity = true;
        } else {
            output = anchor;
            const std::size_t enabled_pick = choose_index(
                request.crossover_seed, anchor.instance_id, "crossover-enabled",
                static_cast<std::uint64_t>(slot), 0U, candidates.size());
            output.enabled = candidates[enabled_pick].instance->enabled;

            const OperatorDescriptor* descriptor = registry.find(anchor.type_id);
            if (descriptor == nullptr) {
                return fail(CrossoverErrorCode::invalid_parent, "aligned operator descriptor disappeared during crossover", std::move(provenance));
            }
            for (const ParameterDescriptor& parameter : descriptor->parameters) {
                std::vector<const Candidate*> protected_values;
                for (const Candidate& candidate : candidates) {
                    if (parameter_locked(
                            ordered[candidate.parent_index].parent->locks,
                            candidate.instance->instance_id,
                            parameter.name)) {
                        protected_values.push_back(&candidate);
                    }
                }
                ParameterValue chosen;
                bool has_value = false;
                if (!protected_values.empty()) {
                    const auto first_value = protected_values.front()->instance->parameters.find(parameter.name);
                    if (first_value == protected_values.front()->instance->parameters.end()) {
                        return fail(CrossoverErrorCode::invalid_parent, "protected crossover parameter is missing", std::move(provenance));
                    }
                    chosen = first_value->second;
                    has_value = true;
                    for (const Candidate* candidate : protected_values) {
                        const auto value = candidate->instance->parameters.find(parameter.name);
                        if (value == candidate->instance->parameters.end() || value->second != chosen) {
                            return fail(CrossoverErrorCode::protected_conflict, "aligned parameter locks disagree and cannot both be preserved", std::move(provenance));
                        }
                    }
                } else {
                    const std::size_t value_pick = choose_index(
                        request.crossover_seed, anchor.instance_id, "crossover-parameter",
                        static_cast<std::uint64_t>(slot), stable_tag_hash(parameter.name), candidates.size());
                    const auto value = candidates[value_pick].instance->parameters.find(parameter.name);
                    if (value != candidates[value_pick].instance->parameters.end()) {
                        chosen = value->second;
                        has_value = true;
                    }
                }
                if (has_value) output.parameters[parameter.name] = std::move(chosen);
            }
        }

        const bool identical_ids = std::all_of(
            candidates.begin(), candidates.end(),
            [&anchor](const Candidate& candidate) { return candidate.instance->instance_id == anchor.instance_id; });
        if (!preserve_locked_identity && !identical_ids) {
            output.instance_id = unique_derived_id(
                request.crossover_seed, anchor.instance_id, "crossover-aligned-operator",
                static_cast<std::uint64_t>(slot), child.operators);
        }
        if (id_in_child(child, output.instance_id)) {
            if (preserve_locked_identity) {
                return fail(CrossoverErrorCode::protected_conflict, "protected operator identity collides in crossover child", std::move(provenance));
            }
            output.instance_id = unique_derived_id(
                request.crossover_seed, anchor.instance_id, "crossover-collision",
                static_cast<std::uint64_t>(slot), child.operators);
        }

        if (!whole_protected.empty()) inherited.operators.push_back(output.instance_id);
        for (const ParameterDescriptor& parameter : registry.find(output.type_id)->parameters) {
            bool protected_parameter = false;
            for (const Candidate& candidate : candidates) {
                if (parameter_locked(
                        ordered[candidate.parent_index].parent->locks,
                        candidate.instance->instance_id,
                        parameter.name)) {
                    protected_parameter = true;
                    break;
                }
            }
            if (protected_parameter) add_parameter_lock(inherited, output.instance_id, parameter.name);
        }
        child.operators.push_back(std::move(output));
    }

    for (std::size_t parent_index = 1U; parent_index < ordered.size(); ++parent_index) {
        const Genome& genome = ordered[parent_index].parent->genome;
        const CrossoverLocks& locks = ordered[parent_index].parent->locks;
        for (std::size_t op = 0U; op < genome.operators.size(); ++op) {
            if (used[parent_index][op]) continue;
            const OperatorInstance& source = genome.operators[op];
            const bool protected_operator = whole_locked(locks, source.instance_id);
            const bool has_parameter_lock = std::any_of(
                locks.parameters.begin(), locks.parameters.end(),
                [&source](const CrossoverParameterLock& lock) { return lock.instance_id == source.instance_id; });
            bool include = protected_operator || has_parameter_lock;
            if (!include) {
                const std::array<std::uint64_t, 3U> words{
                    static_cast<std::uint64_t>(parent_index),
                    static_cast<std::uint64_t>(op),
                    stable_tag_hash(source.type_id),
                };
                DeterministicStream stream = make_named_stream(
                    request.crossover_seed, source.instance_id, "crossover-unmatched-include", words);
                include = stream.uniform_below(2U) != 0U;
            }
            if (!include) continue;
            if (child.operators.size() >= kMaximumMutationOperators) {
                return fail(CrossoverErrorCode::operator_limit, "crossover child exceeds the 64-operator v1 limit", std::move(provenance));
            }

            OperatorInstance output = source;
            if (id_in_child(child, output.instance_id)) {
                if (protected_operator || has_parameter_lock) {
                    return fail(CrossoverErrorCode::protected_conflict, "protected unmatched operator identity collides in crossover child", std::move(provenance));
                }
                output.instance_id = unique_derived_id(
                    request.crossover_seed, source.instance_id, "crossover-unmatched-collision",
                    (static_cast<std::uint64_t>(parent_index) << 32U) | static_cast<std::uint64_t>(op),
                    child.operators);
            }
            if (protected_operator) inherited.operators.push_back(output.instance_id);
            for (const CrossoverParameterLock& lock : locks.parameters) {
                if (lock.instance_id == source.instance_id) add_parameter_lock(inherited, output.instance_id, lock.parameter);
            }
            child.operators.push_back(std::move(output));
        }
    }

    if (const auto error = validate_genome(child, registry); error.has_value()) {
        return fail(CrossoverErrorCode::invalid_child, "crossover child is not registry-valid: " + error->message, std::move(provenance));
    }

    provenance.child_genome_identity = genome_identity_hex(child);
    CrossoverResult result;
    result.genome = std::move(child);
    result.provenance = std::move(provenance);
    result.inherited_locks = std::move(inherited);
    return result;
}

}  // namespace faultmine::core
