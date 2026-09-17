#include "faultmine/project.hpp"

#include "../core/json.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::app {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] ProjectError make_error(
    const ProjectErrorCode code,
    std::string path,
    std::string message,
    const std::optional<std::size_t> byte_offset = std::nullopt) {
    return ProjectError{code, std::move(path), std::move(message), byte_offset};
}

[[nodiscard]] const Value* find_field(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) {
        return nullptr;
    }
    for (const auto& field : object.object) {
        if (field.first == name) {
            return &field.second;
        }
    }
    return nullptr;
}

[[nodiscard]] bool only_fields(
    const Value& object,
    const std::initializer_list<std::string_view> allowed,
    std::string& unexpected) {
    if (object.type != ValueType::object) {
        return false;
    }
    for (const auto& field : object.object) {
        const bool known = std::find(allowed.begin(), allowed.end(), field.first) != allowed.end();
        if (!known) {
            unexpected = field.first;
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_u64_number(const Value& value, std::uint64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty() || value.text.front() == '-') {
        return false;
    }
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto parsed = std::from_chars(first, last, output, 10);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] bool parse_i64_number(const Value& value, std::int64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty()) {
        return false;
    }
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto parsed = std::from_chars(first, last, output, 10);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] bool valid_identity_hex(const std::string_view text, const std::size_t length) noexcept {
    if (text.size() != length) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](const char character) {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
    });
}

[[nodiscard]] std::string serialize_json_value(const Value& value) {
    switch (value.type) {
        case ValueType::null_value:
            return "null";
        case ValueType::boolean:
            return value.boolean ? "true" : "false";
        case ValueType::number:
            return value.text;
        case ValueType::string:
            return "\"" + core::json::escape_string(value.text) + "\"";
        case ValueType::array: {
            std::string output{"["};
            for (std::size_t index = 0U; index < value.array.size(); ++index) {
                if (index != 0U) {
                    output += ',';
                }
                output += serialize_json_value(value.array[index]);
            }
            output += ']';
            return output;
        }
        case ValueType::object: {
            std::string output{"{"};
            for (std::size_t index = 0U; index < value.object.size(); ++index) {
                if (index != 0U) {
                    output += ',';
                }
                output += '"';
                output += core::json::escape_string(value.object[index].first);
                output += "\":";
                output += serialize_json_value(value.object[index].second);
            }
            output += '}';
            return output;
        }
    }
    return "null";
}

[[nodiscard]] bool instance_exists(const core::Genome& genome, const core::InstanceId id) noexcept {
    return std::any_of(
        genome.operators.begin(), genome.operators.end(),
        [id](const core::OperatorInstance& instance) { return instance.instance_id == id; });
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

[[nodiscard]] bool parameter_declared(
    const core::OperatorRegistry& registry,
    const core::OperatorInstance& instance,
    const std::string_view name) noexcept {
    const core::OperatorDescriptor* descriptor = registry.find(instance.type_id);
    return descriptor != nullptr && std::any_of(
        descriptor->parameters.begin(), descriptor->parameters.end(),
        [name](const core::ParameterDescriptor& parameter) { return parameter.name == name; });
}

[[nodiscard]] std::string mode_or_fit(const std::string_view mode) {
    return mode == "fit" || mode == "actual" || mode == "custom" ? std::string{mode} : std::string{"fit"};
}

}  // namespace

