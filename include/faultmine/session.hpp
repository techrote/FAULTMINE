#pragma once

#include "faultmine/crossover.hpp"
#include "faultmine/editor.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/project.hpp"
#include "faultmine/proxy.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::app {

enum class ViewMode {
    fit,
    actual_size,
    custom,
};

struct CanvasViewState {
    ViewMode mode{ViewMode::fit};
    double zoom{1.0};
    double pan_x{};
    double pan_y{};
    bool show_before{};
};

struct PreviewState {
    std::uint32_t width{};
    std::uint32_t height{};
    bool is_proxy{};
    std::string proxy_key;
    std::uint64_t render_generation{};
};

class SessionModel {
public:
    SessionModel();

    [[nodiscard]] bool set_source(
        core::ImageBuffer image,
        std::string source_identity,
        std::filesystem::path source_path,
        std::string* error = nullptr);
    [[nodiscard]] bool load_project_state(
        const ProjectDocument& project,
        core::ImageBuffer source,
        std::filesystem::path resolved_source_path,
        std::string* error = nullptr);
    [[nodiscard]] std::optional<ProjectDocument> make_project_document(std::string* error = nullptr) const;

    [[nodiscard]] bool has_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* full_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* preview_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* preview_result() const noexcept;
    [[nodiscard]] const core::ImageBuffer* display_image() const noexcept;
    [[nodiscard]] const std::string& source_identity() const noexcept;
    [[nodiscard]] const std::filesystem::path& source_path() const noexcept;

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
        bool coalesce_history = false);
    void end_coalesced_edit() noexcept;
    [[nodiscard]] EditResult toggle_operator_lock(std::size_t operator_index);
    [[nodiscard]] EditResult toggle_parameter_lock(std::size_t operator_index, std::string_view parameter_name);
    [[nodiscard]] bool operator_locked(std::size_t operator_index) const noexcept;
    [[nodiscard]] bool parameter_locked(std::size_t operator_index, std::string_view parameter_name) const noexcept;
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool can_undo() const noexcept;
    [[nodiscard]] bool can_redo() const noexcept;
    [[nodiscard]] bool project_dirty() const noexcept;
    void mark_project_saved() noexcept;

    // Compatibility promotion for callers without derivation metadata. It is
    // retained as a history-free manual root rather than fabricating ancestry.
    [[nodiscard]] bool promote_exploration_genome(
        const core::Genome& genome,
        std::string* error = nullptr);

    [[nodiscard]] bool retain_mutation_specimen(
        const core::Genome& genome,
        const core::DescendantProvenance& provenance,
        bool favourite,
        std::string* error = nullptr);
    [[nodiscard]] bool promote_mutation_specimen(
        const core::Genome& genome,
        const core::DescendantProvenance& provenance,
        bool favourite,
        std::string* error = nullptr);
    [[nodiscard]] bool breed_and_promote(
        const std::vector<core::Genome>& ordered_parents,
        core::RootSeed crossover_seed,
        std::string* error = nullptr);
    [[nodiscard]] bool activate_lineage_specimen(
        std::string_view genome_identity,
        std::string* error = nullptr);
    [[nodiscard]] bool set_specimen_favourite(
        std::string_view genome_identity,
        bool favourite,
        std::string* error = nullptr);
    [[nodiscard]] const LineageGraph& lineage() const noexcept;
    [[nodiscard]] std::string active_provenance_summary() const;

    void set_selected_operator(std::optional<std::size_t> operator_index) noexcept;
    [[nodiscard]] std::optional<std::size_t> selected_operator() const noexcept;

    [[nodiscard]] bool ensure_preview(std::string* error = nullptr);
    [[nodiscard]] std::optional<core::ImageBuffer> render_full(std::string* error = nullptr) const;
    [[nodiscard]] PreviewState preview_state() const;
    [[nodiscard]] std::uint64_t render_generation() const noexcept;
    [[nodiscard]] bool preview_dirty() const noexcept;

    void set_proxy_enabled(bool enabled);
    [[nodiscard]] bool proxy_enabled() const noexcept;
    void set_proxy_spec(core::ProxySpec spec);
    [[nodiscard]] const core::ProxySpec& proxy_spec() const noexcept;

    void toggle_effects();
    [[nodiscard]] bool effects_enabled() const noexcept;
    void adjust_row_offset(std::int64_t delta);
    void adjust_jitter(std::int64_t delta);
    void reroll_seed();
    [[nodiscard]] std::int64_t row_offset_amount() const noexcept;
    [[nodiscard]] std::uint64_t jitter_max_shift() const noexcept;

    void set_fit_view() noexcept;
    void set_actual_view() noexcept;
    void zoom_by(double factor) noexcept;
    void pan_by(double dx, double dy) noexcept;
    void reset_pan() noexcept;
    void toggle_before() noexcept;
    [[nodiscard]] const CanvasViewState& view_state() const noexcept;

private:
    [[nodiscard]] static core::Genome make_default_genome();
    void mark_preview_dirty() noexcept;
    [[nodiscard]] const core::ImageBuffer* choose_preview_source(std::string* error);
    void after_semantic_edit(const EditResult& result) noexcept;
    [[nodiscard]] std::optional<std::size_t> find_operator_type(std::string_view type_id) const noexcept;
    [[nodiscard]] bool ensure_current_lineage_root(std::string* error = nullptr);
    [[nodiscard]] bool adopt_exploration_genome(const core::Genome& genome, std::string* error = nullptr);

    EditorModel editor_;
    std::optional<core::ImageBuffer> source_;
    std::string source_identity_;
    std::filesystem::path source_path_;

    LineageGraph lineage_;
    bool lineage_detached_{true};

    core::ProxySpec proxy_spec_{};
    bool proxy_enabled_{true};
    std::optional<core::ProxyImage> proxy_cache_;
    std::optional<core::ImageBuffer> preview_result_;
    bool preview_dirty_{true};
    bool preview_is_proxy_{};
    std::string active_proxy_key_;
    std::uint64_t render_generation_{};

    CanvasViewState view_{};
    std::string selected_instance_id_;
};

}  // namespace faultmine::app
