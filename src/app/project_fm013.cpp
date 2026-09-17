#include "faultmine/project.hpp"

#define serialize_project_canonical serialize_project_canonical_fm012
#define parse_project parse_project_fm012
#include "project.cpp"
#undef parse_project
#undef serialize_project_canonical

#include "../core/json.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace faultmine::app {
namespace {

[[nodiscard]] core::json::Value* mutable_field(core::json::Value& value, const std::string_view name) noexcept {
    if (value.type != core::json::ValueType::object) return nullptr;
    for (auto& pair : value.object) {
        if (pair.first == name) return &pair.second;
    }
    return nullptr;
}

[[nodiscard]] ProjectParseResult laboratory_project_fail(std::string path, std::string message) {
    return ProjectParseResult{
        std::nullopt,
        ProjectError{ProjectErrorCode::invalid_value, std::move(path), std::move(message), std::nullopt}};
}

[[nodiscard]] std::string without_trailing_newline(std::string text) {
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

}  // namespace

std::string serialize_project_canonical(const ProjectDocument& project) {
    ProjectDocument ordinary = project;
    ordinary.source.laboratory.reset();
    std::string output = serialize_project_canonical_fm012(ordinary);
    if (!project.source.laboratory.has_value()) return output;

    if (const auto validation = laboratory::validate_materialized_source(*project.source.laboratory); validation.has_value()) {
        return {};
    }
    const std::string frozen = without_trailing_newline(
        laboratory::serialize_materialized_source(*project.source.laboratory));
    const std::size_t genome_field = output.find(",\"genome\":");
    if (genome_field == std::string::npos || genome_field == 0U || output[genome_field - 1U] != '}') return {};
    output.insert(genome_field - 1U, ",\"laboratory\":" + frozen);
    return output;
}

ProjectParseResult parse_project(
    const std::string_view text,
    const core::OperatorRegistry& registry) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != core::json::ValueType::object) {
        return parse_project_fm012(text, registry);
    }
    const core::json::Value* source = find_field(*parsed.value, "source");
    const core::json::Value* laboratory_value = source == nullptr ? nullptr : find_field(*source, "laboratory");
    if (laboratory_value == nullptr) return parse_project_fm012(text, registry);

    const core::json::Value* version_value = find_field(*parsed.value, "project_version");
    std::uint64_t version{};
    if (version_value == nullptr || !parse_u64_number(*version_value, version) || version != kProjectSchemaVersion) {
        return laboratory_project_fail("$.source.laboratory", "laboratory source extension is valid only for project schema v2");
    }

    const laboratory::MaterializedParseResult frozen = laboratory::parse_materialized_source(
        serialize_json_value(*laboratory_value));
    if (!frozen.ok()) {
        return laboratory_project_fail("$.source.laboratory", frozen.error);
    }

    core::json::Value stripped = *parsed.value;
    core::json::Value* mutable_source = mutable_field(stripped, "source");
    if (mutable_source == nullptr || mutable_source->type != core::json::ValueType::object) {
        return laboratory_project_fail("$.source", "source must be an object");
    }
    mutable_source->object.erase(
        std::remove_if(
            mutable_source->object.begin(), mutable_source->object.end(),
            [](const auto& pair) { return pair.first == "laboratory"; }),
        mutable_source->object.end());
    std::string ordinary_text = serialize_json_value(stripped);
    ordinary_text.push_back('\n');
    ProjectParseResult result = parse_project_fm012(ordinary_text, registry);
    if (!result.ok()) return result;

    const std::string frozen_identity = core::source_identity_hex(frozen.source->image);
    if (frozen_identity != result.project->source.source_identity ||
        !frozen.source->provenance.materialized_source_identity.has_value() ||
        *frozen.source->provenance.materialized_source_identity != result.project->source.source_identity) {
        return laboratory_project_fail(
            "$.source.laboratory",
            "frozen laboratory pixels/provenance do not match the project source identity");
    }
    result.project->source.laboratory = *frozen.source;
    return result;
}

}  // namespace faultmine::app
