#pragma once

#include "faultmine/lineage.hpp"
#include "faultmine/session.hpp"
#include "faultmine/specimen_tray.hpp"
#include "faultmine/temporal.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::app {

inline constexpr std::uint32_t kExportManifestSchemaVersion = 1U;
inline constexpr std::string_view kApplicationVersion = "0.12.0";

enum class ExportKind {
    still,
    contact_sheet,
    frame_sequence,
};

[[nodiscard]] std::string_view export_kind_name(ExportKind kind) noexcept;

enum class ExportOverwritePolicy {
    fail_if_exists,
    replace_existing,
};

struct ExportOperatorSummary {
    std::string instance_id;
    std::string type_id;
    std::uint32_t type_version{};
    bool enabled{};

    bool operator==(const ExportOperatorSummary&) const = default;
};

struct ExportDerivationSummary {
    std::string kind;
    std::vector<std::string> parent_genome_identities;
    std::uint32_t policy_version{};
    std::optional<std::string> seed;
    std::optional<std::uint64_t> descendant_index;
    std::optional<std::string> mutation_radius;

    bool operator==(const ExportDerivationSummary&) const = default;
};

struct ExportFrameRecord {
    std::uint64_t frame_index{};
    std::string filename_utf8;
    std::string image_identity;
    std::string temporal_identity;

    bool operator==(const ExportFrameRecord&) const = default;
};

struct ExportContactCell {
    std::size_t cell_index{};
    std::size_t tray_index{};
    std::uint64_t descendant_index{};
    std::string mutation_seed;
    std::string genome_identity;

    bool operator==(const ExportContactCell&) const = default;
};

struct ExportManifest {
    std::uint32_t manifest_schema_version{kExportManifestSchemaVersion};
    std::string application_version{std::string{kApplicationVersion}};
    std::uint32_t engine_contract_version{};
    std::uint32_t genome_schema_version{};
    std::string source_identity;
    std::string source_path_utf8;
    std::string genome_identity;
    std::string canonical_genome_json;
    std::string root_seed;
    std::vector<ExportOperatorSummary> operators;
    ExportKind kind{ExportKind::still};
    bool canonical_pixels{true};
    bool proxy_pixels{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::string pixel_format{"rgba8-unorm"};
    std::string file_format{"png"};
    std::uint64_t frame_start_inclusive{};
    std::uint64_t frame_end_exclusive{1U};
    std::uint64_t timeline_rate_numerator{30U};
    std::uint64_t timeline_rate_denominator{1U};
    bool complete{true};
    bool cancelled{};
    std::optional<ExportDerivationSummary> derivation;
    std::vector<ExportFrameRecord> frames;
    std::vector<ExportContactCell> contact_cells;

    bool operator==(const ExportManifest&) const = default;
};

struct ExportManifestError {
    std::string path;
    std::string message;
};

struct ExportManifestParseResult {
    std::optional<ExportManifest> manifest;
    std::optional<ExportManifestError> error;

    [[nodiscard]] bool ok() const noexcept {
        return manifest.has_value() && !error.has_value();
    }
};

[[nodiscard]] std::string serialize_export_manifest(const ExportManifest& manifest);
[[nodiscard]] ExportManifestParseResult parse_export_manifest(
    std::string_view text,
    const core::OperatorRegistry& registry);

struct ExportOptions {
    ExportOverwritePolicy overwrite_policy{ExportOverwritePolicy::fail_if_exists};
    bool write_manifest{true};
};

struct ContactSheetOptions {
    std::size_t columns{4U};
    std::uint32_t cell_width{256U};
    std::uint32_t cell_height{192U};
    std::uint32_t label_height{16U};
};

struct SequenceOptions {
    std::uint64_t frame_start_inclusive{};
    std::uint64_t frame_end_exclusive{};
    std::uint32_t minimum_padding{6U};
};

struct ExportProgress {
    std::uint64_t completed{};
    std::uint64_t total{};
    std::uint64_t current_frame{};
};

using ExportContinueCallback = std::function<bool(const ExportProgress&)>;

struct ExportResult {
    bool success{};
    bool cancelled{};
    std::filesystem::path primary_path;
    std::filesystem::path manifest_path;
    std::vector<std::filesystem::path> completed_files;
    std::optional<ExportManifest> manifest;
    std::string error;
};

[[nodiscard]] std::filesystem::path companion_manifest_path(const std::filesystem::path& primary_path);

[[nodiscard]] ExportResult export_canonical_still(
    const SessionModel& session,
    const std::filesystem::path& destination_png,
    const ExportOptions& options = {});

[[nodiscard]] ExportResult export_contact_sheet(
    const SessionModel& session,
    const std::vector<SpecimenTrayItem>& tray_items,
    const std::filesystem::path& destination_png,
    const ContactSheetOptions& sheet_options = {},
    const ExportOptions& options = {});

[[nodiscard]] ExportResult export_frame_sequence(
    const SessionModel& session,
    const std::filesystem::path& sequence_base_png,
    const SequenceOptions& sequence_options,
    const ExportOptions& options = {},
    ExportContinueCallback continue_callback = {});

}  // namespace faultmine::app
