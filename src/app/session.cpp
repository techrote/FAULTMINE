#include "faultmine/session.hpp"

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace faultmine::app {
namespace {

core::OperatorInstance make_operator(const core::InstanceId id, const char* type_id) {
    core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type_id;
    instance.type_version = 1U;
    instance.enabled = true;
    return instance;
}

std::string pipeline_error_text(const core::PipelineError& error) {
    std::string text{"operator "};
    text += std::to_string(error.operator_index);
    if (!error.operator_type.empty()) {
        text += " (";
        text += error.operator_type;
        text += ')';
    }
    text += ": ";
    text += error.message;
    return text;
}

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string encoded = path.u8string();
    return std::string{
        reinterpret_cast<const char*>(encoded.data()),
        reinterpret_cast<const char*>(encoded.data()) + encoded.size()};
}

std::int64_t scaled_milli(const double value) noexcept {
    constexpr double kLimit = static_cast<double>(std::numeric_limits<std::int64_t>::max()) / 1000.0;
    const double bounded = std::clamp(value, -kLimit, kLimit);
    return static_cast<std::int64_t>(std::llround(bounded * 1000.0));
}

}  // namespace

SessionModel::SessionModel()
    : editor_(make_default_genome()) {}

core::Genome SessionModel::make_default_genome() {
    core::Genome genome;
    genome.root_seed = core::RootSeed{0x4641554c544d3034ULL};

    auto row = make_operator(
        core::InstanceId{0x4641554c544d0004ULL, 0x0000000000000001ULL},
        core::kFaultRowOffset);
    row.parameters.emplace("amount", core::ParameterValue{std::int64_t{3}});
    row.parameters.emplace("boundary", core::ParameterValue{std::string{"wrap"}});

    auto channels = make_operator(
        core::InstanceId{0x4641554c544d0004ULL, 0x0000000000000002ULL},
        core::kFaultChannelPermute);
    channels.parameters.emplace("order", core::ParameterValue{std::string{"bgra"}});

    auto xors = make_operator(
        core::InstanceId{0x4641554c544d0004ULL, 0x0000000000000003ULL},
        core::kFaultByteXor);
    xors.parameters.emplace("mask", core::ParameterValue{std::uint64_t{7U}});
    xors.parameters.emplace("channels", core::ParameterValue{std::string{"rb"}});

    auto jitter = make_operator(
        core::InstanceId{0x4641554c544d0004ULL, 0x0000000000000004ULL},
        core::kFaultScanlineJitter);
    jitter.parameters.emplace("max_shift", core::ParameterValue{std::uint64_t{2U}});
    jitter.parameters.emplace("boundary", core::ParameterValue{std::string{"wrap"}});

    genome.operators = {std::move(row), std::move(channels), std::move(xors), std::move(jitter)};
    return genome;
}

bool SessionModel::set_source(
    core::ImageBuffer image,
    std::string source_identity,
    std::filesystem::path source_path,
    std::string* error) {
    if (const auto validation = core::validate_canonical_image(image); validation.has_value()) {
        if (error != nullptr) {
            *error = validation->message;
        }
        return false;
    }
    const std::string actual_identity = core::source_identity_hex(image);
    if (source_identity != actual_identity) {
        if (error != nullptr) {
            *error = "source identity does not match canonical source bytes";
        }
        return false;
    }

    source_ = std::move(image);
    source_identity_ = std::move(source_identity);
    source_path_ = std::move(source_path);
    proxy_cache_.reset();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    editor_.mark_external_change();
    mark_preview_dirty();
    set_fit_view();
    return true;
}

bool SessionModel::load_project_state(
    const ProjectDocument& project,
    core::ImageBuffer source,
    std::filesystem::path resolved_source_path,
    std::string* error) {
    if (const auto validation = core::validate_canonical_image(source); validation.has_value()) {
        if (error != nullptr) {
            *error = validation->message;
        }
        return false;
    }
    const std::string actual_identity = core::source_identity_hex(source);
    if (actual_identity != project.source.source_identity) {
        if (error != nullptr) {
            *error = "resolved source content does not match the project source identity";
        }
        return false;
    }
    EditResult edit = editor_.replace_state(project.genome, project.locks, true);
    if (!edit.ok()) {
        if (error != nullptr) {
            *error = edit.error->message;
        }
        return false;
    }

    source_ = std::move(source);
    source_identity_ = actual_identity;
    source_path_ = std::move(resolved_source_path);
    proxy_spec_ = project.session.proxy_spec;
    proxy_enabled_ = project.session.proxy_enabled;
    proxy_cache_.reset();
    preview_result_.reset();
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    selected_instance_id_ = project.session.selected_instance_id;

    if (project.ui.mode == "actual") {
        view_.mode = ViewMode::actual_size;
    } else if (project.ui.mode == "custom") {
        view_.mode = ViewMode::custom;
    } else {
        view_.mode = ViewMode::fit;
    }
    view_.zoom = static_cast<double>(project.ui.zoom_milli) / 1000.0;
    view_.pan_x = static_cast<double>(project.ui.pan_x_milli) / 1000.0;
    view_.pan_y = static_cast<double>(project.ui.pan_y_milli) / 1000.0;
    view_.show_before = project.ui.show_before;
    mark_preview_dirty();
    return true;
}

