#include "faultmine/project.hpp"

#include "../core/json.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

[[nodiscard]] ProjectParseResult fail(
    const ProjectErrorCode code,
    std::string path,
    std::string message) {
    return ProjectParseResult{std::nullopt, make_error(code, std::move(path), std::move(message))};
}

[[nodiscard]] const Value* find_field(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (const auto& field : object.object) {
        if (field.first == name) return &field.second;
    }
    return nullptr;
}

[[nodiscard]] bool only_fields(
    const Value& object,
    const std::initializer_list<std::string_view> allowed,
    std::string& unexpected) {
    if (object.type != ValueType::object) return false;
    for (const auto& field : object.object) {
        if (std::find(allowed.begin(), allowed.end(), field.first) == allowed.end()) {
            unexpected = field.first;
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_u64_number(const Value& value, std::uint64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty() || value.text.front() == '-') return false;
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto parsed = std::from_chars(first, last, output, 10);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] bool parse_i64_number(const Value& value, std::int64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty()) return false;
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto parsed = std::from_chars(first, last, output, 10);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

[[nodiscard]] bool valid_identity_hex(const std::string_view text, const std::size_t length) noexcept {
    if (text.size() != length) return false;
    return std::all_of(text.begin(), text.end(), [](const char character) {
        return (character >= '0' && character <= '9') ||
            (character >= 'a' && character <= 'f') ||
            (character >= 'A' && character <= 'F');
    });
}

[[nodiscard]] std::string lower_hex(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char character) {
        return static_cast<char>(character >= 'A' && character <= 'F'
            ? character + static_cast<unsigned char>('a' - 'A')
            : character);
    });
    return text;
}