std::string serialize_project_canonical(const ProjectDocument& project) {
    std::string genome = core::serialize_canonical_genome(project.genome);
    if (!genome.empty() && genome.back() == '\n') {
        genome.pop_back();
    }

    std::vector<core::InstanceId> lock_ids;
    lock_ids.reserve(project.locks.operators.size() + project.locks.parameters.size());
    for (const auto id : project.locks.operators) {
        if (std::find(lock_ids.begin(), lock_ids.end(), id) == lock_ids.end()) {
            lock_ids.push_back(id);
        }
    }
    for (const auto& lock : project.locks.parameters) {
        if (std::find(lock_ids.begin(), lock_ids.end(), lock.instance_id) == lock_ids.end()) {
            lock_ids.push_back(lock.instance_id);
        }
    }
    std::sort(lock_ids.begin(), lock_ids.end(), [](const core::InstanceId left, const core::InstanceId right) {
        return left.to_string() < right.to_string();
    });

    std::string locks{"["};
    for (std::size_t index = 0U; index < lock_ids.size(); ++index) {
        if (index != 0U) {
            locks += ',';
        }
        const core::InstanceId id = lock_ids[index];
        const bool operator_locked = std::find(project.locks.operators.begin(), project.locks.operators.end(), id) !=
            project.locks.operators.end();
        std::vector<std::string> parameters;
        for (const auto& lock : project.locks.parameters) {
            if (lock.instance_id == id) {
                parameters.push_back(lock.parameter);
            }
        }
        std::sort(parameters.begin(), parameters.end());
        parameters.erase(std::unique(parameters.begin(), parameters.end()), parameters.end());

        locks += "{\"instance_id\":\"" + id.to_string() + "\",\"operator\":";
        locks += operator_locked ? "true" : "false";
        locks += ",\"parameters\":[";
        for (std::size_t parameter_index = 0U; parameter_index < parameters.size(); ++parameter_index) {
            if (parameter_index != 0U) {
                locks += ',';
            }
            locks += "\"" + core::json::escape_string(parameters[parameter_index]) + "\"";
        }
        locks += "]}";
    }
    locks += ']';

    std::string output;
    output += "{\"project_version\":" + std::to_string(project.project_version);
    output += ",\"source\":{\"path\":\"" + core::json::escape_string(project.source.path_utf8) + "\"";
    output += ",\"identity\":\"" + core::json::escape_string(project.source.source_identity) + "\"}";
    output += ",\"genome\":" + genome;
    output += ",\"locks\":" + locks;
    output += ",\"session\":{\"proxy_enabled\":";
    output += project.session.proxy_enabled ? "true" : "false";
    output += ",\"proxy_max_width\":" + std::to_string(project.session.proxy_spec.max_width);
    output += ",\"proxy_max_height\":" + std::to_string(project.session.proxy_spec.max_height);
    output += ",\"proxy_method_version\":" + std::to_string(project.session.proxy_spec.method_version);
    output += ",\"selected_instance_id\":\"" + core::json::escape_string(project.session.selected_instance_id) + "\"}";
    output += ",\"ui\":{\"view_mode\":\"" + core::json::escape_string(mode_or_fit(project.ui.mode)) + "\"";
    output += ",\"zoom_milli\":" + std::to_string(project.ui.zoom_milli);
    output += ",\"pan_x_milli\":" + std::to_string(project.ui.pan_x_milli);
    output += ",\"pan_y_milli\":" + std::to_string(project.ui.pan_y_milli);
    output += ",\"show_before\":";
    output += project.ui.show_before ? "true" : "false";
    output += "}}\n";
    return output;
}

