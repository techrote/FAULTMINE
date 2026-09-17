#include "faultmine/export.hpp"

#define serialize_export_manifest serialize_export_manifest_fm012
#define parse_export_manifest parse_export_manifest_fm012
#define export_still export_still_fm012
#define export_contact_sheet export_contact_sheet_fm012
#define export_frame_sequence export_frame_sequence_fm012
#include "export.cpp"
#undef export_frame_sequence
#undef export_contact_sheet
#undef export_still
#undef parse_export_manifest
#undef serialize_export_manifest

#include "../core/json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace faultmine::exporting {
namespace {

[[nodiscard]] core::json::Value* mutable_manifest_field(
    core::json::Value& value,
    const std::string_view name) noexcept {
    if (value.type != core::json::ValueType::object) return nullptr;
    for (auto& pair : value.object) {
        if (pair.first == name) return &pair.second;
    }
    return nullptr;
}

[[nodiscard]] std::string trim_newline(std::string value) {
    if (!value.empty() && value.back() == '\n') value.pop_back();
    return value;
}

[[nodiscard]] std::optional<ExportError> rewrite_augmented_manifest(
    const std::filesystem::path& path,
    const ExportManifest& manifest) {
    std::filesystem::path temporary = path;
    temporary += L".fm013.tmp";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return ExportError{"rewrite laboratory provenance manifest", path, "could not create temporary manifest"};
        const std::string text = serialize_export_manifest(manifest);
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            std::filesystem::remove(temporary, ignored);
            return ExportError{"rewrite laboratory provenance manifest", path, "could not commit temporary manifest"};
        }
    }
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
        const DWORD code = GetLastError();
        std::filesystem::remove(temporary, ignored);
        return ExportError{
            "rewrite laboratory provenance manifest",
            path,
            "atomic manifest replacement failed with Win32 error " + std::to_string(code)};
    }
    return std::nullopt;
}

void attach_laboratory_provenance(const app::SessionModel& session, ExportResult& result) {
    if (!result.ok() || !result.manifest.has_value() || !session.laboratory_provenance().has_value()) return;
    result.manifest->laboratory = *session.laboratory_provenance();
    if (result.manifest_path.has_value()) {
        if (auto error = rewrite_augmented_manifest(*result.manifest_path, *result.manifest); error.has_value()) {
            result.error = std::move(error);
        }
    }
}

}  // namespace

std::string serialize_export_manifest(const ExportManifest& manifest) {
    ExportManifest ordinary = manifest;
    ordinary.laboratory.reset();
    std::string output = serialize_export_manifest_fm012(ordinary);
    if (!manifest.laboratory.has_value()) return output;
    const std::string provenance = trim_newline(
        laboratory::serialize_laboratory_provenance(*manifest.laboratory));
    const std::size_t still_field = output.find(",\"still\":");
    if (still_field == std::string::npos) return {};
    output.insert(still_field, ",\"laboratory\":" + provenance);
    return output;
}

ManifestParseResult parse_export_manifest(
    const std::string_view text,
    const core::OperatorRegistry& registry) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != core::json::ValueType::object) {
        return parse_export_manifest_fm012(text, registry);
    }
    const core::json::Value* laboratory_value = field(*parsed.value, "laboratory");
    if (laboratory_value == nullptr) return parse_export_manifest_fm012(text, registry);

    const laboratory::LaboratoryParseResult provenance = laboratory::parse_laboratory_provenance(
        serialize_json_value(*laboratory_value));
    if (!provenance.ok()) return ManifestParseResult{std::nullopt, provenance.error};
    if (provenance.provenance->outcome != laboratory::Outcome::success ||
        !provenance.provenance->materialized_source_identity.has_value()) {
        return ManifestParseResult{std::nullopt, "export laboratory provenance must describe a successful materialized source"};
    }

    core::json::Value stripped = *parsed.value;
    stripped.object.erase(
        std::remove_if(
            stripped.object.begin(), stripped.object.end(),
            [](const auto& pair) { return pair.first == "laboratory"; }),
        stripped.object.end());
    std::string ordinary_text = serialize_json_value(stripped);
    ordinary_text.push_back('\n');
    ManifestParseResult result = parse_export_manifest_fm012(ordinary_text, registry);
    if (!result.ok()) return result;
    if (*provenance.provenance->materialized_source_identity != result.manifest->source_identity) {
        return ManifestParseResult{std::nullopt, "export laboratory materialized source identity does not match manifest source identity"};
    }
    result.manifest->laboratory = *provenance.provenance;
    return result;
}

ExportResult export_still(
    const app::SessionModel& session,
    const StillExportRequest& request) {
    ExportResult result = export_still_fm012(session, request);
    attach_laboratory_provenance(session, result);
    return result;
}

ExportResult export_contact_sheet(
    const app::SessionModel& session,
    const ContactSheetRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result = export_contact_sheet_fm012(
        session, request, std::move(should_cancel), std::move(progress));
    attach_laboratory_provenance(session, result);
    return result;
}

ExportResult export_frame_sequence(
    const app::SessionModel& session,
    const FrameSequenceRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result = export_frame_sequence_fm012(
        session, request, std::move(should_cancel), std::move(progress));
    attach_laboratory_provenance(session, result);
    return result;
}

}  // namespace faultmine::exporting
