#include "faultmine/session.hpp"

#include "faultmine/determinism.hpp"
#include "faultmine/image.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace faultmine::app {
namespace {

core::OperatorInstance make_operator(
    const core::InstanceId id,
    const char* type_id) {
    core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type_id;
    instance.type_version = 1U;
    instance.enabled = true;
    return instance;
}

core::OperatorInstance* find_operator(core::Genome& genome, const char* type_id) noexcept {
    for (auto& instance : genome.operators) {
        if (instance.type_id == type_id) {
            return &instance;
        }
    }
    return nullptr;
}

const core::OperatorInstance* find_operator(const core::Genome& genome, const char* type_id) noexcept {
    for (const auto& instance : genome.operators) {
        if (instance.type_id == type_id) {
            return &instance;
        }
    }
    return nullptr;
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

}  // namespace

SessionModel::SessionModel()
    : registry_(core::make_starter_fault_registry()),
      genome_(make_default_genome()) {}

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

    genome.operators = {
        std::move(row),
        std::move(channels),
        std::move(xors),
        std::move(jitter)};
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
    mark_preview_dirty();
    set_fit_view();
    return true;
}

bool SessionModel::has_source() const noexcept {
    return source_.has_value();
}

const core::ImageBuffer* SessionModel::full_source() const noexcept {
    return source_.has_value() ? &*source_ : nullptr;
}

const core::ImageBuffer* SessionModel::preview_source() const noexcept {
    if (!source_.has_value()) {
        return nullptr;
    }
    if (preview_is_proxy_ && proxy_cache_.has_value()) {
        return &proxy_cache_->image;
    }
    return &*source_;
}

const core::ImageBuffer* SessionModel::preview_result() const noexcept {
    return preview_result_.has_value() ? &*preview_result_ : nullptr;
}

const core::ImageBuffer* SessionModel::display_image() const noexcept {
    if (view_.show_before) {
        return preview_source();
    }
    return preview_result();
}

const std::string& SessionModel::source_identity() const noexcept {
    return source_identity_;
}

const std::filesystem::path& SessionModel::source_path() const noexcept {
    return source_path_;
}

const core::Genome& SessionModel::genome() const noexcept {
    return genome_;
}

std::string SessionModel::genome_identity() const {
    return core::genome_identity_hex(genome_);
}

const core::ImageBuffer* SessionModel::choose_preview_source(std::string* error) {
    if (!source_.has_value()) {
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
    if (!proxy_cache_.has_value() || proxy_cache_->cache_key != wanted_key) {
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
    if (!preview_dirty_ && preview_result_.has_value()) {
        return true;
    }

    const core::ImageBuffer* preview_input = choose_preview_source(error);
    if (preview_input == nullptr) {
        return false;
    }

    core::RenderResult rendered = core::render_pipeline(*preview_input, genome_, registry_);
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error.has_value()
                ? pipeline_error_text(*rendered.error)
                : std::string{"pipeline render failed"};
        }
        return false;
    }

    preview_result_ = std::move(*rendered.image);
    preview_dirty_ = false;
    ++render_generation_;
    return true;
}