ProjectParseResult parse_project(
    const std::string_view text,
    const core::OperatorRegistry& registry) {
    const auto parsed = core::json::parse(text);
    if (parsed.error.has_value()) {
        return ProjectParseResult{
            std::nullopt,
            make_error(
                parsed.error->kind == core::json::ParseErrorKind::duplicate_key
                    ? ProjectErrorCode::duplicate_field
                    : ProjectErrorCode::syntax,
                "$",
                parsed.error->message,
                parsed.error->offset)};
    }
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$", "project root must be an object")};
    }
    const Value& root = *parsed.value;
    std::string unexpected;
    if (!only_fields(root, {"project_version", "source", "genome", "locks", "session", "ui"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, "$.'" + unexpected + "'", "unexpected project field")};
    }

    ProjectDocument project;
    const Value* version = find_field(root, "project_version");
    const Value* source = find_field(root, "source");
    const Value* genome_value = find_field(root, "genome");
    const Value* locks = find_field(root, "locks");
    const Value* session = find_field(root, "session");
    const Value* ui = find_field(root, "ui");
    if (version == nullptr || source == nullptr || genome_value == nullptr || locks == nullptr || session == nullptr || ui == nullptr) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, "$", "project is missing a required top-level field")};
    }

    std::uint64_t project_version = 0U;
    if (!parse_u64_number(*version, project_version) || project_version != kProjectSchemaVersion) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unsupported_version, "$.project_version", "unsupported project version")};
    }
    project.project_version = static_cast<std::uint32_t>(project_version);

    if (source->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.source", "source must be an object")};
    }
    if (!only_fields(*source, {"path", "identity"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, "$.source." + unexpected, "unexpected source field")};
    }
    const Value* path = find_field(*source, "path");
    const Value* identity = find_field(*source, "identity");
    if (path == nullptr || identity == nullptr) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, "$.source", "source path and identity are required")};
    }
    if (path->type != ValueType::string || identity->type != ValueType::string) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.source", "source path and identity must be strings")};
    }
    if (!valid_identity_hex(identity->text, 64U)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.source.identity", "source identity must be 64 hexadecimal digits")};
    }
    project.source.path_utf8 = path->text;
    project.source.source_identity = identity->text;
    std::transform(
        project.source.source_identity.begin(), project.source.source_identity.end(), project.source.source_identity.begin(),
        [](const unsigned char character) { return static_cast<char>(character >= 'A' && character <= 'F' ? character + ('a' - 'A') : character); });

    if (genome_value->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.genome", "genome must be an object")};
    }
    std::string genome_text = serialize_json_value(*genome_value);
    genome_text.push_back('\n');
    const auto genome = core::parse_genome(genome_text, registry);
    if (!genome.ok()) {
        std::string message = "embedded genome is invalid";
        if (genome.error.has_value()) {
            message += ": " + genome.error->path + ": " + genome.error->message;
        }
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_genome, "$.genome", std::move(message))};
    }
    project.genome = *genome.genome;

    if (locks->type != ValueType::array) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.locks", "locks must be an array")};
    }
    std::vector<core::InstanceId> seen_lock_records;
    for (std::size_t lock_index = 0U; lock_index < locks->array.size(); ++lock_index) {
        const Value& record = locks->array[lock_index];
        const std::string record_path = "$.locks[" + std::to_string(lock_index) + "]";
        if (record.type != ValueType::object) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, record_path, "lock record must be an object")};
        }
        if (!only_fields(record, {"instance_id", "operator", "parameters"}, unexpected)) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, record_path + "." + unexpected, "unexpected lock field")};
        }
        const Value* instance_value = find_field(record, "instance_id");
        const Value* operator_value = find_field(record, "operator");
        const Value* parameters_value = find_field(record, "parameters");
        if (instance_value == nullptr || operator_value == nullptr || parameters_value == nullptr) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, record_path, "lock record is missing required fields")};
        }
        if (instance_value->type != ValueType::string || operator_value->type != ValueType::boolean || parameters_value->type != ValueType::array) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, record_path, "lock field types are invalid")};
        }
        const auto instance_id = core::InstanceId::parse(instance_value->text);
        if (!instance_id.has_value() || !instance_exists(project.genome, *instance_id)) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_lock, record_path + ".instance_id", "lock instance is absent from the embedded genome")};
        }
        if (std::find(seen_lock_records.begin(), seen_lock_records.end(), *instance_id) != seen_lock_records.end()) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_lock, record_path, "duplicate lock record for one operator instance")};
        }
        seen_lock_records.push_back(*instance_id);
        if (operator_value->boolean) {
            project.locks.operators.push_back(*instance_id);
        }
        const core::OperatorInstance* instance = find_instance(project.genome, *instance_id);
        std::vector<std::string> seen_parameters;
        for (std::size_t parameter_index = 0U; parameter_index < parameters_value->array.size(); ++parameter_index) {
            const Value& parameter = parameters_value->array[parameter_index];
            if (parameter.type != ValueType::string) {
                return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, record_path + ".parameters", "locked parameter names must be strings")};
            }
            if (std::find(seen_parameters.begin(), seen_parameters.end(), parameter.text) != seen_parameters.end()) {
                return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_lock, record_path + ".parameters", "duplicate parameter lock")};
            }
            if (instance == nullptr || !parameter_declared(registry, *instance, parameter.text)) {
                return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_lock, record_path + ".parameters", "locked parameter is not declared by the operator")};
            }
            seen_parameters.push_back(parameter.text);
            project.locks.parameters.push_back(ParameterLock{*instance_id, parameter.text});
        }
    }

    if (session->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.session", "session must be an object")};
    }
    if (!only_fields(*session, {"proxy_enabled", "proxy_max_width", "proxy_max_height", "proxy_method_version", "selected_instance_id"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, "$.session." + unexpected, "unexpected session field")};
    }
    const Value* proxy_enabled = find_field(*session, "proxy_enabled");
    const Value* proxy_width = find_field(*session, "proxy_max_width");
    const Value* proxy_height = find_field(*session, "proxy_max_height");
    const Value* proxy_version = find_field(*session, "proxy_method_version");
    const Value* selected = find_field(*session, "selected_instance_id");
    if (proxy_enabled == nullptr || proxy_width == nullptr || proxy_height == nullptr || proxy_version == nullptr || selected == nullptr) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, "$.session", "session is missing required fields")};
    }
    std::uint64_t width = 0U;
    std::uint64_t height = 0U;
    std::uint64_t method = 0U;
    if (proxy_enabled->type != ValueType::boolean || selected->type != ValueType::string ||
        !parse_u64_number(*proxy_width, width) || !parse_u64_number(*proxy_height, height) || !parse_u64_number(*proxy_version, method) ||
        width == 0U || height == 0U || width > std::numeric_limits<std::uint32_t>::max() ||
        height > std::numeric_limits<std::uint32_t>::max() || method != core::kProxyMethodVersion) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.session", "session proxy state is invalid or unsupported")};
    }
    if (!selected->text.empty()) {
        const auto selected_id = core::InstanceId::parse(selected->text);
        if (!selected_id.has_value() || !instance_exists(project.genome, *selected_id)) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.session.selected_instance_id", "selected instance is absent from the genome")};
        }
    }
    project.session.proxy_enabled = proxy_enabled->boolean;
    project.session.proxy_spec.max_width = static_cast<std::uint32_t>(width);
    project.session.proxy_spec.max_height = static_cast<std::uint32_t>(height);
    project.session.proxy_spec.method_version = static_cast<std::uint32_t>(method);
    project.session.selected_instance_id = selected->text;

    if (ui->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.ui", "ui must be an object")};
    }
    if (!only_fields(*ui, {"view_mode", "zoom_milli", "pan_x_milli", "pan_y_milli", "show_before"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, "$.ui." + unexpected, "unexpected ui field")};
    }
    const Value* view_mode = find_field(*ui, "view_mode");
    const Value* zoom = find_field(*ui, "zoom_milli");
    const Value* pan_x = find_field(*ui, "pan_x_milli");
    const Value* pan_y = find_field(*ui, "pan_y_milli");
    const Value* show_before = find_field(*ui, "show_before");
    if (view_mode == nullptr || zoom == nullptr || pan_x == nullptr || pan_y == nullptr || show_before == nullptr) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, "$.ui", "ui is missing required fields")};
    }
    std::int64_t zoom_milli = 0;
    std::int64_t pan_x_milli = 0;
    std::int64_t pan_y_milli = 0;
    if (view_mode->type != ValueType::string || show_before->type != ValueType::boolean ||
        (view_mode->text != "fit" && view_mode->text != "actual" && view_mode->text != "custom") ||
        !parse_i64_number(*zoom, zoom_milli) || !parse_i64_number(*pan_x, pan_x_milli) || !parse_i64_number(*pan_y, pan_y_milli) ||
        zoom_milli < 50 || zoom_milli > 64000) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.ui", "ui view state is invalid")};
    }
    project.ui.mode = view_mode->text;
    project.ui.zoom_milli = zoom_milli;
    project.ui.pan_x_milli = pan_x_milli;
    project.ui.pan_y_milli = pan_y_milli;
    project.ui.show_before = show_before->boolean;

    return ProjectParseResult{std::move(project), std::nullopt};
}

SourceReferenceStatus assess_source_reference(
    const std::string_view expected_identity,
    const std::optional<std::string>& actual_identity) noexcept {
    if (!actual_identity.has_value()) {
        return SourceReferenceStatus::missing;
    }
    return *actual_identity == expected_identity ? SourceReferenceStatus::identical : SourceReferenceStatus::changed;
}

}  // namespace faultmine::app
