#include "faultmine/export.hpp"

#include "../core/json.hpp"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace faultmine::exporting {
inline constexpr std::uint32_t kExportManifestSchemaVersionFm012 = 1U;
}

#define kExportManifestSchemaVersion kExportManifestSchemaVersionFm012
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
#undef kExportManifestSchemaVersion

namespace faultmine::exporting {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] std::string json_string_fm013(const std::string_view text) {
    return "\"" + core::json::escape_string(text) + "\"";
}

[[nodiscard]] std::string serialize_value_fm013(const Value& value) {
    switch (value.type) {
        case ValueType::null_value: return "null";
        case ValueType::boolean: return value.boolean ? "true" : "false";
        case ValueType::number: return value.text;
        case ValueType::string: return json_string_fm013(value.text);
        case ValueType::array: {
            std::string output{"["};
            for (std::size_t index = 0U; index < value.array.size(); ++index) {
                if (index != 0U) output += ',';
                output += serialize_value_fm013(value.array[index]);
            }
            output += ']';
            return output;
        }
        case ValueType::object: {
            std::string output{"{"};
            for (std::size_t index = 0U; index < value.object.size(); ++index) {
                if (index != 0U) output += ',';
                output += json_string_fm013(value.object[index].first) + ':' + serialize_value_fm013(value.object[index].second);
            }
            output += '}';
            return output;
        }
    }
    return "null";
}

[[nodiscard]] const Value* field_fm013(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (const auto& item : object.object) if (item.first == name) return &item.second;
    return nullptr;
}

[[nodiscard]] Value* mutable_field_fm013(Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (auto& item : object.object) if (item.first == name) return &item.second;
    return nullptr;
}