std::optional<ProjectDocument> SessionModel::make_project_document(std::string* error) const {
    if (!source_.has_value()) {
        if (error != nullptr) {
            *error = "a project requires a loaded canonical source";
        }
        return std::nullopt;
    }
    ProjectDocument project;
    project.source.path_utf8 = path_to_utf8(source_path_);
    project.source.source_identity = source_identity_;
    project.genome = editor_.genome();
    project.locks = editor_.locks();
    project.session.proxy_enabled = proxy_enabled_;
    project.session.proxy_spec = proxy_spec_;
    project.session.selected_instance_id = selected_instance_id_;
    switch (view_.mode) {
        case ViewMode::fit: project.ui.mode = "fit"; break;
        case ViewMode::actual_size: project.ui.mode = "actual"; break;
        case ViewMode::custom: project.ui.mode = "custom"; break;
    }
    project.ui.zoom_milli = std::clamp<std::int64_t>(scaled_milli(view_.zoom), 50, 64000);
    project.ui.pan_x_milli = scaled_milli(view_.pan_x);
    project.ui.pan_y_milli = scaled_milli(view_.pan_y);
    project.ui.show_before = view_.show_before;
    return project;
}

bool SessionModel::has_source() const noexcept { return source_.has_value(); }
const core::ImageBuffer* SessionModel::full_source() const noexcept { return source_ ? &*source_ : nullptr; }
const core::ImageBuffer* SessionModel::preview_result() const noexcept { return preview_result_ ? &*preview_result_ : nullptr; }
const std::string& SessionModel::source_identity() const noexcept { return source_identity_; }
const std::filesystem::path& SessionModel::source_path() const noexcept { return source_path_; }
const core::FaultRegistry& SessionModel::registry() const noexcept { return editor_.registry(); }
const core::Genome& SessionModel::genome() const noexcept { return editor_.genome(); }
std::string SessionModel::genome_identity() const { return editor_.genome_identity(); }
const LockState& SessionModel::locks() const noexcept { return editor_.locks(); }
const std::vector<core::OperatorDescriptor>& SessionModel::operator_descriptors() const noexcept { return editor_.operator_descriptors(); }
const core::OperatorDescriptor* SessionModel::descriptor_for_operator(const std::size_t index) const noexcept { return editor_.descriptor_for_operator(index); }
const core::ParameterDescriptor* SessionModel::descriptor_for_parameter(const std::size_t index, const std::string_view name) const noexcept { return editor_.descriptor_for_parameter(index, name); }

const core::ImageBuffer* SessionModel::preview_source() const noexcept {
    if (!source_) {
        return nullptr;
    }
    if (preview_is_proxy_ && proxy_cache_) {
        return &proxy_cache_->image;
    }
    return &*source_;
}

const core::ImageBuffer* SessionModel::display_image() const noexcept {
    return view_.show_before ? preview_source() : preview_result();
}

void SessionModel::after_semantic_edit(const EditResult& result) noexcept {
    if (result.ok()) {
        mark_preview_dirty();
    }
}

