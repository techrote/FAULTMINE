#pragma once

#include "faultmine/genome.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/proxy.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

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

    [[nodiscard]] bool has_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* full_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* preview_source() const noexcept;
    [[nodiscard]] const core::ImageBuffer* preview_result() const noexcept;
    [[nodiscard]] const core::ImageBuffer* display_image() const noexcept;
    [[nodiscard]] const std::string& source_identity() const noexcept;
    [[nodiscard]] const std::filesystem::path& source_path() const noexcept;

    [[nodiscard]] const core::Genome& genome() const noexcept;
    [[nodiscard]] std::string genome_identity() const;

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
    void reroll_seed() noexcept;
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

    core::FaultRegistry registry_;
    core::Genome genome_;
    std::optional<core::ImageBuffer> source_;
    std::string source_identity_;
    std::filesystem::path source_path_;

    core::ProxySpec proxy_spec_{};
    bool proxy_enabled_{true};
    std::optional<core::ProxyImage> proxy_cache_;
    std::optional<core::ImageBuffer> preview_result_;
    bool preview_dirty_{true};
    bool preview_is_proxy_{};
    std::string active_proxy_key_;
    std::uint64_t render_generation_{};

    CanvasViewState view_{};
};

}  // namespace faultmine::app