[[nodiscard]] std::string serialize_json_value(const Value& value) {
    switch (value.type) {
        case ValueType::null_value: return "null";
        case ValueType::boolean: return value.boolean ? "true" : "false";
        case ValueType::number: return value.text;
        case ValueType::string: return "\"" + core::json::escape_string(value.text) + "\"";
        case ValueType::array: {
            std::string output{"["};
            for (std::size_t index = 0U; index < value.array.size(); ++index) {
                if (index != 0U) output += ',';
                output += serialize_json_value(value.array[index]);
            }
            output += ']';
            return output;
        }
        case ValueType::object: {
            std::string output{"{"};
            for (std::size_t index = 0U; index < value.object.size(); ++index) {
                if (index != 0U) output += ',';
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

[[nodiscard]] std::string embedded_genome_json(const core::Genome& genome) {
    std::string text = core::serialize_canonical_genome(genome);
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

[[nodiscard]] std::optional<core::Genome> parse_embedded_genome(
    const Value& value,
    const core::OperatorRegistry& registry,
    std::string& error) {
    if (value.type != ValueType::object) {
        error = "genome must be an object";
        return std::nullopt;
    }
    std::string text = serialize_json_value(value);
    text.push_back('\n');
    const core::GenomeParseResult parsed = core::parse_genome(text, registry);
    if (!parsed.ok()) {
        error = "embedded genome is invalid";
        if (parsed.error.has_value()) {
            error += ": " + parsed.error->path + ": " + parsed.error->message;
        }
        return std::nullopt;
    }
    return *parsed.genome;
}

[[nodiscard]] bool instance_exists(const core::Genome& genome, const core::InstanceId id) noexcept {
    return std::any_of(
        genome.operators.begin(), genome.operators.end(),
        [id](const core::OperatorInstance& instance) { return instance.instance_id == id; });
}

[[nodiscard]] const core::OperatorInstance* find_instance(
    const core::Genome& genome,
    const core::InstanceId id) noexcept {
    const auto found = std::find_if(
        genome.operators.begin(), genome.operators.end(),
        [id](const core::OperatorInstance& instance) { return instance.instance_id == id; });
    return found == genome.operators.end() ? nullptr : &*found;
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

[[nodiscard]] std::string serialize_locks(const LockState& state) {
    std::vector<core::InstanceId> ids;
    ids.reserve(state.operators.size() + state.parameters.size());
    for (const core::InstanceId id : state.operators) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    for (const ParameterLock& lock : state.parameters) {
        if (std::find(ids.begin(), ids.end(), lock.instance_id) == ids.end()) ids.push_back(lock.instance_id);
    }
    std::sort(ids.begin(), ids.end(), [](const core::InstanceId left, const core::InstanceId right) {
        return left.to_string() < right.to_string();
    });

    std::string output{"["};
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        if (index != 0U) output += ',';
        const core::InstanceId id = ids[index];
        const bool whole = std::find(state.operators.begin(), state.operators.end(), id) != state.operators.end();
        std::vector<std::string> parameters;
        for (const ParameterLock& lock : state.parameters) {
            if (lock.instance_id == id) parameters.push_back(lock.parameter);
        }
        std::sort(parameters.begin(), parameters.end());
        parameters.erase(std::unique(parameters.begin(), parameters.end()), parameters.end());
        output += "{\"instance_id\":\"" + id.to_string() + "\",\"operator\":";
        output += whole ? "true" : "false";
        output += ",\"parameters\":[";
        for (std::size_t parameter_index = 0U; parameter_index < parameters.size(); ++parameter_index) {
            if (parameter_index != 0U) output += ',';
            output += "\"" + core::json::escape_string(parameters[parameter_index]) + "\"";
        }
        output += "]}";
    }
    output += ']';
    return output;
}

[[nodiscard]] std::optional<ProjectError> parse_locks(
    const Value& value,
    const core::Genome& genome,
    const core::OperatorRegistry& registry,
    LockState& output) {
    if (value.type != ValueType::array) {
        return make_error(ProjectErrorCode::wrong_type, "$.locks", "locks must be an array");
    }
    std::vector<core::InstanceId> seen;
    for (std::size_t index = 0U; index < value.array.size(); ++index) {
        const Value& record = value.array[index];
        const std::string path = "$.locks[" + std::to_string(index) + "]";
        std::string unexpected;
        if (record.type != ValueType::object) {
            return make_error(ProjectErrorCode::wrong_type, path, "lock record must be an object");
        }
        if (!only_fields(record, {"instance_id", "operator", "parameters"}, unexpected)) {
            return make_error(ProjectErrorCode::unexpected_field, path + "." + unexpected, "unexpected lock field");
        }
        const Value* instance_value = find_field(record, "instance_id");
        const Value* operator_value = find_field(record, "operator");
        const Value* parameters_value = find_field(record, "parameters");
        if (instance_value == nullptr || operator_value == nullptr || parameters_value == nullptr) {
            return make_error(ProjectErrorCode::missing_field, path, "lock record is missing required fields");
        }
        if (instance_value->type != ValueType::string || operator_value->type != ValueType::boolean ||
            parameters_value->type != ValueType::array) {
            return make_error(ProjectErrorCode::wrong_type, path, "lock field types are invalid");
        }
        const auto instance_id = core::InstanceId::parse(instance_value->text);
        if (!instance_id.has_value() || !instance_exists(genome, *instance_id)) {
            return make_error(ProjectErrorCode::invalid_lock, path + ".instance_id", "lock instance is absent from the active genome");
        }
        if (std::find(seen.begin(), seen.end(), *instance_id) != seen.end()) {
            return make_error(ProjectErrorCode::invalid_lock, path, "duplicate lock record for one operator instance");
        }
        seen.push_back(*instance_id);
        if (operator_value->boolean) output.operators.push_back(*instance_id);
        const core::OperatorInstance* instance = find_instance(genome, *instance_id);
        std::vector<std::string> parameter_names;
        for (const Value& parameter : parameters_value->array) {
            if (parameter.type != ValueType::string) {
                return make_error(ProjectErrorCode::wrong_type, path + ".parameters", "locked parameter names must be strings");
            }
            if (std::find(parameter_names.begin(), parameter_names.end(), parameter.text) != parameter_names.end()) {
                return make_error(ProjectErrorCode::invalid_lock, path + ".parameters", "duplicate parameter lock");
            }
            if (instance == nullptr || !parameter_declared(registry, *instance, parameter.text)) {
                return make_error(ProjectErrorCode::invalid_lock, path + ".parameters", "locked parameter is not declared by the operator");
            }
            parameter_names.push_back(parameter.text);
            output.parameters.push_back(ParameterLock{*instance_id, parameter.text});
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::string serialize_derivation(const SpecimenDerivation& derivation) {
    std::string output{"{\"kind\":\""};
    output += derivation_kind_name(derivation.kind);
    output += "\",\"parents\":[";
    for (std::size_t index = 0U; index < derivation.parent_genome_identities.size(); ++index) {
        if (index != 0U) output += ',';
        output += "\"" + derivation.parent_genome_identities[index] + "\"";
    }
    output += "],\"policy_version\":" + std::to_string(derivation.policy_version);
    output += ",\"seed\":";
    output += derivation.seed.has_value() ? "\"" + derivation.seed->to_string() + "\"" : "null";
    output += ",\"descendant_index\":";
    output += derivation.descendant_index.has_value() ? std::to_string(*derivation.descendant_index) : "null";
    output += ",\"radius\":";
    output += derivation.mutation_radius.has_value()
        ? "\"" + std::string{core::mutation_radius_name(*derivation.mutation_radius)} + "\""
        : "null";
    output += '}';
    return output;
}

[[nodiscard]] std::string serialize_lineage(const LineageState& lineage) {
    std::vector<const SpecimenRecord*> ordered;
    ordered.reserve(lineage.specimens.size());
    for (const SpecimenRecord& record : lineage.specimens) ordered.push_back(&record);
    std::sort(ordered.begin(), ordered.end(), [](const SpecimenRecord* left, const SpecimenRecord* right) {
        if (left->creation_ordinal != right->creation_ordinal) return left->creation_ordinal < right->creation_ordinal;
        return left->genome_identity < right->genome_identity;
    });

    std::string output{"{\"active_genome_identity\":\""};
    output += lineage.active_genome_identity;
    output += "\",\"specimens\":[";
    for (std::size_t index = 0U; index < ordered.size(); ++index) {
        if (index != 0U) output += ',';
        const SpecimenRecord& record = *ordered[index];
        output += "{\"genome_identity\":\"" + record.genome_identity + "\"";
        output += ",\"source_identity\":\"" + record.source_identity + "\"";
        output += ",\"genome\":" + embedded_genome_json(record.genome);
        output += ",\"favourite\":";
        output += record.favourite ? "true" : "false";
        output += ",\"creation_ordinal\":" + std::to_string(record.creation_ordinal);
        output += ",\"derivation\":" + serialize_derivation(record.derivation) + '}';
    }
    output += "]}";
    return output;
}

[[nodiscard]] std::optional<ProjectError> parse_derivation(
    const Value& value,
    const std::string& path,
    SpecimenDerivation& output) {
    if (value.type != ValueType::object) {
        return make_error(ProjectErrorCode::wrong_type, path, "derivation must be an object");
    }
    std::string unexpected;
    if (!only_fields(value, {"kind", "parents", "policy_version", "seed", "descendant_index", "radius"}, unexpected)) {
        return make_error(ProjectErrorCode::unexpected_field, path + "." + unexpected, "unexpected derivation field");
    }
    const Value* kind = find_field(value, "kind");
    const Value* parents = find_field(value, "parents");
    const Value* policy = find_field(value, "policy_version");
    const Value* seed = find_field(value, "seed");
    const Value* descendant = find_field(value, "descendant_index");
    const Value* radius = find_field(value, "radius");
    if (kind == nullptr || parents == nullptr || policy == nullptr || seed == nullptr || descendant == nullptr || radius == nullptr) {
        return make_error(ProjectErrorCode::missing_field, path, "derivation is missing required fields");
    }
    if (kind->type != ValueType::string || parents->type != ValueType::array) {
        return make_error(ProjectErrorCode::wrong_type, path, "derivation kind/parents have invalid types");
    }
    const auto parsed_kind = parse_derivation_kind(kind->text);
    if (!parsed_kind.has_value()) {
        return make_error(ProjectErrorCode::invalid_lineage, path + ".kind", "unknown derivation kind");
    }
    output.kind = *parsed_kind;
    std::uint64_t policy_version = 0U;
    if (!parse_u64_number(*policy, policy_version) || policy_version > std::numeric_limits<std::uint32_t>::max()) {
        return make_error(ProjectErrorCode::invalid_lineage, path + ".policy_version", "invalid derivation policy version");
    }
    output.policy_version = static_cast<std::uint32_t>(policy_version);
    for (const Value& parent : parents->array) {
        if (parent.type != ValueType::string || !valid_identity_hex(parent.text, 64U)) {
            return make_error(ProjectErrorCode::invalid_lineage, path + ".parents", "parent identities must be 64 hexadecimal digits");
        }
        output.parent_genome_identities.push_back(lower_hex(parent.text));
    }
    if (seed->type == ValueType::string) {
        const auto parsed_seed = core::RootSeed::parse(seed->text);
        if (!parsed_seed.has_value()) {
            return make_error(ProjectErrorCode::invalid_lineage, path + ".seed", "invalid derivation seed");
        }
        output.seed = *parsed_seed;
    } else if (seed->type != ValueType::null_value) {
        return make_error(ProjectErrorCode::wrong_type, path + ".seed", "derivation seed must be a root-seed string or null");
    }
    if (descendant->type == ValueType::number) {
        std::uint64_t index = 0U;
        if (!parse_u64_number(*descendant, index)) {
            return make_error(ProjectErrorCode::invalid_lineage, path + ".descendant_index", "invalid descendant index");
        }
        output.descendant_index = index;
    } else if (descendant->type != ValueType::null_value) {
        return make_error(ProjectErrorCode::wrong_type, path + ".descendant_index", "descendant index must be unsigned or null");
    }
    if (radius->type == ValueType::string) {
        if (radius->text == "low") output.mutation_radius = core::MutationRadius::low;
        else if (radius->text == "medium") output.mutation_radius = core::MutationRadius::medium;
        else if (radius->text == "high") output.mutation_radius = core::MutationRadius::high;
        else return make_error(ProjectErrorCode::invalid_lineage, path + ".radius", "unknown mutation radius");
    } else if (radius->type != ValueType::null_value) {
        return make_error(ProjectErrorCode::wrong_type, path + ".radius", "mutation radius must be a string or null");
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ProjectError> parse_lineage(
    const Value& value,
    const core::OperatorRegistry& registry,
    const std::string_view source_identity,
    const core::Genome& active_genome,
    LineageState& output) {
    if (value.type != ValueType::object) {
        return make_error(ProjectErrorCode::wrong_type, "$.lineage", "lineage must be an object");
    }
    std::string unexpected;
    if (!only_fields(value, {"active_genome_identity", "specimens"}, unexpected)) {
        return make_error(ProjectErrorCode::unexpected_field, "$.lineage." + unexpected, "unexpected lineage field");
    }
    const Value* active = find_field(value, "active_genome_identity");
    const Value* specimens = find_field(value, "specimens");
    if (active == nullptr || specimens == nullptr) {
        return make_error(ProjectErrorCode::missing_field, "$.lineage", "lineage is missing required fields");
    }
    if (active->type != ValueType::string || !valid_identity_hex(active->text, 64U) || specimens->type != ValueType::array) {
        return make_error(ProjectErrorCode::invalid_lineage, "$.lineage", "lineage active identity/specimen collection is invalid");
    }
    output.active_genome_identity = lower_hex(active->text);

    for (std::size_t index = 0U; index < specimens->array.size(); ++index) {
        const Value& value_record = specimens->array[index];
        const std::string path = "$.lineage.specimens[" + std::to_string(index) + "]";
        if (value_record.type != ValueType::object) {
            return make_error(ProjectErrorCode::wrong_type, path, "specimen record must be an object");
        }
        if (!only_fields(value_record, {"genome_identity", "source_identity", "genome", "favourite", "creation_ordinal", "derivation"}, unexpected)) {
            return make_error(ProjectErrorCode::unexpected_field, path + "." + unexpected, "unexpected specimen field");
        }
        const Value* identity = find_field(value_record, "genome_identity");
        const Value* source = find_field(value_record, "source_identity");
        const Value* genome = find_field(value_record, "genome");
        const Value* favourite = find_field(value_record, "favourite");
        const Value* ordinal = find_field(value_record, "creation_ordinal");
        const Value* derivation = find_field(value_record, "derivation");
        if (identity == nullptr || source == nullptr || genome == nullptr || favourite == nullptr || ordinal == nullptr || derivation == nullptr) {
            return make_error(ProjectErrorCode::missing_field, path, "specimen record is missing required fields");
        }
        if (identity->type != ValueType::string || source->type != ValueType::string || favourite->type != ValueType::boolean ||
            !valid_identity_hex(identity->text, 64U) || !valid_identity_hex(source->text, 64U)) {
            return make_error(ProjectErrorCode::invalid_lineage, path, "specimen identity/source/favourite fields are invalid");
        }
        std::uint64_t creation_ordinal = 0U;
        if (!parse_u64_number(*ordinal, creation_ordinal)) {
            return make_error(ProjectErrorCode::invalid_lineage, path + ".creation_ordinal", "invalid specimen creation ordinal");
        }
        std::string genome_error;
        auto parsed_genome = parse_embedded_genome(*genome, registry, genome_error);
        if (!parsed_genome.has_value()) {
            return make_error(ProjectErrorCode::invalid_lineage, path + ".genome", std::move(genome_error));
        }
        SpecimenRecord record;
        record.genome_identity = lower_hex(identity->text);
        record.source_identity = lower_hex(source->text);
        record.genome = std::move(*parsed_genome);
        record.favourite = favourite->boolean;
        record.creation_ordinal = creation_ordinal;
        if (const auto derivation_error = parse_derivation(*derivation, path + ".derivation", record.derivation); derivation_error.has_value()) {
            return derivation_error;
        }
        output.specimens.push_back(std::move(record));
    }

    LineageGraph graph;
    std::string lineage_error;
    if (!graph.load(output, registry, source_identity, &lineage_error)) {
        return make_error(ProjectErrorCode::invalid_lineage, "$.lineage", std::move(lineage_error));
    }
    const SpecimenRecord* active_record = graph.active();
    if (active_record == nullptr || active_record->genome != active_genome ||
        active_record->genome_identity != core::genome_identity_hex(active_genome)) {
        return make_error(ProjectErrorCode::invalid_lineage, "$.lineage.active_genome_identity", "active lineage specimen does not match the project active genome");
    }
    output = graph.state();
    return std::nullopt;
}

[[nodiscard]] std::optional<ProjectError> parse_source(
    const Value& value,
    ProjectSourceReference& output) {
    if (value.type != ValueType::object) {
        return make_error(ProjectErrorCode::wrong_type, "$.source", "source must be an object");
    }
    std::string unexpected;
    if (!only_fields(value, {"path", "identity"}, unexpected)) {
        return make_error(ProjectErrorCode::unexpected_field, "$.source." + unexpected, "unexpected source field");
    }
    const Value* path = find_field(value, "path");
    const Value* identity = find_field(value, "identity");
    if (path == nullptr || identity == nullptr) {
        return make_error(ProjectErrorCode::missing_field, "$.source", "source path and identity are required");
    }
    if (path->type != ValueType::string || identity->type != ValueType::string || !valid_identity_hex(identity->text, 64U)) {
        return make_error(ProjectErrorCode::invalid_value, "$.source", "source path/identity are invalid");
    }
    output.path_utf8 = path->text;
    output.source_identity = lower_hex(identity->text);
    return std::nullopt;
}

[[nodiscard]] std::optional<ProjectError> parse_session(
    const Value& value,
    const core::Genome& genome,
    ProjectSessionState& output) {
    if (value.type != ValueType::object) {
        return make_error(ProjectErrorCode::wrong_type, "$.session", "session must be an object");
    }
    std::string unexpected;
    if (!only_fields(value, {"proxy_enabled", "proxy_max_width", "proxy_max_height", "proxy_method_version", "selected_instance_id"}, unexpected)) {
        return make_error(ProjectErrorCode::unexpected_field, "$.session." + unexpected, "unexpected session field");
    }
    const Value* proxy_enabled = find_field(value, "proxy_enabled");
    const Value* width = find_field(value, "proxy_max_width");
    const Value* height = find_field(value, "proxy_max_height");
    const Value* method = find_field(value, "proxy_method_version");
    const Value* selected = find_field(value, "selected_instance_id");
    if (proxy_enabled == nullptr || width == nullptr || height == nullptr || method == nullptr || selected == nullptr) {
        return make_error(ProjectErrorCode::missing_field, "$.session", "session is missing required fields");
    }
    std::uint64_t parsed_width = 0U;
    std::uint64_t parsed_height = 0U;
    std::uint64_t parsed_method = 0U;
    if (proxy_enabled->type != ValueType::boolean || selected->type != ValueType::string ||
        !parse_u64_number(*width, parsed_width) || !parse_u64_number(*height, parsed_height) ||
        !parse_u64_number(*method, parsed_method) || parsed_width == 0U || parsed_height == 0U ||
        parsed_width > std::numeric_limits<std::uint32_t>::max() ||
        parsed_height > std::numeric_limits<std::uint32_t>::max() || parsed_method != core::kProxyMethodVersion) {
        return make_error(ProjectErrorCode::invalid_value, "$.session", "session proxy state is invalid or unsupported");
    }
    if (!selected->text.empty()) {
        const auto id = core::InstanceId::parse(selected->text);
        if (!id.has_value() || !instance_exists(genome, *id)) {
            return make_error(ProjectErrorCode::invalid_value, "$.session.selected_instance_id", "selected instance is absent from the active genome");
        }
    }
    output.proxy_enabled = proxy_enabled->boolean;
    output.proxy_spec.max_width = static_cast<std::uint32_t>(parsed_width);
    output.proxy_spec.max_height = static_cast<std::uint32_t>(parsed_height);
    output.proxy_spec.method_version = static_cast<std::uint32_t>(parsed_method);
    output.selected_instance_id = selected->text;
    return std::nullopt;
}

[[nodiscard]] std::optional<ProjectError> parse_ui(const Value& value, ProjectViewState& output) {
    if (value.type != ValueType::object) {
        return make_error(ProjectErrorCode::wrong_type, "$.ui", "ui must be an object");
    }
    std::string unexpected;
    if (!only_fields(value, {"view_mode", "zoom_milli", "pan_x_milli", "pan_y_milli", "show_before"}, unexpected)) {
        return make_error(ProjectErrorCode::unexpected_field, "$.ui." + unexpected, "unexpected ui field");
    }
    const Value* mode = find_field(value, "view_mode");
    const Value* zoom = find_field(value, "zoom_milli");
    const Value* pan_x = find_field(value, "pan_x_milli");
    const Value* pan_y = find_field(value, "pan_y_milli");
    const Value* before = find_field(value, "show_before");
    if (mode == nullptr || zoom == nullptr || pan_x == nullptr || pan_y == nullptr || before == nullptr) {
        return make_error(ProjectErrorCode::missing_field, "$.ui", "ui is missing required fields");
    }
    std::int64_t parsed_zoom = 0;
    std::int64_t parsed_x = 0;
    std::int64_t parsed_y = 0;
    if (mode->type != ValueType::string || before->type != ValueType::boolean ||
        (mode->text != "fit" && mode->text != "actual" && mode->text != "custom") ||
        !parse_i64_number(*zoom, parsed_zoom) || !parse_i64_number(*pan_x, parsed_x) ||
        !parse_i64_number(*pan_y, parsed_y) || parsed_zoom < 50 || parsed_zoom > 64000) {
        return make_error(ProjectErrorCode::invalid_value, "$.ui", "ui view state is invalid");
    }
    output.mode = mode->text;
    output.zoom_milli = parsed_zoom;
    output.pan_x_milli = parsed_x;
    output.pan_y_milli = parsed_y;
    output.show_before = before->boolean;
    return std::nullopt;
}

}  // namespace

std::string serialize_project_canonical(const ProjectDocument& project) {
    std::string output;
    output += "{\"project_version\":" + std::to_string(kProjectSchemaVersion);
    output += ",\"source\":{\"path\":\"" + core::json::escape_string(project.source.path_utf8) + "\"";
    output += ",\"identity\":\"" + project.source.source_identity + "\"}";
    output += ",\"genome\":" + embedded_genome_json(project.genome);
    output += ",\"locks\":" + serialize_locks(project.locks);
    output += ",\"lineage\":" + serialize_lineage(project.lineage);
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
        return fail(ProjectErrorCode::wrong_type, "$", "project root must be an object");
    }
    const Value& root = *parsed.value;
    const Value* version = find_field(root, "project_version");
    if (version == nullptr) return fail(ProjectErrorCode::missing_field, "$.project_version", "project version is required");
    std::uint64_t parsed_version = 0U;
    if (!parse_u64_number(*version, parsed_version) ||
        (parsed_version != kLegacyProjectSchemaVersion && parsed_version != kProjectSchemaVersion)) {
        return fail(ProjectErrorCode::unsupported_version, "$.project_version", "unsupported project version");
    }

    std::string unexpected;
    if (parsed_version == kLegacyProjectSchemaVersion) {
        if (!only_fields(root, {"project_version", "source", "genome", "locks", "session", "ui"}, unexpected)) {
            return fail(ProjectErrorCode::unexpected_field, "$." + unexpected, "unexpected v1 project field");
        }
    } else if (!only_fields(root, {"project_version", "source", "genome", "locks", "lineage", "session", "ui"}, unexpected)) {
        return fail(ProjectErrorCode::unexpected_field, "$." + unexpected, "unexpected v2 project field");
    }

    const Value* source = find_field(root, "source");
    const Value* genome_value = find_field(root, "genome");
    const Value* locks = find_field(root, "locks");
    const Value* session = find_field(root, "session");
    const Value* ui = find_field(root, "ui");
    if (source == nullptr || genome_value == nullptr || locks == nullptr || session == nullptr || ui == nullptr) {
        return fail(ProjectErrorCode::missing_field, "$", "project is missing a required top-level field");
    }

    ProjectDocument project;
    project.project_version = kProjectSchemaVersion;
    if (const auto source_error = parse_source(*source, project.source); source_error.has_value()) {
        return ProjectParseResult{std::nullopt, source_error};
    }
    std::string genome_error;
    auto genome = parse_embedded_genome(*genome_value, registry, genome_error);
    if (!genome.has_value()) {
        return fail(ProjectErrorCode::invalid_genome, "$.genome", std::move(genome_error));
    }
    project.genome = std::move(*genome);
    if (const auto lock_error = parse_locks(*locks, project.genome, registry, project.locks); lock_error.has_value()) {
        return ProjectParseResult{std::nullopt, lock_error};
    }
    if (const auto session_error = parse_session(*session, project.genome, project.session); session_error.has_value()) {
        return ProjectParseResult{std::nullopt, session_error};
    }
    if (const auto ui_error = parse_ui(*ui, project.ui); ui_error.has_value()) {
        return ProjectParseResult{std::nullopt, ui_error};
    }

    if (parsed_version == kLegacyProjectSchemaVersion) {
        SpecimenRecord root_record;
        root_record.genome_identity = core::genome_identity_hex(project.genome);
        root_record.source_identity = project.source.source_identity;
        root_record.genome = project.genome;
        root_record.derivation = make_manual_root_derivation(DerivationKind::migrated_project);
        root_record.creation_ordinal = 0U;
        project.lineage.active_genome_identity = root_record.genome_identity;
        project.lineage.specimens.push_back(std::move(root_record));
    } else {
        const Value* lineage = find_field(root, "lineage");
        if (lineage == nullptr) return fail(ProjectErrorCode::missing_field, "$.lineage", "v2 project requires lineage");
        if (const auto lineage_error = parse_lineage(
                *lineage, registry, project.source.source_identity, project.genome, project.lineage);
            lineage_error.has_value()) {
            return ProjectParseResult{std::nullopt, lineage_error};
        }
    }

    return ProjectParseResult{std::move(project), std::nullopt};
}

SourceReferenceStatus assess_source_reference(
    const std::string_view expected_identity,
    const std::optional<std::string>& actual_identity) noexcept {
    if (!actual_identity.has_value()) return SourceReferenceStatus::missing;
    return *actual_identity == expected_identity ? SourceReferenceStatus::identical : SourceReferenceStatus::changed;
}

}  // namespace faultmine::app
