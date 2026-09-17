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

namespace faultmine::app {
inline constexpr std::uint32_t kProjectSchemaVersionFm010 = 2U;
}

#define kProjectSchemaVersion kProjectSchemaVersionFm010
#define serialize_project_canonical serialize_project_canonical_fm010
#define parse_project parse_project_fm010
#include "project.cpp"
#undef parse_project
#undef serialize_project_canonical
#undef kProjectSchemaVersion

namespace faultmine::app {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] const Value* field(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (const auto& item : object.object) {
        if (item.first == name) return &item.second;
    }
    return nullptr;
}

[[nodiscard]] Value* mutable_field(Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (auto& item : object.object) {
        if (item.first == name) return &item.second;
    }
    return nullptr;
}

[[nodiscard]] std::string json_string(const std::string_view text) {
    return "\"" + core::json::escape_string(text) + "\"";
}

[[nodiscard]] std::string serialize_value(const Value& value) {
    switch (value.type) {
        case ValueType::null_value: return "null";
        case ValueType::boolean: return value.boolean ? "true" : "false";
        case ValueType::number: return value.text;
        case ValueType::string: return json_string(value.text);
        case ValueType::array: {
            std::string output{"["};
            for (std::size_t index = 0U; index < value.array.size(); ++index) {
                if (index != 0U) output += ',';
                output += serialize_value(value.array[index]);
            }
            output += ']';
            return output;
        }
        case ValueType::object: {
            std::string output{"{"};
            for (std::size_t index = 0U; index < value.object.size(); ++index) {
                if (index != 0U) output += ',';
                output += json_string(value.object[index].first) + ':' + serialize_value(value.object[index].second);
            }
            output += '}';
            return output;
        }
    }
    return "null";
}

[[nodiscard]] bool parse_u32(const Value& value, std::uint32_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty() || value.text.front() == '-') return false;
    std::uint64_t parsed{};
    const char* begin = value.text.data();
    const char* end = begin + value.text.size();
    const auto result = std::from_chars(begin, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] std::optional<Value> make_laboratory_value(const core::MaterializedLaboratorySource& source) {
    std::string validation_error;
    if (!core::validate_materialized_laboratory_source(source, &validation_error)) return std::nullopt;
    std::string provenance_text = core::serialize_laboratory_provenance_json(source.provenance);
    if (!provenance_text.empty() && provenance_text.back() == '\n') provenance_text.pop_back();
    const std::string object_text =
        "{\"width\":" + std::to_string(source.image.width) +
        ",\"height\":" + std::to_string(source.image.height) +
        ",\"pixels_rgba8_hex\":" + json_string(core::bytes_to_hex(source.image.bytes)) +
        ",\"provenance\":" + provenance_text + "}";
    const auto parsed = core::json::parse(object_text);
    return parsed.value;
}

[[nodiscard]] std::optional<core::MaterializedLaboratorySource> parse_laboratory_value(
    const Value& value,
    std::string& error) {
    if (value.type != ValueType::object) {
        error = "laboratory_source must be an object or null";
        return std::nullopt;
    }
    for (const auto& item : value.object) {
        if (item.first != "width" && item.first != "height" && item.first != "pixels_rgba8_hex" && item.first != "provenance") {
            error = "unexpected laboratory_source field: " + item.first;
            return std::nullopt;
        }
    }
    const Value* width = field(value, "width");
    const Value* height = field(value, "height");
    const Value* pixels = field(value, "pixels_rgba8_hex");
    const Value* provenance = field(value, "provenance");
    std::uint32_t parsed_width{};
    std::uint32_t parsed_height{};
    if (width == nullptr || height == nullptr || pixels == nullptr || provenance == nullptr ||
        !parse_u32(*width, parsed_width) || !parse_u32(*height, parsed_height) ||
        pixels->type != ValueType::string || provenance->type != ValueType::object) {
        error = "laboratory_source fields are missing or invalid";
        return std::nullopt;
    }
    const auto expected = core::canonical_rgba8_byte_size(parsed_width, parsed_height);
    if (!expected.has_value() || *expected > 256ULL * 1024ULL * 1024ULL) {
        error = "laboratory_source dimensions exceed the project v3 embedded-pixel bound";
        return std::nullopt;
    }
    auto decoded = core::bytes_from_hex(pixels->text, *expected, &error);
    if (!decoded.has_value() || decoded->size() != *expected) {
        if (error.empty()) error = "laboratory_source pixel payload length does not match dimensions";
        return std::nullopt;
    }
    auto image = core::make_rgba8_image(parsed_width, parsed_height, *decoded);
    if (!image.ok()) {
        error = image.error.has_value() ? image.error->message : "laboratory_source image is invalid";
        return std::nullopt;
    }
    std::string provenance_text = serialize_value(*provenance);
    provenance_text.push_back('\n');
    auto parsed_provenance = core::parse_laboratory_provenance_json(provenance_text, &error);
    if (!parsed_provenance.has_value()) return std::nullopt;
    core::MaterializedLaboratorySource source{std::move(*image.image), std::move(*parsed_provenance)};
    if (!core::validate_materialized_laboratory_source(source, &error)) return std::nullopt;
    return source;
}