std::optional<core::ImageBuffer> SessionModel::render_full(std::string* error) const {
    if (!source_.has_value()) {
        if (error != nullptr) {
            *error = "no source image is loaded";
        }
        return std::nullopt;
    }
    core::RenderResult rendered = core::render_pipeline(*source_, genome_, registry_);
    if (!rendered.ok()) {
        if (error != nullptr) {
            *error = rendered.error.has_value()
                ? pipeline_error_text(*rendered.error)
                : std::string{"full-resolution pipeline render failed"};
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

std::uint64_t SessionModel::render_generation() const noexcept {
    return render_generation_;
}

bool SessionModel::preview_dirty() const noexcept {
    return preview_dirty_;
}

void SessionModel::set_proxy_enabled(const bool enabled) {
    if (proxy_enabled_ != enabled) {
        proxy_enabled_ = enabled;
        mark_preview_dirty();
    }
}

bool SessionModel::proxy_enabled() const noexcept {
    return proxy_enabled_;
}

void SessionModel::set_proxy_spec(core::ProxySpec spec) {
    if (!(proxy_spec_ == spec)) {
        proxy_spec_ = spec;
        proxy_cache_.reset();
        mark_preview_dirty();
    }
}

const core::ProxySpec& SessionModel::proxy_spec() const noexcept {
    return proxy_spec_;
}

void SessionModel::toggle_effects() {
    const bool enable = !effects_enabled();
    for (auto& instance : genome_.operators) {
        instance.enabled = enable;
    }
    mark_preview_dirty();
}

bool SessionModel::effects_enabled() const noexcept {
    return std::any_of(
        genome_.operators.begin(),
        genome_.operators.end(),
        [](const core::OperatorInstance& instance) { return instance.enabled; });
}

void SessionModel::adjust_row_offset(const std::int64_t delta) {
    core::OperatorInstance* instance = find_operator(genome_, core::kFaultRowOffset);
    if (instance == nullptr) {
        return;
    }
    const auto parameter = instance->parameters.find("amount");
    if (parameter == instance->parameters.end()) {
        return;
    }
    auto* amount = std::get_if<std::int64_t>(&parameter->second);
    if (amount == nullptr) {
        return;
    }
    if ((delta > 0 && *amount > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 && *amount < std::numeric_limits<std::int64_t>::min() - delta)) {
        return;
    }
    *amount += delta;
    mark_preview_dirty();
}

void SessionModel::adjust_jitter(const std::int64_t delta) {
    core::OperatorInstance* instance = find_operator(genome_, core::kFaultScanlineJitter);
    if (instance == nullptr) {
        return;
    }
    const auto parameter = instance->parameters.find("max_shift");
    if (parameter == instance->parameters.end()) {
        return;
    }
    auto* amount = std::get_if<std::uint64_t>(&parameter->second);
    if (amount == nullptr) {
        return;
    }
    constexpr std::uint64_t kUiMaximum = 64U;
    if (delta > 0) {
        const std::uint64_t change = static_cast<std::uint64_t>(delta);
        *amount = std::min(kUiMaximum, *amount + std::min(change, kUiMaximum));
    } else if (delta < 0) {
        const std::uint64_t change = static_cast<std::uint64_t>(-(delta + 1)) + 1U;
        *amount = change >= *amount ? 0U : *amount - change;
    }
    mark_preview_dirty();
}

void SessionModel::reroll_seed() noexcept {
    genome_.root_seed.value = core::mix64(genome_.root_seed.value + 0x9e3779b97f4a7c15ULL);
    mark_preview_dirty();
}

std::int64_t SessionModel::row_offset_amount() const noexcept {
    const core::OperatorInstance* instance = find_operator(genome_, core::kFaultRowOffset);
    if (instance == nullptr) {
        return 0;
    }
    const auto parameter = instance->parameters.find("amount");
    if (parameter == instance->parameters.end()) {
        return 0;
    }
    const auto* amount = std::get_if<std::int64_t>(&parameter->second);
    return amount != nullptr ? *amount : 0;
}

std::uint64_t SessionModel::jitter_max_shift() const noexcept {
    const core::OperatorInstance* instance = find_operator(genome_, core::kFaultScanlineJitter);
    if (instance == nullptr) {
        return 0U;
    }
    const auto parameter = instance->parameters.find("max_shift");
    if (parameter == instance->parameters.end()) {
        return 0U;
    }
    const auto* amount = std::get_if<std::uint64_t>(&parameter->second);
    return amount != nullptr ? *amount : 0U;
}

void SessionModel::set_fit_view() noexcept {
    view_.mode = ViewMode::fit;
    view_.zoom = 1.0;
    view_.pan_x = 0.0;
    view_.pan_y = 0.0;
}

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

void SessionModel::pan_by(const double dx, const double dy) noexcept {
    view_.pan_x += dx;
    view_.pan_y += dy;
}

void SessionModel::reset_pan() noexcept {
    view_.pan_x = 0.0;
    view_.pan_y = 0.0;
}

void SessionModel::toggle_before() noexcept {
    view_.show_before = !view_.show_before;
}

const CanvasViewState& SessionModel::view_state() const noexcept {
    return view_;
}

void SessionModel::mark_preview_dirty() noexcept {
    preview_dirty_ = true;
}

}  // namespace faultmine::app
