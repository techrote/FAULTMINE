#include "faultmine/editor.hpp"

#include "faultmine/colour.hpp"
#include "faultmine/determinism.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace faultmine::app {
namespace {

[[nodiscard]] EditResult make_error(const EditErrorCode code, std::string message) {
    return EditResult{EditError{code, std::move(message)}};
}

[[nodiscard]] const core::OperatorInstance* find_instance(
    const core::Genome& genome,
    const core::InstanceId id) noexcept {
    for (const auto& instance : genome.operators) {
        if (instance.instance_id == id) {
            return &instance;
        }
    }
    return nullptr;
}

[[nodiscard]] bool parse_u64_text(const std::string_view text, std::uint64_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    int base = 10;
    std::string_view digits = text;
    if (digits.size() > 2U && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
        base = 16;
        digits.remove_prefix(2U);
    }
    if (digits.empty()) {
        return false;
    }
    const char* first = digits.data();
    const char* last = digits.data() + digits.size();
    const auto parsed = std::from_chars(first, last, value, base);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] bool parse_i64_text(const std::string_view text, std::int64_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    bool negative = false;
    std::string_view digits = text;
    if (digits.front() == '-') {
        negative = true;
        digits.remove_prefix(1U);
    } else if (digits.front() == '+') {
        digits.remove_prefix(1U);
    }
    std::uint64_t magnitude = 0U;
    if (!parse_u64_text(digits, magnitude)) {
        return false;
    }
    if (!negative) {
        if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            return false;
        }
        value = static_cast<std::int64_t>(magnitude);
        return true;
    }
    constexpr std::uint64_t kMinimumMagnitude = std::uint64_t{1} << 63U;
    if (magnitude > kMinimumMagnitude) {
        return false;
    }
    if (magnitude == kMinimumMagnitude) {
        value = std::numeric_limits<std::int64_t>::min();
        return true;
    }
    value = -static_cast<std::int64_t>(magnitude);
    return true;
}

[[nodiscard]] core::ParameterValue default_value_for(const core::ParameterDescriptor& descriptor) {
    const core::MutationMetadata& mutation = descriptor.mutation;
    switch (descriptor.kind) {
        case core::ParameterKind::boolean:
            return false;
        case core::ParameterKind::signed_integer: {
            std::int64_t value = 0;
            if (mutation.domain == core::MutationDomain::signed_range) {
                value = std::clamp<std::int64_t>(0, mutation.signed_min, mutation.signed_max);
            }
            return value;
        }
        case core::ParameterKind::unsigned_integer:
            if (mutation.domain == core::MutationDomain::unsigned_range) {
                return mutation.unsigned_min;
            }
            return std::uint64_t{0};
        case core::ParameterKind::text:
            if (mutation.domain == core::MutationDomain::choice && !mutation.choices.empty()) {
                return mutation.choices.front();
            }
            if (mutation.domain == core::MutationDomain::colour_rgba) {
                return std::string{"#000000ff"};
            }
            if (mutation.domain == core::MutationDomain::palette) {
                core::Palette palette;
                palette.entries = {core::Rgba8{0U, 0U, 0U, 255U}, core::Rgba8{255U, 255U, 255U, 255U}};
                return core::serialize_palette_canonical(palette);
            }
            if (mutation.domain == core::MutationDomain::lut) {
                return core::serialize_lut_canonical(core::make_identity_lut());
            }
            return std::string{};
    }
    return std::string{};
}