[[nodiscard]] ProjectParseResult laboratory_error(std::string message) {
    return ProjectParseResult{
        std::nullopt,
        ProjectError{ProjectErrorCode::invalid_laboratory_source, "$.laboratory_source", std::move(message), std::nullopt}};
}

}  // namespace

std::string serialize_project_canonical(const ProjectDocument& project) {
    ProjectDocument legacy = project;
    legacy.project_version = kLineageProjectSchemaVersion;
    legacy.laboratory_source.reset();
    const std::string legacy_text = serialize_project_canonical_fm010(legacy);
    auto parsed = core::json::parse(legacy_text);
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) return {};
    Value root = std::move(*parsed.value);
    Value* version = mutable_field(root, "project_version");
    if (version == nullptr) return {};
    version->type = ValueType::number;
    version->text = std::to_string(kProjectSchemaVersion);

    Value laboratory_value;
    if (project.laboratory_source.has_value()) {
        auto value = make_laboratory_value(*project.laboratory_source);
        if (!value.has_value()) return {};
        laboratory_value = std::move(*value);
    } else {
        laboratory_value.type = ValueType::null_value;
    }
    const auto source_position = std::find_if(root.object.begin(), root.object.end(), [](const auto& item) { return item.first == "source"; });
    root.object.insert(source_position == root.object.end() ? root.object.end() : source_position + 1,
        std::make_pair(std::string{"laboratory_source"}, std::move(laboratory_value)));
    return serialize_value(root) + '\n';
}

ProjectParseResult parse_project(const std::string_view text, const core::OperatorRegistry& registry) {
    const auto parsed = core::json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) {
        return parse_project_fm010(text, registry);
    }
    const Value* version_value = field(*parsed.value, "project_version");
    std::uint32_t version{};
    if (version_value == nullptr || !parse_u32(*version_value, version)) return parse_project_fm010(text, registry);
    if (version == kLegacyProjectSchemaVersion || version == kLineageProjectSchemaVersion) {
        ProjectParseResult result = parse_project_fm010(text, registry);
        if (result.ok()) result.project->project_version = kProjectSchemaVersion;
        return result;
    }
    if (version != kProjectSchemaVersion) {
        return ProjectParseResult{
            std::nullopt,
            ProjectError{ProjectErrorCode::unsupported_version, "$.project_version", "unsupported project version", std::nullopt}};
    }

    Value root = *parsed.value;
    auto lab_iterator = std::find_if(root.object.begin(), root.object.end(), [](const auto& item) { return item.first == "laboratory_source"; });
    if (lab_iterator == root.object.end()) {
        return laboratory_error("project v3 requires an explicit laboratory_source field (null for ordinary sources)");
    }
    Value lab_value = lab_iterator->second;
    root.object.erase(lab_iterator);
    Value* mutable_version = mutable_field(root, "project_version");
    if (mutable_version == nullptr) return laboratory_error("project version field disappeared during v3 normalization");
    mutable_version->text = std::to_string(kLineageProjectSchemaVersion);
    std::string legacy_text = serialize_value(root);
    legacy_text.push_back('\n');
    ProjectParseResult result = parse_project_fm010(legacy_text, registry);
    if (!result.ok()) return result;
    result.project->project_version = kProjectSchemaVersion;

    if (lab_value.type == ValueType::null_value) return result;
    std::string lab_error;
    auto source = parse_laboratory_value(lab_value, lab_error);
    if (!source.has_value()) return laboratory_error(std::move(lab_error));
    if (source->provenance.materialized_source_identity != result.project->source.source_identity ||
        core::source_identity_hex(source->image) != result.project->source.source_identity) {
        return laboratory_error("embedded frozen pixels do not match the project source identity");
    }
    result.project->laboratory_source = std::move(*source);
    return result;
}

}  // namespace faultmine::app
