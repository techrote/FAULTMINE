#include "faultmine/genome.hpp"

#include "faultmine/sha256.hpp"
#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <limits>
#include <string>
#include <system_error>

namespace faultmine::core {
namespace {

[[nodiscard]] GenomeError make_error(
    const GenomeErrorCode code,
    std::string path,
    std::string message,
    const std::optional<std::size_t> byte_offset = std::nullopt) {
    return GenomeError{code, std::move(path), std::move(message), byte_offset};
}

[[nodiscard]] const json::Value* find_member(const json::Value& object, const std::string_view name) noexcept {
    if (object.type != json::ValueType::object) {
        return nullptr;
    }
    for (const auto& member : object.object) {
        if (member.first == name) {
            return &member.second;
        }
    }
    return nullptr;
}

[[nodiscard]] bool is_known_field(
    const std::string_view field,
    const std::initializer_list<std::string_view> allowed) noexcept {
    return std::find(allowed.begin(), allowed.end(), field) != allowed.end();
}

[[nodiscard]] bool check_only_fields(
    const json::Value& object,
    const std::initializer_list<std::string_view> allowed,
    const std::string_view path,
    std::optional<GenomeError>& error) {
    if (object.type != json::ValueType::object) {
        error = make_error(GenomeErrorCode::wrong_type, std::string{path}, "expected an object");
        return false;
    }
    for (const auto& member : object.object) {
        if (!is_known_field(member.first, allowed)) {
            error = make_error(
                GenomeErrorCode::unexpected_field,
                std::string{path} + "." + member.first,
                "unexpected field");
            return false;
        }
    }
    return true;
}

[[nodiscard]] const json::Value* require_member(
    const json::Value& object,
    const std::string_view name,
    const std::string_view path,
    std::optional<GenomeError>& error) {
    const json::Value* member = find_member(object, name);
    if (member == nullptr) {
        error = make_error(
            GenomeErrorCode::missing_field,
            std::string{path} + "." + std::string{name},
            "required field is missing");
    }
    return member;
}

[[nodiscard]] bool parse_u32(
    const json::Value& value,
    const std::string_view path,
    std::uint32_t& output,
    std::optional<GenomeError>& error) {
    if (value.type != json::ValueType::number) {
        error = make_error(GenomeErrorCode::wrong_type, std::string{path}, "expected an unsigned integer");
        return false;
    }
    if (value.text.empty() || value.text.front() == '-' || value.text.find_first_of(".eE") != std::string::npos) {
        error = make_error(GenomeErrorCode::invalid_value, std::string{path}, "expected an unsigned decimal integer");
        return false;
    }

    std::uint64_t parsed = 0;
    const auto conversion = std::from_chars(value.text.data(), value.text.data() + value.text.size(), parsed, 10);
    if (conversion.ec == std::errc::result_out_of_range || parsed > std::numeric_limits<std::uint32_t>::max()) {
        error = make_error(GenomeErrorCode::numeric_overflow, std::string{path}, "integer exceeds uint32 range");
        return false;
    }
    if (conversion.ec != std::errc{} || conversion.ptr != value.text.data() + value.text.size()) {
        error = make_error(GenomeErrorCode::invalid_value, std::string{path}, "invalid unsigned integer");
        return false;
    }
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_i64(
    const json::Value& value,
    const std::string_view path,
    std::int64_t& output,
    std::optional<GenomeError>& error) {
    if (value.type != json::ValueType::number || value.text.find_first_of(".eE") != std::string::npos) {
        error = make_error(GenomeErrorCode::wrong_type, std::string{path}, "expected a signed integer");
        return false;
    }
    const auto conversion = std::from_chars(value.text.data(), value.text.data() + value.text.size(), output, 10);
    if (conversion.ec == std::errc::result_out_of_range) {
        error = make_error(GenomeErrorCode::numeric_overflow, std::string{path}, "integer exceeds int64 range");
        return false;
    }
    if (conversion.ec != std::errc{} || conversion.ptr != value.text.data() + value.text.size()) {
        error = make_error(GenomeErrorCode::invalid_value, std::string{path}, "invalid signed integer");
        return false;
    }
    return true;
}

[[nodiscard]] bool parse_u64(
    const json::Value& value,
    const std::string_view path,
    std::uint64_t& output,
    std::optional<GenomeError>& error) {
    if (value.type != json::ValueType::number || value.text.empty() || value.text.front() == '-' ||
        value.text.find_first_of(".eE") != std::string::npos) {
        error = make_error(GenomeErrorCode::wrong_type, std::string{path}, "expected an unsigned integer");
        return false;
    }
    const auto conversion = std::from_chars(value.text.data(), value.text.data() + value.text.size(), output, 10);
    if (conversion.ec == std::errc::result_out_of_range) {
        error = make_error(GenomeErrorCode::numeric_overflow, std::string{path}, "integer exceeds uint64 range");
        return false;
    }
    if (conversion.ec != std::errc{} || conversion.ptr != value.text.data() + value.text.size()) {
        error = make_error(GenomeErrorCode::invalid_value, std::string{path}, "invalid unsigned integer");
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<ParameterValue> parse_parameter_value(
    const json::Value& value,
    const std::string& path,
    std::optional<GenomeError>& error) {
    if (!check_only_fields(value, {"kind", "value"}, path, error)) {
        return std::nullopt;
    }
    const json::Value* kind_value = require_member(value, "kind", path, error);
    const json::Value* payload = require_member(value, "value", path, error);
    if (kind_value == nullptr || payload == nullptr) {
        return std::nullopt;
    }
    if (kind_value->type != json::ValueType::string) {
        error = make_error(GenomeErrorCode::wrong_type, path + ".kind", "parameter kind must be a string");
        return std::nullopt;
    }

    if (kind_value->text == "bool") {
        if (payload->type != json::ValueType::boolean) {
            error = make_error(GenomeErrorCode::wrong_type, path + ".value", "bool parameter requires a boolean value");
            return std::nullopt;
        }
        return ParameterValue{payload->boolean};
    }
    if (kind_value->text == "i64") {
        std::int64_t parsed = 0;
        if (!parse_i64(*payload, path + ".value", parsed, error)) {
            return std::nullopt;
        }
        return ParameterValue{parsed};
    }
    if (kind_value->text == "u64") {
        std::uint64_t parsed = 0;
        if (!parse_u64(*payload, path + ".value", parsed, error)) {
            return std::nullopt;
        }
        return ParameterValue{parsed};
    }
    if (kind_value->text == "string") {
        if (payload->type != json::ValueType::string) {
            error = make_error(GenomeErrorCode::wrong_type, path + ".value", "string parameter requires a string value");
            return std::nullopt;
        }
        return ParameterValue{payload->text};
    }

    error = make_error(GenomeErrorCode::invalid_value, path + ".kind", "unknown parameter kind");
    return std::nullopt;
}

[[nodiscard]] std::optional<OperatorInstance> parse_operator(
    const json::Value& value,
    const std::size_t index,
    std::optional<GenomeError>& error) {
    const std::string path = "$.operators[" + std::to_string(index) + "]";
    if (!check_only_fields(
            value,
            {"instance_id", "type_id", "type_version", "enabled", "parameters"},
            path,
            error)) {
        return std::nullopt;
    }

    const json::Value* instance_value = require_member(value, "instance_id", path, error);
    const json::Value* type_value = require_member(value, "type_id", path, error);
    const json::Value* version_value = require_member(value, "type_version", path, error);
    const json::Value* enabled_value = require_member(value, "enabled", path, error);
    const json::Value* parameters_value = require_member(value, "parameters", path, error);
    if (instance_value == nullptr || type_value == nullptr || version_value == nullptr ||
        enabled_value == nullptr || parameters_value == nullptr) {
        return std::nullopt;
    }

    if (instance_value->type != json::ValueType::string) {
        error = make_error(GenomeErrorCode::wrong_type, path + ".instance_id", "instance_id must be a string");
        return std::nullopt;
    }
    const auto instance_id = InstanceId::parse(instance_value->text);
    if (!instance_id.has_value()) {
        error = make_error(GenomeErrorCode::invalid_value, path + ".instance_id", "instance_id must contain exactly 32 hexadecimal digits");
        return std::nullopt;
    }

    if (type_value->type != json::ValueType::string || type_value->text.empty()) {
        error = make_error(GenomeErrorCode::wrong_type, path + ".type_id", "type_id must be a non-empty string");
        return std::nullopt;
    }

    OperatorInstance instance;
    instance.instance_id = *instance_id;
    instance.type_id = type_value->text;
    if (!parse_u32(*version_value, path + ".type_version", instance.type_version, error)) {
        return std::nullopt;
    }
    if (enabled_value->type != json::ValueType::boolean) {
        error = make_error(GenomeErrorCode::wrong_type, path + ".enabled", "enabled must be a boolean");
        return std::nullopt;
    }
    instance.enabled = enabled_value->boolean;

    if (parameters_value->type != json::ValueType::object) {
        error = make_error(GenomeErrorCode::wrong_type, path + ".parameters", "parameters must be an object");
        return std::nullopt;
    }
    for (const auto& parameter : parameters_value->object) {
        const std::string parameter_path = path + ".parameters." + parameter.first;
        auto parsed_value = parse_parameter_value(parameter.second, parameter_path, error);
        if (!parsed_value.has_value()) {
            return std::nullopt;
        }
        instance.parameters.emplace(parameter.first, std::move(*parsed_value));
    }
    return instance;
}

[[nodiscard]] std::string serialize_parameter(const ParameterValue& value) {
    std::string output{"{\"kind\":\""};
    switch (parameter_kind(value)) {
        case ParameterKind::boolean:
            output += "bool\",\"value\":";
            output += std::get<bool>(value) ? "true" : "false";
            break;
        case ParameterKind::signed_integer:
            output += "i64\",\"value\":";
            output += std::to_string(std::get<std::int64_t>(value));
            break;
        case ParameterKind::unsigned_integer:
            output += "u64\",\"value\":";
            output += std::to_string(std::get<std::uint64_t>(value));
            break;
        case ParameterKind::text:
            output += "string\",\"value\":\"";
            output += json::escape_string(std::get<std::string>(value));
            output += '"';
            break;
    }
    output += '}';
    return output;
}

}  // namespace

bool OperatorRegistry::register_operator(OperatorDescriptor descriptor, std::string* error) {
    const auto set_error = [error](const std::string_view message) {
        if (error != nullptr) {
            *error = std::string{message};
        }
    };

    if (descriptor.type_id.empty() || !json::is_valid_utf8(descriptor.type_id)) {
        set_error("operator type_id must be non-empty valid UTF-8");
        return false;
    }
    if (descriptor.minimum_supported_version == 0U ||
        descriptor.current_version < descriptor.minimum_supported_version) {
        set_error("operator version range is invalid");
        return false;
    }
    if (find(descriptor.type_id) != nullptr) {
        set_error("operator type_id is already registered");
        return false;
    }

    for (std::size_t index = 0; index < descriptor.parameters.size(); ++index) {
        const ParameterDescriptor& parameter = descriptor.parameters[index];
        if (parameter.name.empty() || !json::is_valid_utf8(parameter.name)) {
            set_error("parameter name must be non-empty valid UTF-8");
            return false;
        }
        if (parameter.mutation.policy_version == 0U) {
            set_error("mutation metadata policy_version must be non-zero");
            return false;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (descriptor.parameters[previous].name == parameter.name) {
                set_error("parameter names must be unique within an operator descriptor");
                return false;
            }
        }
    }

    descriptors_.push_back(std::move(descriptor));
    return true;
}

const OperatorDescriptor* OperatorRegistry::find(const std::string_view type_id) const noexcept {
    for (const OperatorDescriptor& descriptor : descriptors_) {
        if (descriptor.type_id == type_id) {
            return &descriptor;
        }
    }
    return nullptr;
}

ParameterKind parameter_kind(const ParameterValue& value) noexcept {
    if (std::holds_alternative<bool>(value)) {
        return ParameterKind::boolean;
    }
    if (std::holds_alternative<std::int64_t>(value)) {
        return ParameterKind::signed_integer;
    }
    if (std::holds_alternative<std::uint64_t>(value)) {
        return ParameterKind::unsigned_integer;
    }
    return ParameterKind::text;
}

std::optional<GenomeError> validate_genome(const Genome& genome, const OperatorRegistry& registry) {
    if (genome.schema_version != kGenomeSchemaVersion) {
        return make_error(GenomeErrorCode::unsupported_version, "$.schema_version", "unsupported genome schema version");
    }
    if (genome.engine_contract_version != kEngineContractVersion) {
        return make_error(GenomeErrorCode::unsupported_version, "$.engine_contract_version", "unsupported engine contract version");
    }

    for (std::size_t index = 0; index < genome.operators.size(); ++index) {
        const OperatorInstance& instance = genome.operators[index];
        const std::string path = "$.operators[" + std::to_string(index) + "]";

        for (std::size_t previous = 0; previous < index; ++previous) {
            if (genome.operators[previous].instance_id == instance.instance_id) {
                return make_error(GenomeErrorCode::duplicate_instance_id, path + ".instance_id", "operator instance IDs must be unique");
            }
        }

        const OperatorDescriptor* descriptor = registry.find(instance.type_id);
        if (descriptor == nullptr) {
            return make_error(GenomeErrorCode::unknown_operator, path + ".type_id", "operator type is not registered");
        }
        if (instance.type_version < descriptor->minimum_supported_version ||
            instance.type_version > descriptor->current_version) {
            return make_error(GenomeErrorCode::unsupported_version, path + ".type_version", "operator version is not supported by the registry");
        }

        for (const auto& parameter : instance.parameters) {
            const auto descriptor_it = std::find_if(
                descriptor->parameters.begin(),
                descriptor->parameters.end(),
                [&parameter](const ParameterDescriptor& candidate) { return candidate.name == parameter.first; });
            if (descriptor_it == descriptor->parameters.end()) {
                return make_error(GenomeErrorCode::invalid_parameter, path + ".parameters." + parameter.first, "parameter is not declared by the operator descriptor");
            }
            if (parameter_kind(parameter.second) != descriptor_it->kind) {
                return make_error(GenomeErrorCode::invalid_parameter, path + ".parameters." + parameter.first, "parameter kind does not match the operator descriptor");
            }
            if (std::holds_alternative<std::string>(parameter.second) &&
                !json::is_valid_utf8(std::get<std::string>(parameter.second))) {
                return make_error(GenomeErrorCode::invalid_parameter, path + ".parameters." + parameter.first, "string parameter is not valid UTF-8");
            }
        }

        for (const ParameterDescriptor& parameter_descriptor : descriptor->parameters) {
            if (parameter_descriptor.required && instance.parameters.find(parameter_descriptor.name) == instance.parameters.end()) {
                return make_error(GenomeErrorCode::missing_field, path + ".parameters." + parameter_descriptor.name, "required operator parameter is missing");
            }
        }
    }
    return std::nullopt;
}

std::string serialize_canonical_genome(const Genome& genome) {
    std::string output;
    output += "{\"schema_version\":" + std::to_string(genome.schema_version);
    output += ",\"engine_contract_version\":" + std::to_string(genome.engine_contract_version);
    output += ",\"root_seed\":\"" + genome.root_seed.to_string() + "\"";
    output += ",\"operators\":[";

    for (std::size_t index = 0; index < genome.operators.size(); ++index) {
        if (index != 0U) {
            output += ',';
        }
        const OperatorInstance& instance = genome.operators[index];
        output += "{\"instance_id\":\"" + instance.instance_id.to_string() + "\"";
        output += ",\"type_id\":\"" + json::escape_string(instance.type_id) + "\"";
        output += ",\"type_version\":" + std::to_string(instance.type_version);
        output += ",\"enabled\":";
        output += instance.enabled ? "true" : "false";
        output += ",\"parameters\":{";

        bool first_parameter = true;
        for (const auto& parameter : instance.parameters) {
            if (!first_parameter) {
                output += ',';
            }
            first_parameter = false;
            output += '"';
            output += json::escape_string(parameter.first);
            output += "\":";
            output += serialize_parameter(parameter.second);
        }
        output += "}}";
    }

    output += "]}\n";
    return output;
}

GenomeParseResult parse_genome(const std::string_view text, const OperatorRegistry& registry) {
    const json::ParseResult json_result = json::parse(text);
    if (!json_result.value.has_value()) {
        const json::ParseError& parse_error = *json_result.error;
        const GenomeErrorCode code = parse_error.kind == json::ParseErrorKind::duplicate_key
            ? GenomeErrorCode::duplicate_field
            : GenomeErrorCode::syntax_error;
        return GenomeParseResult{
            std::nullopt,
            make_error(code, "$", parse_error.message, parse_error.offset)};
    }

    const json::Value& root = *json_result.value;
    std::optional<GenomeError> error;
    if (!check_only_fields(
            root,
            {"schema_version", "engine_contract_version", "root_seed", "operators"},
            "$",
            error)) {
        return GenomeParseResult{std::nullopt, std::move(error)};
    }

    const json::Value* schema_value = require_member(root, "schema_version", "$", error);
    const json::Value* engine_value = require_member(root, "engine_contract_version", "$", error);
    const json::Value* seed_value = require_member(root, "root_seed", "$", error);
    const json::Value* operators_value = require_member(root, "operators", "$", error);
    if (schema_value == nullptr || engine_value == nullptr || seed_value == nullptr || operators_value == nullptr) {
        return GenomeParseResult{std::nullopt, std::move(error)};
    }

    Genome genome;
    if (!parse_u32(*schema_value, "$.schema_version", genome.schema_version, error) ||
        !parse_u32(*engine_value, "$.engine_contract_version", genome.engine_contract_version, error)) {
        return GenomeParseResult{std::nullopt, std::move(error)};
    }
    if (genome.schema_version != kGenomeSchemaVersion) {
        return GenomeParseResult{std::nullopt, make_error(GenomeErrorCode::unsupported_version, "$.schema_version", "unsupported genome schema version")};
    }
    if (genome.engine_contract_version != kEngineContractVersion) {
        return GenomeParseResult{std::nullopt, make_error(GenomeErrorCode::unsupported_version, "$.engine_contract_version", "unsupported engine contract version")};
    }

    if (seed_value->type != json::ValueType::string) {
        return GenomeParseResult{std::nullopt, make_error(GenomeErrorCode::wrong_type, "$.root_seed", "root_seed must be a hexadecimal string")};
    }
    const auto root_seed = RootSeed::parse(seed_value->text);
    if (!root_seed.has_value()) {
        return GenomeParseResult{std::nullopt, make_error(GenomeErrorCode::invalid_value, "$.root_seed", "root_seed must contain exactly 16 hexadecimal digits")};
    }
    genome.root_seed = *root_seed;

    if (operators_value->type != json::ValueType::array) {
        return GenomeParseResult{std::nullopt, make_error(GenomeErrorCode::wrong_type, "$.operators", "operators must be an array")};
    }
    genome.operators.reserve(operators_value->array.size());
    for (std::size_t index = 0; index < operators_value->array.size(); ++index) {
        auto instance = parse_operator(operators_value->array[index], index, error);
        if (!instance.has_value()) {
            return GenomeParseResult{std::nullopt, std::move(error)};
        }
        genome.operators.push_back(std::move(*instance));
    }

    if (auto validation_error = validate_genome(genome, registry); validation_error.has_value()) {
        return GenomeParseResult{std::nullopt, std::move(validation_error)};
    }
    return GenomeParseResult{std::move(genome), std::nullopt};
}

std::string genome_identity_hex(const Genome& genome) {
    return sha256_hex(serialize_canonical_genome(genome));
}

}  // namespace faultmine::core
