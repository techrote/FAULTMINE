#include "faultmine/session.hpp"

#define set_source set_source_fm012
#define load_project_state load_project_state_fm012
#define make_project_document make_project_document_fm012
#include "session_fm011.cpp"
#undef make_project_document
#undef load_project_state
#undef set_source

#include <utility>

namespace faultmine::app {

bool SessionModel::set_source(
    core::ImageBuffer image,
    std::string source_identity,
    std::filesystem::path source_path,
    std::string* error) {
    const bool accepted = set_source_fm012(
        std::move(image), std::move(source_identity), std::move(source_path), error);
    if (accepted) laboratory_provenance_.reset();
    return accepted;
}

bool SessionModel::set_materialized_laboratory_source(
    laboratory::MaterializedSource source,
    std::filesystem::path source_path,
    std::string* error) {
    if (const auto validation = laboratory::validate_materialized_source(source); validation.has_value()) {
        if (error != nullptr) *error = *validation;
        return false;
    }
    const std::string identity = core::source_identity_hex(source.image);
    laboratory::LaboratoryProvenance provenance = source.provenance;
    if (!set_source_fm012(std::move(source.image), identity, std::move(source_path), error)) return false;
    laboratory_provenance_ = std::move(provenance);
    return true;
}

bool SessionModel::load_project_state(
    const ProjectDocument& project,
    core::ImageBuffer source,
    std::filesystem::path resolved_source_path,
    std::string* error) {
    if (project.source.laboratory.has_value()) {
        if (const auto validation = laboratory::validate_materialized_source(*project.source.laboratory); validation.has_value()) {
            if (error != nullptr) *error = "project laboratory source is invalid: " + *validation;
            return false;
        }
        if (core::source_identity_hex(project.source.laboratory->image) != project.source.source_identity ||
            core::source_identity_hex(source) != project.source.source_identity) {
            if (error != nullptr) *error = "resolved source does not match the project's frozen laboratory pixels";
            return false;
        }
    }
    if (!load_project_state_fm012(project, std::move(source), std::move(resolved_source_path), error)) return false;
    laboratory_provenance_ = project.source.laboratory.has_value()
        ? std::optional<laboratory::LaboratoryProvenance>{project.source.laboratory->provenance}
        : std::nullopt;
    return true;
}

std::optional<ProjectDocument> SessionModel::make_project_document(std::string* error) const {
    auto project = make_project_document_fm012(error);
    if (!project.has_value()) return std::nullopt;
    if (laboratory_provenance_.has_value() && source_.has_value()) {
        project->source.laboratory = laboratory::MaterializedSource{*source_, *laboratory_provenance_};
    }
    return project;
}

const std::optional<laboratory::LaboratoryProvenance>& SessionModel::laboratory_provenance() const noexcept {
    return laboratory_provenance_;
}

}  // namespace faultmine::app