[[nodiscard]] EditResult validate_parameter_value(
    const core::ParameterDescriptor& descriptor,
    core::ParameterValue& value) {
    if (core::parameter_kind(value) != descriptor.kind) {
        return make_error(EditErrorCode::wrong_type, "parameter kind does not match descriptor");
    }
    const core::MutationMetadata& mutation = descriptor.mutation;
    switch (mutation.domain) {
        case core::MutationDomain::signed_range: {
            const auto typed = std::get<std::int64_t>(value);
            if (typed < mutation.signed_min || typed > mutation.signed_max) {
                return make_error(EditErrorCode::invalid_value, "signed value is outside the descriptor range");
            }
            if (mutation.signed_step <= 0 ||
                (typed - mutation.signed_min) % mutation.signed_step != 0) {
                return make_error(EditErrorCode::invalid_value, "signed value does not match the descriptor step");
            }
            break;
        }
        case core::MutationDomain::unsigned_range: {
            const auto typed = std::get<std::uint64_t>(value);
            if (typed < mutation.unsigned_min || typed > mutation.unsigned_max) {
                return make_error(EditErrorCode::invalid_value, "unsigned value is outside the descriptor range");
            }
            if (mutation.unsigned_step == 0U ||
                (typed - mutation.unsigned_min) % mutation.unsigned_step != 0U) {
                return make_error(EditErrorCode::invalid_value, "unsigned value does not match the descriptor step");
            }
            break;
        }
        case core::MutationDomain::choice: {
            const auto& typed = std::get<std::string>(value);
            if (std::find(mutation.choices.begin(), mutation.choices.end(), typed) == mutation.choices.end()) {
                return make_error(EditErrorCode::invalid_value, "text value is not one of the descriptor choices");
            }
            break;
        }
        case core::MutationDomain::toggle:
            if (!std::holds_alternative<bool>(value)) {
                return make_error(EditErrorCode::wrong_type, "toggle parameter must be boolean");
            }
            break;
        case core::MutationDomain::colour_rgba: {
            const auto& typed = std::get<std::string>(value);
            const auto colour = core::parse_rgba8_hex(typed);
            if (!colour.has_value()) {
                return make_error(EditErrorCode::invalid_value, "colour must be #RRGGBB or #RRGGBBAA");
            }
            value = core::rgba8_to_hex(*colour);
            break;
        }
        case core::MutationDomain::palette: {
            const auto parsed = core::parse_palette(std::get<std::string>(value));
            if (!parsed.ok()) {
                return make_error(
                    EditErrorCode::invalid_value,
                    parsed.error.has_value() ? parsed.error->message : "invalid palette asset");
            }
            value = core::serialize_palette_canonical(*parsed.palette);
            break;
        }
        case core::MutationDomain::lut: {
            const auto parsed = core::parse_lut(std::get<std::string>(value));
            if (!parsed.ok()) {
                return make_error(
                    EditErrorCode::invalid_value,
                    parsed.error.has_value() ? parsed.error->message : "invalid LUT asset");
            }
            value = core::serialize_lut_canonical(*parsed.lut);
            break;
        }
        case core::MutationDomain::bitmask:
        case core::MutationDomain::opaque:
            break;
    }
    return {};
}

