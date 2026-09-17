#pragma once

#include "faultmine/fault_catalogue.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::app {

struct ParameterLock {
    core::InstanceId instance_id{};
    std::string parameter;

    bool operator==(const ParameterLock&) const = default;
};

struct LockState {
    std::vector<core::InstanceId> operators;
    std::vector<ParameterLock> parameters;

    bool operator==(const LockState&) const = default;
};

enum class EditErrorCode {
    invalid_index,
    unknown_operator,
    unknown_parameter,
    wrong_type,
    invalid_value,
    invalid_genome,
};

struct EditError {
    EditErrorCode code{EditErrorCode::invalid_value};
    std::string message;
};

struct EditResult {
    std::optional<EditError> error;

    [[nodiscard]] bool ok() const noexcept {
        return !error.has_value();
    }
};

class EditorModel {
public:
    EditorModel();
    explicit EditorModel(core::Genome genome);

    [[nodiscard]] const core::FaultRegistry& registry() const noexcept;
    [[nodiscard]] const core::Genome& genome() const noexcept;
    [[nodiscard]] std::string genome_identity() const;
    [[nodiscard]] const LockState& locks() const noexcept;

    [[nodiscard]] const std::vector<core::OperatorDescriptor>& operator_descriptors() const noexcept;
    [[nodiscard]] const core::OperatorDescriptor* descriptor_for_operator(std::size_t operator_index) const noexcept;
    [[nodiscard]] const core::ParameterDescriptor* descriptor_for_parameter(
        std::size_t operator_index,
        std::string_view parameter_name) const noexcept;

    [[nodiscard]] EditResult add_operator(std::string_view type_id, std::size_t insert_index);
    [[nodiscard]] EditResult remove_operator(std::size_t operator_index);
    [[nodiscard]] EditResult duplicate_operator(std::size_t operator_index);
    [[nodiscard]] EditResult move_operator(std::size_t from_index, std::size_t to_index);
    [[nodiscard]] EditResult toggle_operator_enabled(std::size_t operator_index);
    [[nodiscard]] EditResult set_parameter_from_text(
        std::size_t operator_index,
        std::string_view parameter_name,
        std::string_view text);
    [[nodiscard]] EditResult nudge_parameter(
        std::size_t operator_index,
        std::string_view parameter_name,
        int direction,
        bool large_step,
        bool coalesce_history);
    void end_coalesced_edit() noexcept;

    [[nodiscard]] EditResult toggle_operator_lock(std::size_t operator_index);
    [[nodiscard]] EditResult toggle_parameter_lock(
        std::size_t operator_index,
        std::string_view parameter_name);
    [[nodiscard]] bool operator_locked(std::size_t operator_index) const noexcept;
    [[nodiscard]] bool parameter_locked(
        std::size_t operator_index,
        std::string_view parameter_name) const noexcept;

    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    void clear_history() noexcept;

    [[nodiscard]] bool project_dirty() const noexcept;
    void mark_saved() noexcept;
    void mark_external_change() noexcept;

    [[nodiscard]] EditResult replace_state(
        core::Genome genome,
        LockState locks,
        bool clear_history_boundary = true);
    [[nodiscard]] EditResult set_all_enabled(bool enabled);
    [[nodiscard]] EditResult reroll_seed();

private:
    struct Snapshot {
        core::Genome genome;
        LockState locks;
    };

    [[nodiscard]] Snapshot snapshot() const;
    void restore_snapshot(Snapshot snapshot);
    void begin_edit();
    void begin_coalesced_edit(std::string key);
    [[nodiscard]] bool instance_id_exists(core::InstanceId id) const noexcept;
    [[nodiscard]] core::InstanceId derive_unique_instance_id(
        core::InstanceId parent,
        std::string_view purpose) const noexcept;
    void remove_locks_for_instance(core::InstanceId id);
    [[nodiscard]] EditResult validate_state(const core::Genome& genome, const LockState& locks) const;

    core::FaultRegistry registry_;
    core::Genome genome_;
    LockState locks_;
    std::vector<Snapshot> undo_;
    std::vector<Snapshot> redo_;
    std::string coalesce_key_;
    bool dirty_{};
};

[[nodiscard]] std::string parameter_value_to_text(const core::ParameterValue& value);

}  // namespace faultmine::app
