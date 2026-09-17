#pragma once

#include "faultmine/genome.hpp"
#include "faultmine/laboratory.hpp"
#include "faultmine/lineage.hpp"
#include "faultmine/session.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace faultmine::exporting {

inline constexpr std::uint32_t kExportManifestSchemaVersion = 1U;
inline constexpr std::string_view kApplicationVersion = "0.13.0";

enum class ExportKind {
    still,
    contact_sheet,
    frame_sequence,
};

enum class CollisionPolicy {
    fail_if_exists,
    overwrite,
};

struct OperatorManifest {
    std::string instance_id;
    std::string type_id;
    std::uint32_t type_version{};
    bool enabled{};

    bool operator==(const OperatorManifest&) const = default;
};

struct DerivationManifest {
    std::string kind;
    std::vector<std::string> parent_genome_identities;
    std::uint32_t policy_version{};
    std::optional<std::string> seed;
    std::optional<std::uint64_t> descendant_index;
    std::optional<std::string> mutation_radius;

    bool operator==(const DerivationManifest&) const = default;
};

struct ContactCellManifest {
    std::uint64_t cell_index{};
    std::uint64_t order_key{};
    std::string genome_identity;
    std::string canonical_genome;
    std::string image_identity;

    bool operator==(const ContactCellManifest&) const = default;
};

struct FrameManifest {
    std::uint64_t frame_index{};
    std::string filename;
    std::string image_identity;
    std::string frame_identity;

    bool operator==(const FrameManifest&) const = default;
};

struct ExportManifest {
    std::uint32_t manifest_version{kExportManifestSchemaVersion};
    ExportKind kind{ExportKind::still};
    std::string application_version{kApplicationVersion};
    std::uint32_t engine_contract_version{core::kEngineContractVersion};
    std::uint32_t genome_schema_version{core::kGenomeSchemaVersion};
    std::string source_identity;
    std::string source_path;
    std::string genome_identity;
    std::string canonical_genome;
    std::string root_seed;
    std::vector<OperatorManifest> operators;
    bool canonical_full_resolution{true};
    std::uint32_t output_width{};
    std::uint32_t output_height{};
    std::string output_format{"png-rgba8"};
    std::string output_image_identity;
    std::optional<DerivationManifest> derivation;
    // Optional FM-013 extension. It records the external-decoder derivation of
    // the normalized source while leaving downstream canonical output semantics
    // bound to source_identity and the embedded canonical genome.
    std::optional<laboratory::LaboratoryProvenance> laboratory;

    std::optional<std::uint64_t> still_frame;

    std::vector<ContactCellManifest> contact_cells;
    std::uint32_t contact_columns{};
    std::uint32_t contact_cell_width{};
    std::uint32_t contact_cell_height{};

    std::optional<std::uint64_t> frame_begin;
    std::optional<std::uint64_t> frame_end_exclusive;
    std::optional<std::uint64_t> rate_numerator;
    std::optional<std::uint64_t> rate_denominator;
    std::uint32_t filename_padding{};
    bool sequence_complete{};
    bool sequence_cancelled{};
    std::vector<FrameManifest> frames;

    bool operator==(const ExportManifest&) const = default;
};

struct ManifestParseResult {
    std::optional<ExportManifest> manifest;
    std::string error;

    [[nodiscard]] bool ok() const noexcept {
        return manifest.has_value() && error.empty();
    }
};

[[nodiscard]] std::string serialize_export_manifest(const ExportManifest& manifest);
[[nodiscard]] ManifestParseResult parse_export_manifest(
    std::string_view text,
    const core::OperatorRegistry& registry);

struct ExportError {
    std::string operation;
    std::filesystem::path path;
    std::string message;
};

struct ExportResult {
    std::vector<std::filesystem::path> completed_files;
    std::optional<std::filesystem::path> manifest_path;
    std::optional<ExportManifest> manifest;
    std::optional<ExportError> error;
    bool cancelled{};

    [[nodiscard]] bool ok() const noexcept {
        return !error.has_value();
    }
};

using CancelCallback = std::function<bool()>;
using ProgressCallback = std::function<void(
    std::size_t completed,
    std::size_t total,
    const std::filesystem::path& current_path)>;

struct StillExportRequest {
    std::filesystem::path destination;
    CollisionPolicy collision{CollisionPolicy::fail_if_exists};
    bool write_manifest{true};
    std::optional<std::uint64_t> frame_index;
};

struct ContactSheetSpecimen {
    std::uint64_t order_key{};
    core::Genome genome;
    std::string genome_identity;

    bool operator==(const ContactSheetSpecimen&) const = default;
};

struct ContactSheetRequest {
    std::filesystem::path destination;
    std::vector<ContactSheetSpecimen> specimens;
    std::uint32_t columns{4U};
    std::uint32_t cell_width{320U};
    std::uint32_t cell_height{240U};
    std::uint64_t frame_index{};
    CollisionPolicy collision{CollisionPolicy::fail_if_exists};
    bool write_manifest{true};
};

struct FrameSequenceRequest {
    std::filesystem::path directory;
    std::string stem{"frame"};
    std::uint64_t frame_begin{};
    std::uint64_t frame_end_exclusive{};
    std::uint32_t minimum_padding{6U};
    CollisionPolicy collision{CollisionPolicy::fail_if_exists};
    bool write_manifest{true};
};

[[nodiscard]] std::string sequence_frame_filename(
    std::string_view stem,
    std::uint64_t frame_index,
    std::uint64_t frame_end_exclusive,
    std::uint32_t minimum_padding = 6U);

[[nodiscard]] ExportResult export_still(
    const app::SessionModel& session,
    const StillExportRequest& request);

[[nodiscard]] ExportResult export_contact_sheet(
    const app::SessionModel& session,
    const ContactSheetRequest& request,
    CancelCallback should_cancel = {},
    ProgressCallback progress = {});

[[nodiscard]] ExportResult export_frame_sequence(
    const app::SessionModel& session,
    const FrameSequenceRequest& request,
    CancelCallback should_cancel = {},
    ProgressCallback progress = {});

}  // namespace faultmine::exporting