[[nodiscard]] std::optional<core::ParameterValue> parse_parameter_text(
    const core::ParameterDescriptor& descriptor,
    const std::string_view text,
    EditResult& result) {
    core::ParameterValue value;
    switch (descriptor.kind) {
        case core::ParameterKind::boolean:
            if (text == "true") {
                value = true;
            } else if (text == "false") {
                value = false;
            } else {
                result = make_error(EditErrorCode::invalid_value, "boolean value must be exactly true or false");
                return std::nullopt;
            }
            break;
        case core::ParameterKind::signed_integer: {
            std::int64_t parsed = 0;
            if (!parse_i64_text(text, parsed)) {
                result = make_error(EditErrorCode::invalid_value, "invalid signed integer; decimal and 0x hexadecimal are accepted");
                return std::nullopt;
            }
            value = parsed;
            break;
        }
        case core::ParameterKind::unsigned_integer: {
            std::uint64_t parsed = 0U;
            if (!parse_u64_text(text, parsed)) {
                result = make_error(EditErrorCode::invalid_value, "invalid unsigned integer; decimal and 0x hexadecimal are accepted");
                return std::nullopt;
            }
            value = parsed;
            break;
        }
        case core::ParameterKind::text:
            value = std::string{text};
            break;
    }
    result = validate_parameter_value(descriptor, value);
    if (!result.ok()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool id_in_vector(const std::vector<core::InstanceId>& values, const core::InstanceId id) noexcept {
    return std::find(values.begin(), values.end(), id) != values.end();
}

}  // namespace

EditorModel::EditorModel()
    : registry_(core::make_default_fault_registry()) {}

EditorModel::EditorModel(core::Genome genome)
    : registry_(core::make_default_fault_registry()), genome_(std::move(genome)) {}

const core::FaultRegistry& EditorModel::registry() const noexcept {
    return registry_;
}

const core::Genome& EditorModel::genome() const noexcept {
    return genome_;
}

std::string EditorModel::genome_identity() const {
    return core::genome_identity_hex(genome_);
}

const LockState& EditorModel::locks() const noexcept {
    return locks_;
}

const std::vector<core::OperatorDescriptor>& EditorModel::operator_descriptors() const noexcept {
    return registry_.schema_registry().descriptors();
}

const core::OperatorDescriptor* EditorModel::descriptor_for_operator(const std::size_t operator_index) const noexcept {
    if (operator_index >= genome_.operators.size()) {
        return nullptr;
    }
    return registry_.schema_registry().find(genome_.operators[operator_index].type_id);
}

const core::ParameterDescriptor* EditorModel::descriptor_for_parameter(
    const std::size_t operator_index,
    const std::string_view parameter_name) const noexcept {
    const core::OperatorDescriptor* descriptor = descriptor_for_operator(operator_index);
    if (descriptor == nullptr) {
        return nullptr;
    }
    for (const auto& parameter : descriptor->parameters) {
        if (parameter.name == parameter_name) {
            return &parameter;
        }
    }
    return nullptr;
}

EditorModel::Snapshot EditorModel::snapshot() const {
    return Snapshot{genome_, locks_};
}

void EditorModel::restore_snapshot(Snapshot snapshot_value) {
    genome_ = std::move(snapshot_value.genome);
    locks_ = std::move(snapshot_value.locks);
}

void EditorModel::begin_edit() {
    undo_.push_back(snapshot());
    redo_.clear();
    coalesce_key_.clear();
    dirty_ = true;
}

void EditorModel::begin_coalesced_edit(std::string key) {
    if (coalesce_key_ != key) {
        begin_edit();
        coalesce_key_ = std::move(key);
    } else {
        dirty_ = true;
    }
}

void EditorModel::end_coalesced_edit() noexcept {
    coalesce_key_.clear();
}

bool EditorModel::instance_id_exists(const core::InstanceId id) const noexcept {
    return find_instance(genome_, id) != nullptr;
}

core::InstanceId EditorModel::derive_unique_instance_id(
    const core::InstanceId parent,
    const std::string_view purpose) const noexcept {
    for (std::uint64_t ordinal = 0U;; ++ordinal) {
        const core::InstanceId candidate = core::derive_instance_id(genome_.root_seed, parent, purpose, ordinal);
        if (!instance_id_exists(candidate)) {
            return candidate;
        }
    }
}

void EditorModel::remove_locks_for_instance(const core::InstanceId id) {
    std::erase(locks_.operators, id);
    std::erase_if(locks_.parameters, [id](const ParameterLock& lock) { return lock.instance_id == id; });
}

EditResult EditorModel::validate_state(const core::Genome& genome, const LockState& locks) const {
    if (const auto error = core::validate_genome(genome, registry_.schema_registry()); error.has_value()) {
        return make_error(EditErrorCode::invalid_genome, error->path + ": " + error->message);
    }
    for (std::size_t index = 0U; index < locks.operators.size(); ++index) {
        if (find_instance(genome, locks.operators[index]) == nullptr) {
            return make_error(EditErrorCode::invalid_genome, "operator lock refers to an instance not present in the genome");
        }
        if (std::find(locks.operators.begin(), locks.operators.begin() + static_cast<std::ptrdiff_t>(index), locks.operators[index]) !=
            locks.operators.begin() + static_cast<std::ptrdiff_t>(index)) {
            return make_error(EditErrorCode::invalid_genome, "duplicate operator lock");
        }
    }
    for (std::size_t index = 0U; index < locks.parameters.size(); ++index) {
        const ParameterLock& lock = locks.parameters[index];
        const core::OperatorInstance* instance = find_instance(genome, lock.instance_id);
        if (instance == nullptr) {
            return make_error(EditErrorCode::invalid_genome, "parameter lock refers to an instance not present in the genome");
        }
        const core::OperatorDescriptor* descriptor = registry_.schema_registry().find(instance->type_id);
        const bool declared = descriptor != nullptr && std::any_of(
            descriptor->parameters.begin(), descriptor->parameters.end(),
            [&lock](const core::ParameterDescriptor& parameter) { return parameter.name == lock.parameter; });
        if (!declared) {
            return make_error(EditErrorCode::invalid_genome, "parameter lock refers to an undeclared parameter");
        }
        if (std::find(locks.parameters.begin(), locks.parameters.begin() + static_cast<std::ptrdiff_t>(index), lock) !=
            locks.parameters.begin() + static_cast<std::ptrdiff_t>(index)) {
            return make_error(EditErrorCode::invalid_genome, "duplicate parameter lock");
        }
    }
    return {};
}

EditResult EditorModel::add_operator(const std::string_view type_id, const std::size_t insert_index) {
    const core::OperatorDescriptor* descriptor = registry_.schema_registry().find(type_id);
    if (descriptor == nullptr) {
        return make_error(EditErrorCode::unknown_operator, "operator type is not registered");
    }

    core::OperatorInstance instance;
    const core::InstanceId parent{core::stable_tag_hash(type_id), core::stable_tag_hash("manual-add")};
    instance.instance_id = derive_unique_instance_id(parent, "manual-add-operator");
    instance.type_id = descriptor->type_id;
    instance.type_version = descriptor->current_version;
    instance.enabled = true;
    for (const auto& parameter : descriptor->parameters) {
        core::ParameterValue value = default_value_for(parameter);
        EditResult validation = validate_parameter_value(parameter, value);
        if (!validation.ok()) {
            return validation;
        }
        instance.parameters.emplace(parameter.name, std::move(value));
    }

    core::Genome candidate = genome_;
    candidate.operators.insert(
        candidate.operators.begin() + static_cast<std::ptrdiff_t>(std::min(insert_index, candidate.operators.size())),
        instance);
    if (const auto error = core::validate_genome(candidate, registry_.schema_registry()); error.has_value()) {
        return make_error(EditErrorCode::invalid_genome, error->path + ": " + error->message);
    }
    begin_edit();
    genome_ = std::move(candidate);
    return {};
}

EditResult EditorModel::remove_operator(const std::size_t operator_index) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    const core::InstanceId id = genome_.operators[operator_index].instance_id;
    begin_edit();
    genome_.operators.erase(genome_.operators.begin() + static_cast<std::ptrdiff_t>(operator_index));
    remove_locks_for_instance(id);
    return {};
}