[[nodiscard]] std::optional<std::uint32_t> manifest_version_fm013(const Value& root) noexcept {
    const Value* version = field_fm013(root, "manifest_version");
    if (version == nullptr || version->type != ValueType::number || version->text.empty()) return std::nullopt;
    std::uint64_t parsed{};
    const char* begin = version->text.data();
    const char* end = begin + version->text.size();
    const auto conversion = std::from_chars(begin, end, parsed, 10);
    if (conversion.ec != std::errc{} || conversion.ptr != end ||
        parsed > std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
    return static_cast<std::uint32_t>(parsed);
}

[[nodiscard]] bool atomic_write_manifest_fm013(
    const std::filesystem::path& destination,
    const std::string_view text,
    std::string& error) {
    std::filesystem::path temporary = destination;
    temporary += L".fm013.tmp";
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            error = "could not open FM-013 manifest temporary file";
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) {
            error = "could not write complete FM-013 manifest temporary file";
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    if (MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE) {
        error = "could not atomically replace export manifest with FM-013 provenance: " + std::to_string(GetLastError());
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

void attach_laboratory_provenance_fm013(ExportResult& result, const app::SessionModel& session) {
    if (!result.manifest.has_value()) return;
    result.manifest->manifest_version = kExportManifestSchemaVersion;
    result.manifest->application_version = std::string{kApplicationVersion};
    result.manifest->laboratory_provenance = session.laboratory_provenance();
    if (!result.manifest_path.has_value()) return;
    std::string error;
    const std::string text = serialize_export_manifest(*result.manifest);
    if (text.empty() || !atomic_write_manifest_fm013(*result.manifest_path, text, error)) {
        result.error = ExportError{"write FM-013 laboratory provenance manifest", *result.manifest_path, std::move(error)};
    }
}

}  // namespace

std::string serialize_export_manifest(const ExportManifest& manifest) {
    ExportManifest legacy = manifest;
    legacy.manifest_version = kLegacyExportManifestSchemaVersion;
    legacy.laboratory_provenance.reset();
    std::string text = serialize_export_manifest_fm012(legacy);
    if (text.size() < 2U || text[text.size() - 2U] != '}' || text.back() != '\n') return {};
    text.resize(text.size() - 2U);
    text += ",\"laboratory_provenance\":";
    if (manifest.laboratory_provenance.has_value()) {
        std::string validation_error;
        if (!core::validate_laboratory_provenance(*manifest.laboratory_provenance, &validation_error)) return {};
        std::string provenance = core::serialize_laboratory_provenance_json(*manifest.laboratory_provenance);
        if (!provenance.empty() && provenance.back() == '\n') provenance.pop_back();
        text += provenance;
    } else {
        text += "null";
    }
    text += "}\n";
    const std::string legacy_prefix = "{\"manifest_version\":" + std::to_string(kLegacyExportManifestSchemaVersion);
    const std::string current_prefix = "{\"manifest_version\":" + std::to_string(kExportManifestSchemaVersion);
    if (!text.starts_with(legacy_prefix)) return {};
    text.replace(0U, legacy_prefix.size(), current_prefix);
    return text;
}

ManifestParseResult parse_export_manifest(const std::string_view text, const core::OperatorRegistry& registry) {
    const auto parsed = core::json::parse(text);
    if (!parsed.value.has_value() || parsed.value->type != ValueType::object) return parse_export_manifest_fm012(text, registry);
    const auto version = manifest_version_fm013(*parsed.value);
    if (!version.has_value()) return ManifestParseResult{std::nullopt, "manifest_version is missing or invalid"};
    if (*version == kLegacyExportManifestSchemaVersion) return parse_export_manifest_fm012(text, registry);
    if (*version != kExportManifestSchemaVersion) return ManifestParseResult{std::nullopt, "unsupported export manifest version"};

    Value root = *parsed.value;
    auto laboratory = std::find_if(root.object.begin(), root.object.end(), [](const auto& item) {
        return item.first == "laboratory_provenance";
    });
    if (laboratory == root.object.end()) return ManifestParseResult{std::nullopt, "manifest v2 requires laboratory_provenance (null for ordinary sources)"};
    Value laboratory_value = laboratory->second;
    root.object.erase(laboratory);
    Value* mutable_version = mutable_field_fm013(root, "manifest_version");
    if (mutable_version == nullptr) return ManifestParseResult{std::nullopt, "manifest version normalization failed"};
    mutable_version->text = std::to_string(kLegacyExportManifestSchemaVersion);
    std::string legacy_text = serialize_value_fm013(root);
    legacy_text.push_back('\n');
    ManifestParseResult result = parse_export_manifest_fm012(legacy_text, registry);
    if (!result.ok()) return result;
    result.manifest->manifest_version = kExportManifestSchemaVersion;
    result.manifest->laboratory_provenance.reset();
    if (laboratory_value.type == ValueType::null_value) return result;
    if (laboratory_value.type != ValueType::object) return ManifestParseResult{std::nullopt, "laboratory_provenance must be an object or null"};
    std::string provenance_text = serialize_value_fm013(laboratory_value);
    provenance_text.push_back('\n');
    std::string provenance_error;
    auto provenance = core::parse_laboratory_provenance_json(provenance_text, &provenance_error);
    if (!provenance.has_value()) return ManifestParseResult{std::nullopt, "invalid laboratory_provenance: " + provenance_error};
    if (provenance->materialized_source_identity != result.manifest->source_identity) {
        return ManifestParseResult{std::nullopt, "laboratory provenance source identity does not match manifest source"};
    }
    result.manifest->laboratory_provenance = std::move(*provenance);
    return result;
}

ExportResult export_still(const app::SessionModel& session, const StillExportRequest& request) {
    ExportResult result = export_still_fm012(session, request);
    if (result.ok()) attach_laboratory_provenance_fm013(result, session);
    return result;
}

ExportResult export_contact_sheet(
    const app::SessionModel& session,
    const ContactSheetRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result = export_contact_sheet_fm012(session, request, std::move(should_cancel), std::move(progress));
    if (result.ok()) attach_laboratory_provenance_fm013(result, session);
    return result;
}

ExportResult export_frame_sequence(
    const app::SessionModel& session,
    const FrameSequenceRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result = export_frame_sequence_fm012(session, request, std::move(should_cancel), std::move(progress));
    if (result.ok()) attach_laboratory_provenance_fm013(result, session);
    return result;
}

}  // namespace faultmine::exporting
