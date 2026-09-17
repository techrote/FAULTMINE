#include "faultmine/session.hpp"

#define set_source set_source_fm012
#define load_project_state load_project_state_fm012
#define make_project_document make_project_document_fm012
#include "session_fm011.cpp"
#undef make_project_document
#undef load_project_state
#undef set_source

namespace faultmine::app {

bool SessionModel::set_source(
    core::ImageBuffer image,
    std::string source_identity,
    std::filesystem::path source_path,
    std::string* error) {
    if (!set_source_fm012(std::move(image), std::move(source_identity), std::move(source_path), error)) return false;
    laboratory_provenance_.reset();
    return true;
}

bool SessionModel::set_materialized_laboratory_source(
    core::MaterializedLaboratorySource source,
    std::filesystem::path original_encoded_path,
    std::string* error) {
    if (!core::validate_materialized_laboratory_source(source, error)) return false;
    const std::string identity = source.provenance.materialized_source_identity;
    core::LaboratoryProvenance provenance = source.provenance;
    if (!set_source_fm012(std::move(source.image), identity, std::move(original_encoded_path), error)) return false;
    laboratory_provenance_ = std::move(provenance);
    return true;
}

bool SessionModel::load_project_state(
    const ProjectDocument& project,
    core::ImageBuffer source,
    std::filesystem::path resolved_source_path,
    std::string* error) {
    if (project.laboratory_source.has_value()) {
        if (!core::validate_materialized_laboratory_source(*project.laboratory_source, error)) return false;
        if (source != project.laboratory_source->image) {
            if (error != nullptr) *error = "project supplied source does not match its embedded frozen laboratory pixels";
            return false;
        }
    }
    if (!load_project_state_fm012(project, std::move(source), std::move(resolved_source_path), error)) return false;
    laboratory_provenance_ = project.laboratory_source.has_value()
        ? std::optional<core::LaboratoryProvenance>{project.laboratory_source->provenance}
        : std::nullopt;
    return true;
}

std::optional<ProjectDocument> SessionModel::make_project_document(std::string* error) const {
    auto project = make_project_document_fm012(error);
    if (!project.has_value()) return std::nullopt;
    project->project_version = kProjectSchemaVersion;
    project->laboratory_source.reset();
    if (laboratory_provenance_.has_value()) {
        if (!source_.has_value()) {
            if (error != nullptr) *error = "laboratory provenance exists without canonical materialized source pixels";
            return std::nullopt;
        }
        core::MaterializedLaboratorySource materialized{*source_, *laboratory_provenance_};
        if (!core::validate_materialized_laboratory_source(materialized, error)) return std::nullopt;
        project->laboratory_source = std::move(materialized);
    }
    return project;
}

const std::optional<core::LaboratoryProvenance>& SessionModel::laboratory_provenance() const noexcept {
    return laboratory_provenance_;
}

}  // namespace faultmine::app