EditResult EditorModel::duplicate_operator(const std::size_t operator_index) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    core::OperatorInstance copy = genome_.operators[operator_index];
    copy.instance_id = derive_unique_instance_id(copy.instance_id, "manual-duplicate-operator");
    begin_edit();
    genome_.operators.insert(genome_.operators.begin() + static_cast<std::ptrdiff_t>(operator_index + 1U), std::move(copy));
    return {};
}

EditResult EditorModel::move_operator(const std::size_t from_index, const std::size_t to_index) {
    if (from_index >= genome_.operators.size() || to_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    if (from_index == to_index) {
        return {};
    }
    begin_edit();
    core::OperatorInstance moved = std::move(genome_.operators[from_index]);
    genome_.operators.erase(genome_.operators.begin() + static_cast<std::ptrdiff_t>(from_index));
    genome_.operators.insert(genome_.operators.begin() + static_cast<std::ptrdiff_t>(to_index), std::move(moved));
    return {};
}

EditResult EditorModel::toggle_operator_enabled(const std::size_t operator_index) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    begin_edit();
    genome_.operators[operator_index].enabled = !genome_.operators[operator_index].enabled;
    return {};
}

EditResult EditorModel::set_parameter_from_text(
    const std::size_t operator_index,
    const std::string_view parameter_name,
    const std::string_view text) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    const core::ParameterDescriptor* descriptor = descriptor_for_parameter(operator_index, parameter_name);
    if (descriptor == nullptr) {
        return make_error(EditErrorCode::unknown_parameter, "parameter is not declared by the selected operator");
    }
    EditResult parsed_result;
    auto value = parse_parameter_text(*descriptor, text, parsed_result);
    if (!value.has_value()) {
        return parsed_result;
    }
    const auto current = genome_.operators[operator_index].parameters.find(parameter_name);
    if (current != genome_.operators[operator_index].parameters.end() && current->second == *value) {
        return {};
    }
    core::Genome candidate = genome_;
    candidate.operators[operator_index].parameters[std::string{parameter_name}] = *value;
    if (const auto error = core::validate_genome(candidate, registry_.schema_registry()); error.has_value()) {
        return make_error(EditErrorCode::invalid_genome, error->path + ": " + error->message);
    }
    begin_edit();
    genome_ = std::move(candidate);
    return {};
}