EditResult SessionModel::add_operator(const std::string_view type_id, const std::size_t insert_index) {
    EditResult result = editor_.add_operator(type_id, insert_index);
    after_semantic_edit(result);
    return result;
}
EditResult SessionModel::remove_operator(const std::size_t index) {
    EditResult result = editor_.remove_operator(index);
    after_semantic_edit(result);
    if (selected_operator() == std::nullopt) {
        selected_instance_id_.clear();
    }
    return result;
}
EditResult SessionModel::duplicate_operator(const std::size_t index) {
    EditResult result = editor_.duplicate_operator(index);
    after_semantic_edit(result);
    return result;
}
EditResult SessionModel::move_operator(const std::size_t from, const std::size_t to) {
    EditResult result = editor_.move_operator(from, to);
    after_semantic_edit(result);
    return result;
}
EditResult SessionModel::toggle_operator_enabled(const std::size_t index) {
    EditResult result = editor_.toggle_operator_enabled(index);
    after_semantic_edit(result);
    return result;
}
EditResult SessionModel::set_parameter_from_text(const std::size_t index, const std::string_view name, const std::string_view text) {
    EditResult result = editor_.set_parameter_from_text(index, name, text);
    after_semantic_edit(result);
    return result;
}
EditResult SessionModel::nudge_parameter(
    const std::size_t index,
    const std::string_view name,
    const int direction,
    const bool large_step,
    const bool coalesce_history) {
    EditResult result = editor_.nudge_parameter(index, name, direction, large_step, coalesce_history);
    after_semantic_edit(result);
    return result;
}
void SessionModel::end_coalesced_edit() noexcept { editor_.end_coalesced_edit(); }
EditResult SessionModel::toggle_operator_lock(const std::size_t index) { return editor_.toggle_operator_lock(index); }
EditResult SessionModel::toggle_parameter_lock(const std::size_t index, const std::string_view name) { return editor_.toggle_parameter_lock(index, name); }
bool SessionModel::operator_locked(const std::size_t index) const noexcept { return editor_.operator_locked(index); }
bool SessionModel::parameter_locked(const std::size_t index, const std::string_view name) const noexcept { return editor_.parameter_locked(index, name); }

bool SessionModel::undo() {
    if (!editor_.undo()) {
        return false;
    }
    mark_preview_dirty();
    return true;
}
bool SessionModel::redo() {
    if (!editor_.redo()) {
        return false;
    }
    mark_preview_dirty();
    return true;
}
bool SessionModel::can_undo() const noexcept { return editor_.can_undo(); }
bool SessionModel::can_redo() const noexcept { return editor_.can_redo(); }
bool SessionModel::project_dirty() const noexcept { return editor_.project_dirty(); }
void SessionModel::mark_project_saved() noexcept { editor_.mark_saved(); }

void SessionModel::set_selected_operator(const std::optional<std::size_t> operator_index) noexcept {
    if (!operator_index.has_value() || *operator_index >= genome().operators.size()) {
        selected_instance_id_.clear();
        return;
    }
    selected_instance_id_ = genome().operators[*operator_index].instance_id.to_string();
}

std::optional<std::size_t> SessionModel::selected_operator() const noexcept {
    if (selected_instance_id_.empty()) {
        return std::nullopt;
    }
    for (std::size_t index = 0U; index < genome().operators.size(); ++index) {
        if (genome().operators[index].instance_id.to_string() == selected_instance_id_) {
            return index;
        }
    }
    return std::nullopt;
}

const core::ImageBuffer* SessionModel::choose_preview_source(std::string* error) {
    if (!source_) {
        if (error != nullptr) {
            *error = "no source image is loaded";
        }
        return nullptr;
    }
    preview_is_proxy_ = false;
    active_proxy_key_.clear();
    if (!proxy_enabled_) {
        return &*source_;
    }
    const std::string wanted_key = core::proxy_cache_key(source_identity_, proxy_spec_);
    if (!proxy_cache_ || proxy_cache_->cache_key != wanted_key) {
        core::ProxyResult generated = core::make_nearest_proxy(*source_, source_identity_, proxy_spec_);
        if (!generated.ok()) {
            if (error != nullptr) {
                *error = generated.error->message;
            }
            return nullptr;
        }
        proxy_cache_ = std::move(*generated.proxy);
    }
    preview_is_proxy_ = proxy_cache_->is_proxy;
    active_proxy_key_ = proxy_cache_->cache_key;
    return preview_is_proxy_ ? &proxy_cache_->image : &*source_;
}

bool SessionModel::ensure_preview(std::string* error) {
    if (!preview_dirty_ && preview_result_) {
        return true;
    }
    const core::ImageBuffer* input = choose_preview_source(error);
    if (input == nullptr) {
        return false;
    }
    core::RenderResult rendered = core::render_pipeline(*input, editor_.genome(), editor_.registry());
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error ? pipeline_error_text(*rendered.error) : std::string{"pipeline render failed"};
        }
        return false;
    }
    preview_result_ = std::move(*rendered.image);
    preview_dirty_ = false;
    ++render_generation_;
    return true;
}

std::optional<core::ImageBuffer> SessionModel::render_full(std::string* error) const {
    if (!source_) {
        if (error != nullptr) {
            *error = "no source image is loaded";
        }
        return std::nullopt;
    }
    core::RenderResult rendered = core::render_pipeline(*source_, editor_.genome(), editor_.registry());
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error ? pipeline_error_text(*rendered.error) : std::string{"full-resolution pipeline render failed"};
        }
        return std::nullopt;
    }
    return std::move(*rendered.image);
}

