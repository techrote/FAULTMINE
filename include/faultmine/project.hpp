#pragma once

#include "faultmine/editor.hpp"
#include "faultmine/laboratory.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/proxy.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace faultmine::app {

inline constexpr std::uint32_t kLegacyProjectSchemaVersion = 1U;
inline constexpr std::uint32_t kProjectSchemaVersion = 2U;

struct ProjectSourceReference {
    std::string path_utf8;
    std::string source_identity;
    // FM-013 optional v2 extension. When present, the normalized pixels are
    // embedded alongside external-decoder provenance so project reopening does
    // not require rerunning the malformed-codec experiment.
    std::optional<laboratory::MaterializedSource> laboratory;

    bool operator==(const ProjectSourceReference&) const = default;
};

struct ProjectSessionState {
    bool proxy_enabled{true};
    core::ProxySpec proxy_spec{};
    std::string selected_instance_id;

    bool operator==(const ProjectSessionState&) const = default;
};

struct ProjectViewState {
    std::string mode{"fit"};
    std::int64_t zoom_milli{1000};
    std::int64_t pan_x_milli{};
    std::int64_t pan_y_milli{};
    bool show_before{};

    bool operator==(const ProjectViewState&) const = default;
};

struct ProjectDocument {
    std::uint32_t project_version{kProjectSchemaVersion};
    ProjectSourceReference source;
    core::Genome genome;
    LockState locks;
    LineageState lineage;
    ProjectSessionState session;
    ProjectViewState ui;

    bool operator==(const ProjectDocument&) const = default;
};

enum class ProjectErrorCode {
    syntax,
    duplicate_field,
    missing_field,
    unexpected_field,
    wrong_type,
    invalid_value,
    unsupported_version,
    invalid_genome,
    invalid_lock,
    invalid_lineage,
};

struct ProjectError {
    ProjectErrorCode code{ProjectErrorCode::invalid_value};
    std::string path;
    std::string message;
    std::optional<std::size_t> byte_offset;
};

struct ProjectParseResult {
    std::optional<ProjectDocument> project;
    std::optional<ProjectError> error;

    [[nodiscard]] bool ok() const noexcept {
        return project.has_value() && !error.has_value();
    }
};

enum class SourceReferenceStatus {
    missing,
    identical,
    changed,
};

// Serialization emits project schema v2 plus the optional FM-013 laboratory
// source extension. Parsing accepts v1, ordinary v2, and v2 with that extension.
[[nodiscard]] std::string serialize_project_canonical(const ProjectDocument& project);
[[nodiscard]] ProjectParseResult parse_project(
    std::string_view text,
    const core::OperatorRegistry& registry);
[[nodiscard]] SourceReferenceStatus assess_source_reference(
    std::string_view expected_identity,
    const std::optional<std::string>& actual_identity) noexcept;

}  // namespace faultmine::app