EditResult EditorModel::nudge_parameter(
    const std::size_t operator_index,
    const std::string_view parameter_name,
    const int direction,
    const bool large_step,
    const bool coalesce_history) {
    if (direction == 0) {
        return {};
    }
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    const core::ParameterDescriptor* descriptor = descriptor_for_parameter(operator_index, parameter_name);
    if (descriptor == nullptr) {
        return make_error(EditErrorCode::unknown_parameter, "parameter is not declared by the selected operator");
    }
    auto parameter = genome_.operators[operator_index].parameters.find(parameter_name);
    if (parameter == genome_.operators[operator_index].parameters.end()) {
        return make_error(EditErrorCode::unknown_parameter, "parameter value is missing");
    }

    core::ParameterValue next = parameter->second;
    if (descriptor->kind == core::ParameterKind::signed_integer) {
        const std::int64_t current = std::get<std::int64_t>(parameter->second);
        std::int64_t step = descriptor->mutation.domain == core::MutationDomain::signed_range
            ? descriptor->mutation.signed_step
            : 1;
        if (step <= 0) {
            step = 1;
        }
        if (large_step && step <= std::numeric_limits<std::int64_t>::max() / 10) {
            step *= 10;
        }
        const std::int64_t minimum = descriptor->mutation.domain == core::MutationDomain::signed_range
            ? descriptor->mutation.signed_min
            : std::numeric_limits<std::int64_t>::min();
        const std::int64_t maximum = descriptor->mutation.domain == core::MutationDomain::signed_range
            ? descriptor->mutation.signed_max
            : std::numeric_limits<std::int64_t>::max();
        std::int64_t candidate = current;
        if (direction > 0) {
            candidate = current > maximum - std::min(step, maximum) ? maximum : std::min(maximum, current + step);
        } else {
            candidate = current < minimum + std::min(step, std::numeric_limits<std::int64_t>::max()) ? minimum : std::max(minimum, current - step);
        }
        next = candidate;
    } else if (descriptor->kind == core::ParameterKind::unsigned_integer) {
        const std::uint64_t current = std::get<std::uint64_t>(parameter->second);
        std::uint64_t step = descriptor->mutation.domain == core::MutationDomain::unsigned_range
            ? descriptor->mutation.unsigned_step
            : 1U;
        if (step == 0U) {
            step = 1U;
        }
        if (large_step && step <= std::numeric_limits<std::uint64_t>::max() / 10U) {
            step *= 10U;
        }
        const std::uint64_t minimum = descriptor->mutation.domain == core::MutationDomain::unsigned_range
            ? descriptor->mutation.unsigned_min
            : 0U;
        const std::uint64_t maximum = descriptor->mutation.domain == core::MutationDomain::unsigned_range
            ? descriptor->mutation.unsigned_max
            : std::numeric_limits<std::uint64_t>::max();
        if (direction > 0) {
            next = current > maximum - std::min(step, maximum) ? maximum : std::min(maximum, current + step);
        } else {
            next = current < minimum + step || current < step ? minimum : std::max(minimum, current - step);
        }
    } else {
        return make_error(EditErrorCode::wrong_type, "selected parameter is not numeric and cannot be nudged");
    }

    EditResult validation = validate_parameter_value(*descriptor, next);
    if (!validation.ok()) {
        return validation;
    }
    if (next == parameter->second) {
        return {};
    }
    if (coalesce_history) {
        begin_coalesced_edit(
            genome_.operators[operator_index].instance_id.to_string() + ":" + std::string{parameter_name});
    } else {
        begin_edit();
    }
    genome_.operators[operator_index].parameters[std::string{parameter_name}] = std::move(next);
    return {};
}