PreviewState SessionModel::preview_state() const {
    PreviewState state;
    if (const core::ImageBuffer* image = preview_source(); image != nullptr) {
        state.width = image->width;
        state.height = image->height;
    }
    state.is_proxy = preview_is_proxy_;
    state.proxy_key = active_proxy_key_;
    state.render_generation = render_generation_;
    return state;
}
std::uint64_t SessionModel::render_generation() const noexcept { return render_generation_; }
bool SessionModel::preview_dirty() const noexcept { return preview_dirty_; }

void SessionModel::set_proxy_enabled(const bool enabled) {
    if (proxy_enabled_ != enabled) {
        proxy_enabled_ = enabled;
        mark_preview_dirty();
    }
}
bool SessionModel::proxy_enabled() const noexcept { return proxy_enabled_; }
void SessionModel::set_proxy_spec(core::ProxySpec spec) {
    if (!(proxy_spec_ == spec)) {
        proxy_spec_ = spec;
        proxy_cache_.reset();
        mark_preview_dirty();
    }
}
const core::ProxySpec& SessionModel::proxy_spec() const noexcept { return proxy_spec_; }

void SessionModel::toggle_effects() {
    const EditResult result = editor_.set_all_enabled(!effects_enabled());
    after_semantic_edit(result);
}
bool SessionModel::effects_enabled() const noexcept {
    return std::any_of(genome().operators.begin(), genome().operators.end(), [](const core::OperatorInstance& instance) { return instance.enabled; });
}

std::optional<std::size_t> SessionModel::find_operator_type(const std::string_view type_id) const noexcept {
    for (std::size_t index = 0U; index < genome().operators.size(); ++index) {
        if (genome().operators[index].type_id == type_id) {
            return index;
        }
    }
    return std::nullopt;
}

void SessionModel::adjust_row_offset(const std::int64_t delta) {
    const auto index = find_operator_type(core::kFaultRowOffset);
    if (!index.has_value()) {
        return;
    }
    const EditResult result = editor_.nudge_parameter(*index, "amount", delta < 0 ? -1 : 1, false, false);
    after_semantic_edit(result);
}
void SessionModel::adjust_jitter(const std::int64_t delta) {
    const auto index = find_operator_type(core::kFaultScanlineJitter);
    if (!index.has_value()) {
        return;
    }
    const EditResult result = editor_.nudge_parameter(*index, "max_shift", delta < 0 ? -1 : 1, false, false);
    after_semantic_edit(result);
}
void SessionModel::reroll_seed() {
    const EditResult result = editor_.reroll_seed();
    after_semantic_edit(result);
}
std::int64_t SessionModel::row_offset_amount() const noexcept {
    const auto index = find_operator_type(core::kFaultRowOffset);
    if (!index.has_value()) {
        return 0;
    }
    const auto parameter = genome().operators[*index].parameters.find("amount");
    const auto* value = parameter == genome().operators[*index].parameters.end() ? nullptr : std::get_if<std::int64_t>(&parameter->second);
    return value != nullptr ? *value : 0;
}
std::uint64_t SessionModel::jitter_max_shift() const noexcept {
    const auto index = find_operator_type(core::kFaultScanlineJitter);
    if (!index.has_value()) {
        return 0U;
    }
    const auto parameter = genome().operators[*index].parameters.find("max_shift");
    const auto* value = parameter == genome().operators[*index].parameters.end() ? nullptr : std::get_if<std::uint64_t>(&parameter->second);
    return value != nullptr ? *value : 0U;
}

void SessionModel::set_fit_view() noexcept { view_ = CanvasViewState{}; }
void SessionModel::set_actual_view() noexcept {
    view_.mode = ViewMode::actual_size;
    view_.zoom = 1.0;
    view_.pan_x = 0.0;
    view_.pan_y = 0.0;
}
void SessionModel::zoom_by(const double factor) noexcept {
    if (!(factor > 0.0)) {
        return;
    }
    view_.mode = ViewMode::custom;
    view_.zoom = std::clamp(view_.zoom * factor, 0.05, 64.0);
}
void SessionModel::pan_by(const double dx, const double dy) noexcept { view_.pan_x += dx; view_.pan_y += dy; }
void SessionModel::reset_pan() noexcept { view_.pan_x = 0.0; view_.pan_y = 0.0; }
void SessionModel::toggle_before() noexcept { view_.show_before = !view_.show_before; }
const CanvasViewState& SessionModel::view_state() const noexcept { return view_; }
void SessionModel::mark_preview_dirty() noexcept { preview_dirty_ = true; }

}  // namespace faultmine::app
