#include "faultmine/specimen_tray.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace faultmine::app {
namespace {

[[nodiscard]] std::string pipeline_error_text(const core::PipelineError& error) {
    std::string text{"operator "};
    text += std::to_string(error.operator_index);
    if (!error.operator_type.empty()) {
        text += " (" + error.operator_type + ")";
    }
    text += ": " + error.message;
    return text;
}

}  // namespace

core::MutationLocks SpecimenTrayModel::make_mutation_locks(const LockState& locks) {
    core::MutationLocks converted;
    converted.operators = locks.operators;
    converted.parameters.reserve(locks.parameters.size());
    for (const ParameterLock& lock : locks.parameters) {
        converted.parameters.push_back(core::MutationParameterLock{lock.instance_id, lock.parameter});
    }
    return converted;
}

bool SpecimenTrayModel::generate(
    const core::Genome& parent,
    const LockState& locks,
    const core::FaultRegistry& registry,
    SpecimenTrayConfig config,
    std::string* error) {
    const auto fail = [&](std::string message) {
        error_text_ = std::move(message);
        if (error != nullptr) *error = error_text_;
        return false;
    };

    if (config.population_size == 0U || config.population_size > kMaximumSpecimenPopulation) {
        return fail("specimen population must be in the inclusive range 1..32");
    }
    if (config.thumbnail_proxy.method_version != core::kProxyMethodVersion ||
        config.thumbnail_proxy.max_width == 0U || config.thumbnail_proxy.max_height == 0U) {
        return fail("specimen thumbnail proxy specification is unsupported");
    }

    std::vector<SpecimenTrayItem> next;
    next.reserve(std::max(config.population_size, items_.size()));
    for (const SpecimenTrayItem& item : items_) {
        if (item.pinned) next.push_back(item);
    }
    const std::size_t pinned_count = next.size();
    const std::size_t wanted_new = config.population_size > pinned_count
        ? config.population_size - pinned_count
        : 0U;

    const core::MutationLocks mutation_locks = make_mutation_locks(locks);
    for (std::size_t index = 0U; index < wanted_new; ++index) {
        core::MutationRequest request;
        request.mutation_seed = config.mutation_seed;
        request.descendant_index = static_cast<std::uint64_t>(index);
        request.radius = config.radius;
        request.locks = mutation_locks;
        core::MutationResult generated = core::generate_descendant(
            parent, registry.schema_registry(), request);
        if (!generated.ok()) {
            return fail(generated.error.has_value()
                ? generated.error->message
                : std::string{"descendant generation failed without a structured error"});
        }
        SpecimenTrayItem item;
        item.provenance = std::move(generated.provenance);
        item.genome = std::move(*generated.genome);
        next.push_back(std::move(item));
    }

    config_ = config;
    items_ = std::move(next);
    selected_index_ = items_.empty()
        ? std::nullopt
        : std::optional<std::size_t>{std::min(pinned_count, items_.size() - 1U)};
    source_proxy_.reset();
    source_proxy_identity_.clear();
    error_text_.clear();
    ++generation_token_;
    refresh_busy();
    return true;
}

bool SpecimenTrayModel::render_next(
    const core::ImageBuffer& full_source,
    const std::string& source_identity,
    const core::FaultRegistry& registry,
    const std::uint64_t generation_token,
    std::string* error) {
    if (generation_token != generation_token_) {
        return false;
    }

    auto pending = std::find_if(
        items_.begin(), items_.end(),
        [](const SpecimenTrayItem& item) {
            return !item.preview.has_value() && item.render_error.empty();
        });
    if (pending == items_.end()) {
        busy_ = false;
        return false;
    }

    const std::string wanted_key = core::proxy_cache_key(source_identity, config_.thumbnail_proxy);
    if (!source_proxy_.has_value() || source_proxy_->cache_key != wanted_key ||
        source_proxy_identity_ != source_identity) {
        core::ProxyResult generated = core::make_nearest_proxy(
            full_source, source_identity, config_.thumbnail_proxy);
        if (!generated.ok()) {
            error_text_ = generated.error.has_value()
                ? generated.error->message
                : std::string{"thumbnail proxy generation failed"};
            if (error != nullptr) *error = error_text_;
            pending->render_error = error_text_;
            refresh_busy();
            return true;
        }
        source_proxy_ = std::move(*generated.proxy);
        source_proxy_identity_ = source_identity;
    }

    const core::ImageBuffer& input = source_proxy_->is_proxy
        ? source_proxy_->image
        : full_source;
    core::RenderResult rendered = core::render_pipeline(input, pending->genome, registry);
    if (!rendered.ok()) {
        pending->render_error = rendered.error.has_value()
            ? pipeline_error_text(*rendered.error)
            : std::string{"specimen render failed without a structured error"};
        error_text_ = pending->render_error;
        if (error != nullptr) *error = error_text_;
    } else {
        pending->preview = std::move(*rendered.image);
    }
    refresh_busy();
    return true;
}

void SpecimenTrayModel::reset() noexcept {
    items_.clear();
    selected_index_.reset();
    source_proxy_.reset();
    source_proxy_identity_.clear();
    error_text_.clear();
    busy_ = false;
    ++generation_token_;
}

const SpecimenTrayConfig& SpecimenTrayModel::config() const noexcept { return config_; }
const std::vector<SpecimenTrayItem>& SpecimenTrayModel::items() const noexcept { return items_; }
std::optional<std::size_t> SpecimenTrayModel::selected_index() const noexcept { return selected_index_; }

const SpecimenTrayItem* SpecimenTrayModel::selected_item() const noexcept {
    if (!selected_index_.has_value() || *selected_index_ >= items_.size()) return nullptr;
    return &items_[*selected_index_];
}

bool SpecimenTrayModel::select(const std::size_t index) noexcept {
    if (index >= items_.size()) return false;
    selected_index_ = index;
    return true;
}

bool SpecimenTrayModel::toggle_pin(const std::size_t index) noexcept {
    if (index >= items_.size()) return false;
    items_[index].pinned = !items_[index].pinned;
    return true;
}

bool SpecimenTrayModel::busy() const noexcept { return busy_; }
const std::string& SpecimenTrayModel::error_text() const noexcept { return error_text_; }
std::uint64_t SpecimenTrayModel::generation_token() const noexcept { return generation_token_; }

core::RootSeed SpecimenTrayModel::reroll_seed(const core::RootSeed current) noexcept {
    core::RootSeed next{core::mix64(current.value + 0x9e3779b97f4a7c15ULL)};
    if (next == current) ++next.value;
    return next;
}

void SpecimenTrayModel::refresh_busy() noexcept {
    busy_ = std::any_of(
        items_.begin(), items_.end(),
        [](const SpecimenTrayItem& item) {
            return !item.preview.has_value() && item.render_error.empty();
        });
}

}  // namespace faultmine::app
