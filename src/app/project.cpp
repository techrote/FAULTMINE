#include "faultmine/project.hpp"

#include "faultmine/crossover.hpp"
#include "../core/json.hpp"

#include <algorithm>
#include <charconv>
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

void lowercase_hex(std::string& text) noexcept {
    std::transform(text.begin(), text.end(), text.begin(), [](const unsigned char character) {
        return static_cast<char>(character >= 'A' && character <= 'F' ? character + ('a' - 'A') : character);
    });
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

[[nodiscard]] std::string genome_json(const core::Genome& genome) {
    std::string text = core::serialize_canonical_genome(genome);
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

[[nodiscard]] std::optional<core::Genome> parse_genome_value(
    const Value& value,
    const core::OperatorRegistry& registry,
    ProjectError& error,
    std::string path) {
    if (value.type != ValueType::object) {
        error = make_error(ProjectErrorCode::wrong_type, std::move(path), "genome must be an object");
        return std::nullopt;
    }
    std::string text = serialize_json_value(value);
    text.push_back('\n');
    const core::GenomeParseResult parsed = core::parse_genome(text, registry);
    if (!parsed.ok()) {
        std::string message{"embedded genome is invalid"};
        if (parsed.error.has_value()) message += ": " + parsed.error->path + ": " + parsed.error->message;
        error = make_error(ProjectErrorCode::invalid_genome, std::move(path), std::move(message));
        return std::nullopt;
    }
    return *parsed.genome;
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

[[nodiscard]] std::string serialize_locks(const LockState& lock_state) {
    std::vector<core::InstanceId> ids;
    ids.reserve(lock_state.operators.size() + lock_state.parameters.size());
    for (const core::InstanceId id : lock_state.operators) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
    }
    for (const ParameterLock& lock : lock_state.parameters) {
        if (std::find(ids.begin(), ids.end(), lock.instance_id) == ids.end()) ids.push_back(lock.instance_id);
    }
    std::sort(ids.begin(), ids.end(), [](const core::InstanceId left, const core::InstanceId right) {
        return left.to_string() < right.to_string();
    });

    std::string output{"["};
    for (std::size_t index = 0U; index < ids.size(); ++index) {
        if (index != 0U) output += ',';
        const core::InstanceId id = ids[index];
        const bool operator_locked = std::find(lock_state.operators.begin(), lock_state.operators.end(), id) != lock_state.operators.end();
        std::vector<std::string> parameters;
        for (const ParameterLock& lock : lock_state.parameters) {
            if (lock.instance_id == id) parameters.push_back(lock.parameter);
        }
        std::sort(parameters.begin(), parameters.end());
        parameters.erase(std::unique(parameters.begin(), parameters.end()), parameters.end());
        output += "{\"instance_id\":\"" + id.to_string() + "\",\"operator\":";
        output += operator_locked ? "true" : "false";
        output += ",\"parameters\":[";
        for (std::size_t parameter = 0U; parameter < parameters.size(); ++parameter) {
            if (parameter != 0U) output += ',';
            output += "\"" + core::json::escape_string(parameters[parameter]) + "\"";
        }
        output += "]}";
    }
    output += ']';
    return output;
}

[[nodiscard]] bool parse_locks(
    const Value& value,
    const core::Genome& genome,
    const core::OperatorRegistry& registry,
    const std::string& path,
    LockState& output,
    ProjectError& error) {
    if (value.type != ValueType::array) {
        error = make_error(ProjectErrorCode::wrong_type, path, "locks must be an array");
        return false;
    }
    std::vector<core::InstanceId> seen_records;
    for (std::size_t index = 0U; index < value.array.size(); ++index) {
        const Value& record = value.array[index];
        const std::string item_path = path + "[" + std::to_string(index) + "]";
        std::string unexpected;
        if (record.type != ValueType::object) {
            error = make_error(ProjectErrorCode::wrong_type, item_path, "lock record must be an object");
            return false;
        }
        if (!only_fields(record, {"instance_id", "operator", "parameters"}, unexpected)) {
            error = make_error(ProjectErrorCode::unexpected_field, item_path + "." + unexpected, "unexpected lock field");
            return false;
        }
        const Value* instance_value = find_field(record, "instance_id");
        const Value* operator_value = find_field(record, "operator");
        const Value* parameters_value = find_field(record, "parameters");
        if (instance_value == nullptr || operator_value == nullptr || parameters_value == nullptr) {
            error = make_error(ProjectErrorCode::missing_field, item_path, "lock record is missing required fields");
            return false;
        }
        if (instance_value->type != ValueType::string || operator_value->type != ValueType::boolean ||
            parameters_value->type != ValueType::array) {
            error = make_error(ProjectErrorCode::wrong_type, item_path, "lock field types are invalid");
            return false;
        }
        const auto instance_id = core::InstanceId::parse(instance_value->text);
        if (!instance_id.has_value() || find_instance(genome, *instance_id) == nullptr) {
            error = make_error(ProjectErrorCode::invalid_lock, item_path + ".instance_id", "lock instance is absent from the genome");
            return false;
        }
        if (std::find(seen_records.begin(), seen_records.end(), *instance_id) != seen_records.end()) {
            error = make_error(ProjectErrorCode::invalid_lock, item_path, "duplicate lock record for one operator instance");
            return false;
        }
        seen_records.push_back(*instance_id);
        if (operator_value->boolean) output.operators.push_back(*instance_id);
        const core::OperatorInstance* instance = find_instance(genome, *instance_id);
        std::vector<std::string> seen_parameters;
        for (const Value& parameter : parameters_value->array) {
            if (parameter.type != ValueType::string) {
                error = make_error(ProjectErrorCode::wrong_type, item_path + ".parameters", "locked parameter names must be strings");
                return false;
            }
            if (std::find(seen_parameters.begin(), seen_parameters.end(), parameter.text) != seen_parameters.end()) {
                error = make_error(ProjectErrorCode::invalid_lock, item_path + ".parameters", "duplicate parameter lock");
                return false;
            }
            if (instance == nullptr || !parameter_declared(registry, *instance, parameter.text)) {
                error = make_error(ProjectErrorCode::invalid_lock, item_path + ".parameters", "locked parameter is not declared by the operator");
                return false;
            }
            seen_parameters.push_back(parameter.text);
            output.parameters.push_back(ParameterLock{*instance_id, parameter.text});
        }
    }
    return true;
}

[[nodiscard]] std::string derivation_sort_key(const SpecimenDerivation& derivation) {
    std::string key{derivation_kind_name(derivation.kind)};
    key += ':' + std::to_string(derivation.policy_version);
    key += ':' + derivation.seed.to_string();
    key += ':' + std::to_string(derivation.descendant_index);
    key += ':' + derivation.mutation_radius;
    for (const std::string& parent : derivation.parent_specimen_ids) key += ':' + parent;
    return key;
}

[[nodiscard]] std::string serialize_derivation(SpecimenDerivation derivation) {
    std::sort(derivation.parent_specimen_ids.begin(), derivation.parent_specimen_ids.end());
    derivation.parent_specimen_ids.erase(
        std::unique(derivation.parent_specimen_ids.begin(), derivation.parent_specimen_ids.end()),
        derivation.parent_specimen_ids.end());
    std::string output{"{\"kind\":\""};
    output += derivation_kind_name(derivation.kind);
    output += "\",\"policy_version\":" + std::to_string(derivation.policy_version);
    output += ",\"seed\":\"" + derivation.seed.to_string() + "\"";
    output += ",\"descendant_index\":" + std::to_string(derivation.descendant_index);
    output += ",\"mutation_radius\":\"" + core::json::escape_string(derivation.mutation_radius) + "\"";
    output += ",\"parents\":[";
    for (std::size_t index = 0U; index < derivation.parent_specimen_ids.size(); ++index) {
        if (index != 0U) output += ',';
        output += "\"" + derivation.parent_specimen_ids[index] + "\"";
    }
    output += "]}";
    return output;
}

[[nodiscard]] std::string serialize_lineage(LineageState state) {
    std::sort(state.specimens.begin(), state.specimens.end(), [](const SpecimenRecord& left, const SpecimenRecord& right) {
        return left.specimen_id < right.specimen_id;
    });
    std::string output{"{\"active_specimen_id\":\""};
    output += state.active_specimen_id;
    output += "\",\"specimens\":[";
    for (std::size_t index = 0U; index < state.specimens.size(); ++index) {
        if (index != 0U) output += ',';
        SpecimenRecord record = state.specimens[index];
        std::sort(record.derivations.begin(), record.derivations.end(), [](const SpecimenDerivation& left, const SpecimenDerivation& right) {
            return derivation_sort_key(left) < derivation_sort_key(right);
        });
        output += "{\"specimen_id\":\"" + record.specimen_id + "\"";
        output += ",\"source_identity\":\"" + record.source_identity + "\"";
        output += ",\"genome\":" + genome_json(record.genome);
        output += ",\"locks\":" + serialize_locks(record.locks);
        output += ",\"favourite\":";
        output += record.favourite ? "true" : "false";
        output += ",\"creation_ordinal\":" + std::to_string(record.creation_ordinal);
        output += ",\"derivations\":[";
        for (std::size_t derivation = 0U; derivation < record.derivations.size(); ++derivation) {
            if (derivation != 0U) output += ',';
            output += serialize_derivation(record.derivations[derivation]);
        }
        output += "]}";
    }
    output += "]}";
    return output;
}

[[nodiscard]] std::optional<SpecimenDerivation> parse_derivation(
    const Value& value,
    const std::string& path,
    ProjectError& error) {
    std::string unexpected;
    if (value.type != ValueType::object) {
        error = make_error(ProjectErrorCode::wrong_type, path, "derivation must be an object");
        return std::nullopt;
    }
    if (!only_fields(value, {"kind", "policy_version", "seed", "descendant_index", "mutation_radius", "parents"}, unexpected)) {
        error = make_error(ProjectErrorCode::unexpected_field, path + "." + unexpected, "unexpected derivation field");
        return std::nullopt;
    }
    const Value* kind = find_field(value, "kind");
    const Value* policy = find_field(value, "policy_version");
    const Value* seed = find_field(value, "seed");
    const Value* descendant = find_field(value, "descendant_index");
    const Value* radius = find_field(value, "mutation_radius");
    const Value* parents = find_field(value, "parents");
    if (kind == nullptr || policy == nullptr || seed == nullptr || descendant == nullptr || radius == nullptr || parents == nullptr) {
        error = make_error(ProjectErrorCode::missing_field, path, "derivation is missing required fields");
        return std::nullopt;
    }
    std::uint64_t policy_value = 0U;
    std::uint64_t descendant_value = 0U;
    if (kind->type != ValueType::string || seed->type != ValueType::string || radius->type != ValueType::string ||
        parents->type != ValueType::array || !parse_u64_number(*policy, policy_value) ||
        !parse_u64_number(*descendant, descendant_value) || policy_value > std::numeric_limits<std::uint32_t>::max()) {
        error = make_error(ProjectErrorCode::wrong_type, path, "derivation field types are invalid");
        return std::nullopt;
    }
    const auto parsed_kind = parse_derivation_kind(kind->text);
    const auto parsed_seed = core::RootSeed::parse(seed->text);
    if (!parsed_kind.has_value() || !parsed_seed.has_value()) {
        error = make_error(ProjectErrorCode::invalid_value, path, "derivation kind or seed is invalid");
        return std::nullopt;
    }
    if (radius->text != "none" && radius->text != "low" && radius->text != "medium" && radius->text != "high") {
        error = make_error(ProjectErrorCode::invalid_value, path + ".mutation_radius", "unsupported mutation radius provenance value");
        return std::nullopt;
    }
    SpecimenDerivation output;
    output.kind = *parsed_kind;
    output.policy_version = static_cast<std::uint32_t>(policy_value);
    output.seed = *parsed_seed;
    output.descendant_index = descendant_value;
    output.mutation_radius = radius->text;
    for (const Value& parent : parents->array) {
        if (parent.type != ValueType::string || !valid_identity_hex(parent.text, 64U)) {
            error = make_error(ProjectErrorCode::invalid_lineage, path + ".parents", "parent specimen ids must be 64 hexadecimal digits");
            return std::nullopt;
        }
        std::string id = parent.text;
        lowercase_hex(id);
        output.parent_specimen_ids.push_back(std::move(id));
    }
    std::sort(output.parent_specimen_ids.begin(), output.parent_specimen_ids.end());
    if (std::adjacent_find(output.parent_specimen_ids.begin(), output.parent_specimen_ids.end()) != output.parent_specimen_ids.end()) {
        error = make_error(ProjectErrorCode::invalid_lineage, path + ".parents", "derivation repeats a parent specimen");
        return std::nullopt;
    }
    const bool root_kind = output.kind == DerivationKind::manual_root ||
        output.kind == DerivationKind::imported_genome || output.kind == DerivationKind::legacy_project_root;
    if (root_kind && (!output.parent_specimen_ids.empty() || output.policy_version != 0U || output.mutation_radius != "none")) {
        error = make_error(ProjectErrorCode::invalid_lineage, path, "root provenance must not fabricate parents or a derivation policy");
        return std::nullopt;
    }
    if (output.kind == DerivationKind::mutation &&
        (output.parent_specimen_ids.size() != 1U || output.policy_version != core::kMutationPolicyVersion || output.mutation_radius == "none")) {
        error = make_error(ProjectErrorCode::invalid_lineage, path, "mutation provenance is incomplete or uses an unsupported policy");
        return std::nullopt;
    }
    if (output.kind == DerivationKind::crossover &&
        (output.parent_specimen_ids.size() < 2U || output.policy_version != core::kCrossoverPolicyVersion || output.mutation_radius != "none")) {
        error = make_error(ProjectErrorCode::invalid_lineage, path, "crossover provenance is incomplete or uses an unsupported policy");
        return std::nullopt;
    }
    return output;
}

[[nodiscard]] std::optional<LineageState> parse_lineage(
    const Value& value,
    const core::OperatorRegistry& registry,
    const std::string& source_identity,
    ProjectError& error) {
    std::string unexpected;
    if (value.type != ValueType::object) {
        error = make_error(ProjectErrorCode::wrong_type, "$.lineage", "lineage must be an object");
        return std::nullopt;
    }
    if (!only_fields(value, {"active_specimen_id", "specimens"}, unexpected)) {
        error = make_error(ProjectErrorCode::unexpected_field, "$.lineage." + unexpected, "unexpected lineage field");
        return std::nullopt;
    }
    const Value* active = find_field(value, "active_specimen_id");
    const Value* specimens = find_field(value, "specimens");
    if (active == nullptr || specimens == nullptr) {
        error = make_error(ProjectErrorCode::missing_field, "$.lineage", "lineage is missing required fields");
        return std::nullopt;
    }
    if (active->type != ValueType::string || !valid_identity_hex(active->text, 64U) || specimens->type != ValueType::array) {
        error = make_error(ProjectErrorCode::wrong_type, "$.lineage", "lineage active id/specimen collection has invalid types");
        return std::nullopt;
    }
    LineageState state;
    state.active_specimen_id = active->text;
    lowercase_hex(state.active_specimen_id);
    for (std::size_t index = 0U; index < specimens->array.size(); ++index) {
        const Value& item = specimens->array[index];
        const std::string path = "$.lineage.specimens[" + std::to_string(index) + "]";
        if (item.type != ValueType::object) {
            error = make_error(ProjectErrorCode::wrong_type, path, "specimen must be an object");
            return std::nullopt;
        }
        if (!only_fields(item, {"specimen_id", "source_identity", "genome", "locks", "favourite", "creation_ordinal", "derivations"}, unexpected)) {
            error = make_error(ProjectErrorCode::unexpected_field, path + "." + unexpected, "unexpected specimen field");
            return std::nullopt;
        }
        const Value* id = find_field(item, "specimen_id");
        const Value* source = find_field(item, "source_identity");
        const Value* genome = find_field(item, "genome");
        const Value* locks = find_field(item, "locks");
        const Value* favourite = find_field(item, "favourite");
        const Value* ordinal = find_field(item, "creation_ordinal");
        const Value* derivations = find_field(item, "derivations");
        if (id == nullptr || source == nullptr || genome == nullptr || locks == nullptr || favourite == nullptr || ordinal == nullptr || derivations == nullptr) {
            error = make_error(ProjectErrorCode::missing_field, path, "specimen is missing required fields");
            return std::nullopt;
        }
        std::uint64_t creation_ordinal = 0U;
        if (id->type != ValueType::string || source->type != ValueType::string || favourite->type != ValueType::boolean ||
            derivations->type != ValueType::array || !parse_u64_number(*ordinal, creation_ordinal) ||
            !valid_identity_hex(id->text, 64U) || !valid_identity_hex(source->text, 64U)) {
            error = make_error(ProjectErrorCode::wrong_type, path, "specimen field types are invalid");
            return std::nullopt;
        }
        SpecimenRecord record;
        record.specimen_id = id->text;
        record.source_identity = source->text;
        lowercase_hex(record.specimen_id);
        lowercase_hex(record.source_identity);
        const auto parsed_genome = parse_genome_value(*genome, registry, error, path + ".genome");
        if (!parsed_genome.has_value()) return std::nullopt;
        record.genome = *parsed_genome;
        if (!parse_locks(*locks, record.genome, registry, path + ".locks", record.locks, error)) return std::nullopt;
        record.favourite = favourite->boolean;
        record.creation_ordinal = creation_ordinal;
        for (std::size_t derivation = 0U; derivation < derivations->array.size(); ++derivation) {
            const auto parsed = parse_derivation(
                derivations->array[derivation], path + ".derivations[" + std::to_string(derivation) + "]", error);
            if (!parsed.has_value()) return std::nullopt;
            record.derivations.push_back(*parsed);
        }
        state.specimens.push_back(std::move(record));
    }
    if (const auto validation = LineageGraph::validate_state(state, registry, source_identity); validation.has_value()) {
        error = make_error(ProjectErrorCode::invalid_lineage, "$.lineage", validation->message);
        return std::nullopt;
    }
    return state;
}

[[nodiscard]] std::string mode_or_fit(const std::string_view mode) {
    return mode == "fit" || mode == "actual" || mode == "custom" ? std::string{mode} : std::string{"fit"};
}

[[nodiscard]] LineageState synthesized_lineage(const ProjectDocument& project) {
    if (!project.lineage.specimens.empty()) return project.lineage;
    SpecimenRecord root;
    root.specimen_id = core::genome_identity_hex(project.genome);
    root.source_identity = project.source.source_identity;
    root.genome = project.genome;
    root.locks = project.locks;
    root.derivations.push_back(SpecimenDerivation{DerivationKind::manual_root, 0U, core::RootSeed{}, 0U, "none", {}});
    root.creation_ordinal = 0U;
    LineageState lineage;
    lineage.active_specimen_id = root.specimen_id;
    lineage.specimens.push_back(std::move(root));
    return lineage;
}

}  // namespace

std::string serialize_project_canonical(const ProjectDocument& project) {
    const LineageState lineage = synthesized_lineage(project);
    std::string output;
    output += "{\"project_version\":" + std::to_string(kProjectSchemaVersion);
    output += ",\"source\":{\"path\":\"" + core::json::escape_string(project.source.path_utf8) + "\"";
    output += ",\"identity\":\"" + core::json::escape_string(project.source.source_identity) + "\"}";
    output += ",\"genome\":" + genome_json(project.genome);
    output += ",\"locks\":" + serialize_locks(project.locks);
    output += ",\"lineage\":" + serialize_lineage(lineage);
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
        return ProjectParseResult{std::nullopt, make_error(
            parsed.error->kind == core::json::ParseErrorKind::duplicate_key ? ProjectErrorCode::duplicate_field : ProjectErrorCode::syntax,
            "$", parsed.error->message, parsed.error->offset)};
    }
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$", "project root must be an object")};
    }
    const Value& root = *parsed.value;
    const Value* version = find_field(root, "project_version");
    std::uint64_t version_value = 0U;
    if (version == nullptr || !parse_u64_number(*version, version_value) ||
        (version_value != kLegacyProjectSchemaVersion && version_value != kProjectSchemaVersion)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unsupported_version, "$.project_version", "unsupported project version")};
    }
    const bool legacy = version_value == kLegacyProjectSchemaVersion;
    std::string unexpected;
    const bool fields_ok = legacy
        ? only_fields(root, {"project_version", "source", "genome", "locks", "session", "ui"}, unexpected)
        : only_fields(root, {"project_version", "source", "genome", "locks", "lineage", "session", "ui"}, unexpected);
    if (!fields_ok) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::unexpected_field, "$." + unexpected, "unexpected project field")};
    }

    const Value* source = find_field(root, "source");
    const Value* genome_value = find_field(root, "genome");
    const Value* locks_value = find_field(root, "locks");
    const Value* lineage_value = find_field(root, "lineage");
    const Value* session = find_field(root, "session");
    const Value* ui = find_field(root, "ui");
    if (source == nullptr || genome_value == nullptr || locks_value == nullptr || session == nullptr || ui == nullptr ||
        (!legacy && lineage_value == nullptr)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::missing_field, "$", "project is missing a required top-level field")};
    }

    ProjectDocument project;
    project.project_version = kProjectSchemaVersion;
    if (source->type != ValueType::object || !only_fields(*source, {"path", "identity"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.source", "source object is invalid")};
    }
    const Value* path = find_field(*source, "path");
    const Value* identity = find_field(*source, "identity");
    if (path == nullptr || identity == nullptr || path->type != ValueType::string || identity->type != ValueType::string ||
        !valid_identity_hex(identity->text, 64U)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.source", "source path/identity are invalid")};
    }
    project.source.path_utf8 = path->text;
    project.source.source_identity = identity->text;
    lowercase_hex(project.source.source_identity);

    ProjectError detailed_error;
    const auto parsed_genome = parse_genome_value(*genome_value, registry, detailed_error, "$.genome");
    if (!parsed_genome.has_value()) return ProjectParseResult{std::nullopt, std::move(detailed_error)};
    project.genome = *parsed_genome;
    if (!parse_locks(*locks_value, project.genome, registry, "$.locks", project.locks, detailed_error)) {
        return ProjectParseResult{std::nullopt, std::move(detailed_error)};
    }

    if (legacy) {
        SpecimenRecord root_record;
        root_record.specimen_id = core::genome_identity_hex(project.genome);
        root_record.source_identity = project.source.source_identity;
        root_record.genome = project.genome;
        root_record.locks = project.locks;
        root_record.derivations.push_back(SpecimenDerivation{
            DerivationKind::legacy_project_root, 0U, core::RootSeed{}, 0U, "none", {}});
        root_record.creation_ordinal = 0U;
        project.lineage.active_specimen_id = root_record.specimen_id;
        project.lineage.specimens.push_back(std::move(root_record));
    } else {
        const auto parsed_lineage = parse_lineage(*lineage_value, registry, project.source.source_identity, detailed_error);
        if (!parsed_lineage.has_value()) return ProjectParseResult{std::nullopt, std::move(detailed_error)};
        project.lineage = *parsed_lineage;
        const auto active = std::find_if(
            project.lineage.specimens.begin(), project.lineage.specimens.end(),
            [&project](const SpecimenRecord& record) { return record.specimen_id == project.lineage.active_specimen_id; });
        if (active == project.lineage.specimens.end() || active->genome != project.genome || active->locks != project.locks) {
            return ProjectParseResult{std::nullopt, make_error(
                ProjectErrorCode::invalid_lineage, "$.lineage.active_specimen_id",
                "active lineage specimen must exactly match the top-level active genome and locks")};
        }
    }

    if (session->type != ValueType::object || !only_fields(
            *session, {"proxy_enabled", "proxy_max_width", "proxy_max_height", "proxy_method_version", "selected_instance_id"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.session", "session object is invalid")};
    }
    const Value* proxy_enabled = find_field(*session, "proxy_enabled");
    const Value* proxy_width = find_field(*session, "proxy_max_width");
    const Value* proxy_height = find_field(*session, "proxy_max_height");
    const Value* proxy_version = find_field(*session, "proxy_method_version");
    const Value* selected = find_field(*session, "selected_instance_id");
    std::uint64_t width = 0U;
    std::uint64_t height = 0U;
    std::uint64_t method = 0U;
    if (proxy_enabled == nullptr || proxy_width == nullptr || proxy_height == nullptr || proxy_version == nullptr || selected == nullptr ||
        proxy_enabled->type != ValueType::boolean || selected->type != ValueType::string ||
        !parse_u64_number(*proxy_width, width) || !parse_u64_number(*proxy_height, height) || !parse_u64_number(*proxy_version, method) ||
        width == 0U || height == 0U || width > std::numeric_limits<std::uint32_t>::max() ||
        height > std::numeric_limits<std::uint32_t>::max() || method != core::kProxyMethodVersion) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.session", "session proxy state is invalid or unsupported")};
    }
    if (!selected->text.empty()) {
        const auto selected_id = core::InstanceId::parse(selected->text);
        if (!selected_id.has_value() || find_instance(project.genome, *selected_id) == nullptr) {
            return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::invalid_value, "$.session.selected_instance_id", "selected instance is absent from the genome")};
        }
    }
    project.session.proxy_enabled = proxy_enabled->boolean;
    project.session.proxy_spec.max_width = static_cast<std::uint32_t>(width);
    project.session.proxy_spec.max_height = static_cast<std::uint32_t>(height);
    project.session.proxy_spec.method_version = static_cast<std::uint32_t>(method);
    project.session.selected_instance_id = selected->text;

    if (ui->type != ValueType::object || !only_fields(*ui, {"view_mode", "zoom_milli", "pan_x_milli", "pan_y_milli", "show_before"}, unexpected)) {
        return ProjectParseResult{std::nullopt, make_error(ProjectErrorCode::wrong_type, "$.ui", "ui object is invalid")};
    }
    const Value* view_mode = find_field(*ui, "view_mode");
    const Value* zoom = find_field(*ui, "zoom_milli");
    const Value* pan_x = find_field(*ui, "pan_x_milli");
    const Value* pan_y = find_field(*ui, "pan_y_milli");
    const Value* show_before = find_field(*ui, "show_before");
    std::int64_t zoom_milli = 0;
    std::int64_t pan_x_milli = 0;
    std::int64_t pan_y_milli = 0;
    if (view_mode == nullptr || zoom == nullptr || pan_x == nullptr || pan_y == nullptr || show_before == nullptr ||
        view_mode->type != ValueType::string || show_before->type != ValueType::boolean ||
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
    if (!actual_identity.has_value()) return SourceReferenceStatus::missing;
    return *actual_identity == expected_identity ? SourceReferenceStatus::identical : SourceReferenceStatus::changed;
}

}  // namespace faultmine::app