EditResult EditorModel::toggle_operator_lock(const std::size_t operator_index) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    const core::InstanceId id = genome_.operators[operator_index].instance_id;
    begin_edit();
    const auto found = std::find(locks_.operators.begin(), locks_.operators.end(), id);
    if (found == locks_.operators.end()) {
        locks_.operators.push_back(id);
    } else {
        locks_.operators.erase(found);
    }
    return {};
}

EditResult EditorModel::toggle_parameter_lock(
    const std::size_t operator_index,
    const std::string_view parameter_name) {
    if (operator_index >= genome_.operators.size()) {
        return make_error(EditErrorCode::invalid_index, "operator index is out of range");
    }
    if (descriptor_for_parameter(operator_index, parameter_name) == nullptr) {
        return make_error(EditErrorCode::unknown_parameter, "parameter is not declared by the selected operator");
    }
    const ParameterLock wanted{genome_.operators[operator_index].instance_id, std::string{parameter_name}};
    begin_edit();
    const auto found = std::find(locks_.parameters.begin(), locks_.parameters.end(), wanted);
    if (found == locks_.parameters.end()) {
        locks_.parameters.push_back(wanted);
    } else {
        locks_.parameters.erase(found);
    }
    return {};
}

bool EditorModel::operator_locked(const std::size_t operator_index) const noexcept {
    return operator_index < genome_.operators.size() && id_in_vector(locks_.operators, genome_.operators[operator_index].instance_id);
}

bool EditorModel::parameter_locked(
    const std::size_t operator_index,
    const std::string_view parameter_name) const noexcept {
    if (operator_index >= genome_.operators.size()) {
        return false;
    }
    const ParameterLock wanted{genome_.operators[operator_index].instance_id, std::string{parameter_name}};
    return std::find(locks_.parameters.begin(), locks_.parameters.end(), wanted) != locks_.parameters.end();
}

bool EditorModel::can_undo() const noexcept {
    return !undo_.empty();
}

bool EditorModel::can_redo() const noexcept {
    return !redo_.empty();
}

bool EditorModel::undo() {
    if (undo_.empty()) {
        return false;
    }
    redo_.push_back(snapshot());
    restore_snapshot(std::move(undo_.back()));
    undo_.pop_back();
    coalesce_key_.clear();
    dirty_ = true;
    return true;
}

bool EditorModel::redo() {
    if (redo_.empty()) {
        return false;
    }
    undo_.push_back(snapshot());
    restore_snapshot(std::move(redo_.back()));
    redo_.pop_back();
    coalesce_key_.clear();
    dirty_ = true;
    return true;
}

void EditorModel::clear_history() noexcept {
    undo_.clear();
    redo_.clear();
    coalesce_key_.clear();
}

bool EditorModel::project_dirty() const noexcept {
    return dirty_;
}

void EditorModel::mark_saved() noexcept {
    dirty_ = false;
}

void EditorModel::mark_external_change() noexcept {
    clear_history();
    dirty_ = true;
}

EditResult EditorModel::replace_state(
    core::Genome genome,
    LockState locks,
    const bool clear_history_boundary) {
    EditResult validation = validate_state(genome, locks);
    if (!validation.ok()) {
        return validation;
    }
    genome_ = std::move(genome);
    locks_ = std::move(locks);
    if (clear_history_boundary) {
        clear_history();
        dirty_ = false;
    } else {
        dirty_ = true;
    }
    return {};
}

EditResult EditorModel::set_all_enabled(const bool enabled) {
    const bool already = std::all_of(
        genome_.operators.begin(), genome_.operators.end(),
        [enabled](const core::OperatorInstance& instance) { return instance.enabled == enabled; });
    if (already) {
        return {};
    }
    begin_edit();
    for (auto& instance : genome_.operators) {
        instance.enabled = enabled;
    }
    return {};
}

EditResult EditorModel::reroll_seed() {
    begin_edit();
    genome_.root_seed.value = core::mix64(genome_.root_seed.value + 0x9e3779b97f4a7c15ULL);
    return {};
}

std::string parameter_value_to_text(const core::ParameterValue& value) {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return std::to_string(std::get<std::int64_t>(value));
    }
    if (std::holds_alternative<std::uint64_t>(value)) {
        return std::to_string(std::get<std::uint64_t>(value));
    }
    return std::get<std::string>(value);
}

}  // namespace faultmine::app
