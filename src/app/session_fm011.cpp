#include "faultmine/session.hpp"

#include "faultmine/temporal_timeline.hpp"

#define ensure_preview ensure_preview_legacy
#define render_full render_full_legacy
#include "session.cpp"
#undef render_full
#undef ensure_preview

namespace faultmine::app {

bool SessionModel::ensure_preview(std::string* error) {
    if (!preview_dirty_ && preview_result_) return true;
    const core::ImageBuffer* input = choose_preview_source(error);
    if (input == nullptr) return false;
    core::RenderResult rendered = core::render_pipeline_at_frame(
        *input, editor_.genome(), editor_.registry(), current_frame_);
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error ? pipeline_error_text(*rendered.error) : std::string{"temporal pipeline render failed"};
        }
        return false;
    }
    preview_result_ = std::move(*rendered.image);
    preview_dirty_ = false;
    ++render_generation_;
    return true;
}

std::optional<core::ImageBuffer> SessionModel::render_full(std::string* error) const {
    return render_full_at_frame(current_frame_, error);
}

std::optional<core::ImageBuffer> SessionModel::render_full_at_frame(
    const std::uint64_t frame_index,
    std::string* error) const {
    if (!source_) {
        if (error != nullptr) *error = "no source image is loaded";
        return std::nullopt;
    }
    core::RenderResult rendered = core::render_pipeline_at_frame(
        *source_, editor_.genome(), editor_.registry(), frame_index);
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error ? pipeline_error_text(*rendered.error) : std::string{"full-resolution temporal pipeline render failed"};
        }
        return std::nullopt;
    }
    return std::move(*rendered.image);
}

void SessionModel::seek_frame(const std::uint64_t frame_index) noexcept {
    if (current_frame_ == frame_index) return;
    current_frame_ = frame_index;
    mark_preview_dirty();
}

bool SessionModel::step_frame_forward() noexcept {
    if (current_frame_ == std::numeric_limits<std::uint64_t>::max()) return false;
    ++current_frame_;
    mark_preview_dirty();
    return true;
}

bool SessionModel::step_frame_backward() noexcept {
    if (current_frame_ == 0U) return false;
    --current_frame_;
    mark_preview_dirty();
    return true;
}

void SessionModel::reset_timeline() noexcept {
    seek_frame(0U);
}

std::uint64_t SessionModel::current_frame() const noexcept {
    return current_frame_;
}

TimelineRate SessionModel::semantic_timeline_rate() const noexcept {
    for (const core::OperatorInstance& instance : editor_.genome().operators) {
        if (!instance.enabled || instance.type_id != core::kFaultTimelineRate) continue;
        const auto numerator = instance.parameters.find("rate_num");
        const auto denominator = instance.parameters.find("rate_den");
        const auto* num = numerator == instance.parameters.end() ? nullptr : std::get_if<std::uint64_t>(&numerator->second);
        const auto* den = denominator == instance.parameters.end() ? nullptr : std::get_if<std::uint64_t>(&denominator->second);
        if (num != nullptr && den != nullptr && *num != 0U && *den != 0U) {
            return TimelineRate{*num, *den};
        }
    }
    return TimelineRate{};
}

void SessionModel::set_preview_rate_milli(const std::uint32_t rate_milli) noexcept {
    preview_rate_milli_ = std::clamp<std::uint32_t>(rate_milli, 125U, 8000U);
}

std::uint32_t SessionModel::preview_rate_milli() const noexcept {
    return preview_rate_milli_;
}

}  // namespace faultmine::app
