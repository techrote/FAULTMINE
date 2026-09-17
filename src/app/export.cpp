#include "faultmine/export.hpp"

#include "faultmine/proxy.hpp"
#include "faultmine/temporal.hpp"
#include "faultmine/wic_io.hpp"
#include "../core/json.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace faultmine::app {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string encoded = path.generic_u8string();
    return std::string{reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

[[nodiscard]] std::string win32_error_text(const DWORD code) {
    return "Win32 error " + std::to_string(static_cast<std::uint64_t>(code));
}

[[nodiscard]] bool exists_noexcept(const std::filesystem::path& path) noexcept {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

[[nodiscard]] std::optional<std::filesystem::path> temporary_peer_path(
    const std::filesystem::path& target,
    std::string* error) {
    for (std::uint32_t ordinal = 0U; ordinal < 1000U; ++ordinal) {
        std::filesystem::path candidate = target;
        candidate += L".faultmine-part-" + std::to_wstring(ordinal);
        if (!exists_noexcept(candidate)) return candidate;
    }
    if (error != nullptr) *error = "could not allocate a temporary peer file beside " + path_to_utf8(target);
    return std::nullopt;
}

[[nodiscard]] bool commit_temporary_file(
    const std::filesystem::path& temporary,
    const std::filesystem::path& target,
    const ExportOverwritePolicy policy,
    std::string* error) {
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (policy == ExportOverwritePolicy::replace_existing) flags |= MOVEFILE_REPLACE_EXISTING;
    if (MoveFileExW(temporary.c_str(), target.c_str(), flags) != FALSE) return true;
    const DWORD code = GetLastError();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    if (error != nullptr) {
        *error = "could not commit export file " + path_to_utf8(target) + ": " + win32_error_text(code);
    }
    return false;
}

[[nodiscard]] bool write_text_atomic(
    const std::filesystem::path& target,
    const std::string_view text,
    const ExportOverwritePolicy policy,
    std::string* error) {
    if (policy == ExportOverwritePolicy::fail_if_exists && exists_noexcept(target)) {
        if (error != nullptr) *error = "export collision: " + path_to_utf8(target) + " already exists";
        return false;
    }
    const auto temporary = temporary_peer_path(target, error);
    if (!temporary.has_value()) return false;
    {
        std::ofstream stream(*temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            if (error != nullptr) *error = "could not open temporary manifest beside " + path_to_utf8(target);
            return false;
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(*temporary, ignored);
            if (error != nullptr) *error = "manifest write failed before all bytes were stored for " + path_to_utf8(target);
            return false;
        }
    }
    return commit_temporary_file(*temporary, target, policy, error);
}

[[nodiscard]] bool write_png_atomic(
    const core::ImageBuffer& image,
    const std::filesystem::path& target,
    const ExportOverwritePolicy policy,
    std::string* error) {
    if (policy == ExportOverwritePolicy::fail_if_exists && exists_noexcept(target)) {
        if (error != nullptr) *error = "export collision: " + path_to_utf8(target) + " already exists";
        return false;
    }
    const auto temporary = temporary_peer_path(target, error);
    if (!temporary.has_value()) return false;
    if (auto save_error = io::save_wic_png(image, *temporary); save_error.has_value()) {
        std::error_code ignored;
        std::filesystem::remove(*temporary, ignored);
        if (error != nullptr) {
            *error = "PNG encode failed for " + path_to_utf8(target) + ": " + save_error->message;
        }
        return false;
    }
    return commit_temporary_file(*temporary, target, policy, error);
}

[[nodiscard]] std::string quote(const std::string_view text) {
    return '"' + core::json::escape_string(text) + '"';
}

void append_string_array(std::ostringstream& out, const std::vector<std::string>& values) {
    out << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) out << ',';
        out << quote(values[index]);
    }
    out << ']';
}

[[nodiscard]] ExportDerivationSummary summarize_derivation(const SpecimenRecord& record) {
    ExportDerivationSummary summary;
    summary.kind = std::string{derivation_kind_name(record.derivation.kind)};
    summary.parent_genome_identities = record.derivation.parent_genome_identities;
    summary.policy_version = record.derivation.policy_version;
    if (record.derivation.seed.has_value()) summary.seed = record.derivation.seed->to_string();
    summary.descendant_index = record.derivation.descendant_index;
    if (record.derivation.mutation_radius.has_value()) {
        summary.mutation_radius = std::string{core::mutation_radius_name(*record.derivation.mutation_radius)};
    }
    return summary;
}

[[nodiscard]] ExportManifest base_manifest(const SessionModel& session, const ExportKind kind) {
    ExportManifest manifest;
    manifest.engine_contract_version = session.genome().engine_contract_version;
    manifest.genome_schema_version = session.genome().schema_version;
    manifest.source_identity = session.source_identity();
    manifest.source_path_utf8 = path_to_utf8(session.source_path());
    manifest.genome_identity = session.genome_identity();
    manifest.canonical_genome_json = core::serialize_canonical_genome(session.genome());
    manifest.root_seed = session.genome().root_seed.to_string();
    manifest.kind = kind;
    const TimelineRate rate = session.semantic_timeline_rate();
    manifest.timeline_rate_numerator = rate.numerator;
    manifest.timeline_rate_denominator = rate.denominator;
    manifest.operators.reserve(session.genome().operators.size());
    for (const core::OperatorInstance& instance : session.genome().operators) {
        manifest.operators.push_back(ExportOperatorSummary{
            instance.instance_id.to_string(), instance.type_id, instance.type_version, instance.enabled});
    }
    if (const SpecimenRecord* record = session.lineage().find(manifest.genome_identity); record != nullptr) {
        manifest.derivation = summarize_derivation(*record);
    }
    return manifest;
}

[[nodiscard]] const Value* object_field(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (const auto& entry : object.object) {
        if (entry.first == name) return &entry.second;
    }
    return nullptr;
}

[[nodiscard]] bool object_has_only(
    const Value& object,
    const std::initializer_list<std::string_view> allowed,
    std::string* unexpected) {
    if (object.type != ValueType::object) return false;
    for (const auto& entry : object.object) {
        const bool known = std::find(allowed.begin(), allowed.end(), entry.first) != allowed.end();
        if (!known) {
            if (unexpected != nullptr) *unexpected = entry.first;
            return false;
        }
    }
    return true;
}

template <typename Integer>
[[nodiscard]] bool parse_integer(const Value* value, Integer& output) {
    if (value == nullptr || value->type != ValueType::number) return false;
    Integer parsed{};
    const char* first = value->text.data();
    const char* last = first + value->text.size();
    const auto result = std::from_chars(first, last, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != last) return false;
    output = parsed;
    return true;
}

[[nodiscard]] bool parse_string(const Value* value, std::string& output) {
    if (value == nullptr || value->type != ValueType::string) return false;
    output = value->text;
    return true;
}

[[nodiscard]] bool parse_bool(const Value* value, bool& output) {
    if (value == nullptr || value->type != ValueType::boolean) return false;
    output = value->boolean;
    return true;
}

[[nodiscard]] ExportManifestParseResult manifest_error(std::string path, std::string message) {
    return ExportManifestParseResult{std::nullopt, ExportManifestError{std::move(path), std::move(message)}};
}

[[nodiscard]] bool is_lower_hex(const std::string_view text, const std::size_t count) noexcept {
    if (text.size() != count) return false;
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

[[nodiscard]] std::optional<ExportKind> parse_export_kind(const std::string_view value) noexcept {
    if (value == "still") return ExportKind::still;
    if (value == "contact-sheet") return ExportKind::contact_sheet;
    if (value == "frame-sequence") return ExportKind::frame_sequence;
    return std::nullopt;
}

[[nodiscard]] std::optional<core::MutationRadius> parse_radius(const std::string_view value) noexcept {
    if (value == "low") return core::MutationRadius::low;
    if (value == "medium") return core::MutationRadius::medium;
    if (value == "high") return core::MutationRadius::high;
    return std::nullopt;
}

[[nodiscard]] std::uint32_t decimal_digits(std::uint64_t value) noexcept {
    std::uint32_t digits = 1U;
    while (value >= 10U) {
        value /= 10U;
        ++digits;
    }
    return digits;
}

[[nodiscard]] std::filesystem::path sequence_frame_path(
    const std::filesystem::path& base,
    const std::uint64_t frame,
    const std::uint32_t padding) {
    std::wostringstream suffix;
    suffix << L"-f" << std::setw(static_cast<int>(padding)) << std::setfill(L'0') << frame << L".png";
    return base.parent_path() / (base.stem().wstring() + suffix.str());
}

[[nodiscard]] bool preflight_path(
    const std::filesystem::path& path,
    const ExportOverwritePolicy policy,
    std::string* error) {
    if (path.empty()) {
        if (error != nullptr) *error = "export destination is empty";
        return false;
    }
    if (policy == ExportOverwritePolicy::fail_if_exists && exists_noexcept(path)) {
        if (error != nullptr) *error = "export collision: " + path_to_utf8(path) + " already exists";
        return false;
    }
    return true;
}

using Glyph = std::array<std::uint8_t, 5>;

[[nodiscard]] Glyph glyph_for(const char c) noexcept {
    switch (c) {
        case '0': return {7U, 5U, 5U, 5U, 7U};
        case '1': return {2U, 6U, 2U, 2U, 7U};
        case '2': return {7U, 1U, 7U, 4U, 7U};
        case '3': return {7U, 1U, 7U, 1U, 7U};
        case '4': return {5U, 5U, 7U, 1U, 1U};
        case '5': return {7U, 4U, 7U, 1U, 7U};
        case '6': return {7U, 4U, 7U, 5U, 7U};
        case '7': return {7U, 1U, 1U, 1U, 1U};
        case '8': return {7U, 5U, 7U, 5U, 7U};
        case '9': return {7U, 5U, 7U, 1U, 7U};
        case 'a': return {2U, 5U, 7U, 5U, 5U};
        case 'b': return {6U, 5U, 6U, 5U, 6U};
        case 'c': return {3U, 4U, 4U, 4U, 3U};
        case 'd': return {6U, 5U, 5U, 5U, 6U};
        case 'e': return {7U, 4U, 6U, 4U, 7U};
        case 'f': return {7U, 4U, 6U, 4U, 4U};
        case '#': return {5U, 7U, 5U, 7U, 5U};
        case '-': return {0U, 0U, 7U, 0U, 0U};
        default: return {0U, 0U, 0U, 0U, 0U};
    }
}

void draw_label(
    core::ImageBuffer& image,
    const std::uint32_t origin_x,
    const std::uint32_t origin_y,
    const std::string_view label,
    const std::uint32_t max_x) {
    constexpr std::uint32_t scale = 2U;
    constexpr std::uint32_t advance = 8U;
    std::uint32_t cursor = origin_x;
    for (const char c : label) {
        if (cursor + 6U > max_x) break;
        const Glyph glyph = glyph_for(c);
        for (std::uint32_t row = 0U; row < 5U; ++row) {
            for (std::uint32_t column = 0U; column < 3U; ++column) {
                if ((glyph[row] & (1U << (2U - column))) == 0U) continue;
                for (std::uint32_t sy = 0U; sy < scale; ++sy) {
                    for (std::uint32_t sx = 0U; sx < scale; ++sx) {
                        const std::uint32_t x = cursor + column * scale + sx;
                        const std::uint32_t y = origin_y + row * scale + sy;
                        if (x >= image.width || y >= image.height) continue;
                        const std::size_t offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
                        image.bytes[offset + 0U] = 255U;
                        image.bytes[offset + 1U] = 255U;
                        image.bytes[offset + 2U] = 255U;
                        image.bytes[offset + 3U] = 255U;
                    }
                }
            }
        }
        cursor += advance;
    }
}

[[nodiscard]] std::optional<core::ImageBuffer> make_contact_sheet_image(
    const SessionModel& session,
    const std::vector<SpecimenTrayItem>& items,
    const ContactSheetOptions& options,
    std::vector<ExportContactCell>& cells,
    std::string* error) {
    if (items.empty()) {
        if (error != nullptr) *error = "contact sheet requires at least one specimen";
        return std::nullopt;
    }
    if (options.columns == 0U || options.columns > 32U || options.cell_width < 32U ||
        options.cell_height < 32U || options.label_height >= options.cell_height ||
        options.cell_width > 4096U || options.cell_height > 4096U) {
        if (error != nullptr) *error = "contact-sheet dimensions are outside the bounded FM-012 contract";
        return std::nullopt;
    }
    const std::size_t rows = (items.size() + options.columns - 1U) / options.columns;
    const std::uint64_t width64 = static_cast<std::uint64_t>(options.columns) * options.cell_width;
    const std::uint64_t height64 = static_cast<std::uint64_t>(rows) * options.cell_height;
    if (width64 > std::numeric_limits<std::uint32_t>::max() || height64 > std::numeric_limits<std::uint32_t>::max()) {
        if (error != nullptr) *error = "contact-sheet dimensions overflow canonical image limits";
        return std::nullopt;
    }
    auto created = core::make_rgba8_image(static_cast<std::uint32_t>(width64), static_cast<std::uint32_t>(height64));
    if (!created.ok()) {
        if (error != nullptr) *error = created.error->message;
        return std::nullopt;
    }
    core::ImageBuffer sheet = std::move(*created.image);
    for (std::size_t offset = 0U; offset < sheet.bytes.size(); offset += 4U) {
        sheet.bytes[offset + 0U] = 18U;
        sheet.bytes[offset + 1U] = 18U;
        sheet.bytes[offset + 2U] = 18U;
        sheet.bytes[offset + 3U] = 255U;
    }

    const core::ImageBuffer* source = session.full_source();
    if (source == nullptr) {
        if (error != nullptr) *error = "contact sheet requires a loaded full-resolution source";
        return std::nullopt;
    }
    const std::uint32_t image_height = options.cell_height - options.label_height;
    cells.clear();
    cells.reserve(items.size());
    for (std::size_t index = 0U; index < items.size(); ++index) {
        const SpecimenTrayItem& item = items[index];
        const core::RenderResult rendered = core::render_pipeline_at_frame(
            *source, item.genome, session.registry(), session.current_frame());
        if (!rendered.ok()) {
            if (error != nullptr) {
                *error = "contact-sheet specimen " + std::to_string(index) + " canonical render failed: " +
                    (rendered.error.has_value() ? rendered.error->message : std::string{"unknown render error"});
            }
            return std::nullopt;
        }
        const std::string rendered_identity = core::source_identity_hex(*rendered.image);
        core::ProxySpec proxy_spec{options.cell_width, image_height, core::kProxyMethodVersion};
        const core::ProxyResult proxy = core::make_nearest_proxy(*rendered.image, rendered_identity, proxy_spec);
        if (!proxy.ok()) {
            if (error != nullptr) *error = "contact-sheet nearest-neighbour reduction failed: " + proxy.error->message;
            return std::nullopt;
        }
        const core::ImageBuffer& thumbnail = proxy.proxy->image;
        const std::uint32_t cell_x = static_cast<std::uint32_t>(index % options.columns) * options.cell_width;
        const std::uint32_t cell_y = static_cast<std::uint32_t>(index / options.columns) * options.cell_height;
        const std::uint32_t copy_x = cell_x + (options.cell_width - thumbnail.width) / 2U;
        const std::uint32_t copy_y = cell_y + (image_height - thumbnail.height) / 2U;
        for (std::uint32_t y = 0U; y < thumbnail.height; ++y) {
            for (std::uint32_t x = 0U; x < thumbnail.width; ++x) {
                const std::size_t source_offset = (static_cast<std::size_t>(y) * thumbnail.width + x) * 4U;
                const std::size_t target_offset =
                    (static_cast<std::size_t>(copy_y + y) * sheet.width + copy_x + x) * 4U;
                std::copy_n(thumbnail.bytes.data() + source_offset, 4U, sheet.bytes.data() + target_offset);
            }
        }
        const std::string genome_identity = core::genome_identity_hex(item.genome);
        std::string label = "#" + std::to_string(index) + "-" + genome_identity.substr(0U, 8U);
        draw_label(sheet, cell_x + 4U, cell_y + image_height + 2U, label, cell_x + options.cell_width - 2U);
        cells.push_back(ExportContactCell{
            index,
            index,
            item.provenance.descendant_index,
            item.provenance.mutation_seed.to_string(),
            genome_identity});
    }
    return sheet;
}

}  // namespace

std::string_view export_kind_name(const ExportKind kind) noexcept {
    switch (kind) {
        case ExportKind::still: return "still";
        case ExportKind::contact_sheet: return "contact-sheet";
        case ExportKind::frame_sequence: return "frame-sequence";
    }
    return "unknown";
}

std::filesystem::path companion_manifest_path(const std::filesystem::path& primary_path) {
    return primary_path.parent_path() / (primary_path.stem().wstring() + L".fmmanifest.json");
}

std::string serialize_export_manifest(const ExportManifest& manifest) {
    std::ostringstream out;
    out << "{\"manifest_schema_version\":" << manifest.manifest_schema_version;
    out << ",\"application_version\":" << quote(manifest.application_version);
    out << ",\"engine_contract_version\":" << manifest.engine_contract_version;
    out << ",\"genome_schema_version\":" << manifest.genome_schema_version;
    out << ",\"source\":{\"identity\":" << quote(manifest.source_identity)
        << ",\"path\":" << quote(manifest.source_path_utf8) << '}';
    out << ",\"genome\":{\"identity\":" << quote(manifest.genome_identity)
        << ",\"root_seed\":" << quote(manifest.root_seed)
        << ",\"canonical_json\":" << quote(manifest.canonical_genome_json) << '}';
    out << ",\"operators\":[";
    for (std::size_t index = 0U; index < manifest.operators.size(); ++index) {
        if (index != 0U) out << ',';
        const ExportOperatorSummary& op = manifest.operators[index];
        out << "{\"instance_id\":" << quote(op.instance_id)
            << ",\"type_id\":" << quote(op.type_id)
            << ",\"type_version\":" << op.type_version
            << ",\"enabled\":" << (op.enabled ? "true" : "false") << '}';
    }
    out << ']';
    out << ",\"output\":{\"kind\":" << quote(export_kind_name(manifest.kind))
        << ",\"canonical_pixels\":" << (manifest.canonical_pixels ? "true" : "false")
        << ",\"proxy_pixels\":" << (manifest.proxy_pixels ? "true" : "false")
        << ",\"width\":" << manifest.width
        << ",\"height\":" << manifest.height
        << ",\"pixel_format\":" << quote(manifest.pixel_format)
        << ",\"file_format\":" << quote(manifest.file_format)
        << ",\"complete\":" << (manifest.complete ? "true" : "false")
        << ",\"cancelled\":" << (manifest.cancelled ? "true" : "false") << '}';
    out << ",\"timeline\":{\"frame_start_inclusive\":" << manifest.frame_start_inclusive
        << ",\"frame_end_exclusive\":" << manifest.frame_end_exclusive
        << ",\"rate_num\":" << manifest.timeline_rate_numerator
        << ",\"rate_den\":" << manifest.timeline_rate_denominator << '}';
    out << ",\"derivation\":";
    if (!manifest.derivation.has_value()) {
        out << "null";
    } else {
        const ExportDerivationSummary& derivation = *manifest.derivation;
        out << "{\"kind\":" << quote(derivation.kind) << ",\"parents\":";
        append_string_array(out, derivation.parent_genome_identities);
        out << ",\"policy_version\":" << derivation.policy_version;
        out << ",\"seed\":" << (derivation.seed.has_value() ? quote(*derivation.seed) : std::string{"null"});
        out << ",\"descendant_index\":";
        if (derivation.descendant_index.has_value()) out << *derivation.descendant_index; else out << "null";
        out << ",\"mutation_radius\":" << (derivation.mutation_radius.has_value() ? quote(*derivation.mutation_radius) : std::string{"null"});
        out << '}';
    }
    out << ",\"frames\":[";
    for (std::size_t index = 0U; index < manifest.frames.size(); ++index) {
        if (index != 0U) out << ',';
        const ExportFrameRecord& frame = manifest.frames[index];
        out << "{\"frame_index\":" << frame.frame_index
            << ",\"filename\":" << quote(frame.filename_utf8)
            << ",\"image_identity\":" << quote(frame.image_identity)
            << ",\"temporal_identity\":" << quote(frame.temporal_identity) << '}';
    }
    out << ']';
    out << ",\"contact_cells\":[";
    for (std::size_t index = 0U; index < manifest.contact_cells.size(); ++index) {
        if (index != 0U) out << ',';
        const ExportContactCell& cell = manifest.contact_cells[index];
        out << "{\"cell_index\":" << cell.cell_index
            << ",\"tray_index\":" << cell.tray_index
            << ",\"descendant_index\":" << cell.descendant_index
            << ",\"mutation_seed\":" << quote(cell.mutation_seed)
            << ",\"genome_identity\":" << quote(cell.genome_identity) << '}';
    }
    out << "]}\n";
    return out.str();
}

ExportManifestParseResult parse_export_manifest(
    const std::string_view text,
    const core::OperatorRegistry& registry) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value()) {
        return manifest_error("$", parsed.error.has_value() ? parsed.error->message : "invalid JSON");
    }
    const Value& root = *parsed.value;
    std::string unexpected;
    if (!object_has_only(root, {"manifest_schema_version", "application_version", "engine_contract_version", "genome_schema_version", "source", "genome", "operators", "output", "timeline", "derivation", "frames", "contact_cells"}, &unexpected)) {
        return manifest_error("$", root.type == ValueType::object ? "unexpected field: " + unexpected : "manifest root must be an object");
    }

    ExportManifest manifest;
    if (!parse_integer(object_field(root, "manifest_schema_version"), manifest.manifest_schema_version) ||
        manifest.manifest_schema_version != kExportManifestSchemaVersion) {
        return manifest_error("$.manifest_schema_version", "unsupported export manifest schema version");
    }
    if (!parse_string(object_field(root, "application_version"), manifest.application_version) || manifest.application_version.empty()) {
        return manifest_error("$.application_version", "application version is required");
    }
    if (!parse_integer(object_field(root, "engine_contract_version"), manifest.engine_contract_version) ||
        !parse_integer(object_field(root, "genome_schema_version"), manifest.genome_schema_version)) {
        return manifest_error("$", "engine/genome schema versions must be unsigned integers");
    }

    const Value* source = object_field(root, "source");
    if (source == nullptr || !object_has_only(*source, {"identity", "path"}, &unexpected) ||
        !parse_string(object_field(*source, "identity"), manifest.source_identity) ||
        !parse_string(object_field(*source, "path"), manifest.source_path_utf8) ||
        !is_lower_hex(manifest.source_identity, 64U)) {
        return manifest_error("$.source", "source must contain a canonical 64-hex identity and path");
    }

    const Value* genome = object_field(root, "genome");
    if (genome == nullptr || !object_has_only(*genome, {"identity", "root_seed", "canonical_json"}, &unexpected) ||
        !parse_string(object_field(*genome, "identity"), manifest.genome_identity) ||
        !parse_string(object_field(*genome, "root_seed"), manifest.root_seed) ||
        !parse_string(object_field(*genome, "canonical_json"), manifest.canonical_genome_json) ||
        !is_lower_hex(manifest.genome_identity, 64U) || !core::RootSeed::parse(manifest.root_seed).has_value()) {
        return manifest_error("$.genome", "genome identity/root seed/canonical JSON are invalid");
    }
    const core::GenomeParseResult canonical_genome = core::parse_genome(manifest.canonical_genome_json, registry);
    if (!canonical_genome.ok()) return manifest_error("$.genome.canonical_json", "embedded canonical genome does not validate");
    if (core::genome_identity_hex(*canonical_genome.genome) != manifest.genome_identity ||
        canonical_genome.genome->root_seed.to_string() != manifest.root_seed ||
        canonical_genome.genome->engine_contract_version != manifest.engine_contract_version ||
        canonical_genome.genome->schema_version != manifest.genome_schema_version) {
        return manifest_error("$.genome", "embedded canonical genome does not match manifest identity/version fields");
    }

    const Value* operators = object_field(root, "operators");
    if (operators == nullptr || operators->type != ValueType::array) return manifest_error("$.operators", "operators must be an array");
    for (const Value& value : operators->array) {
        ExportOperatorSummary op;
        if (!object_has_only(value, {"instance_id", "type_id", "type_version", "enabled"}, &unexpected) ||
            !parse_string(object_field(value, "instance_id"), op.instance_id) ||
            !parse_string(object_field(value, "type_id"), op.type_id) ||
            !parse_integer(object_field(value, "type_version"), op.type_version) ||
            !parse_bool(object_field(value, "enabled"), op.enabled) ||
            !core::InstanceId::parse(op.instance_id).has_value()) {
            return manifest_error("$.operators[]", "operator summary is malformed");
        }
        manifest.operators.push_back(std::move(op));
    }
    if (manifest.operators.size() != canonical_genome.genome->operators.size()) {
        return manifest_error("$.operators", "operator summary count does not match embedded genome");
    }
    for (std::size_t index = 0U; index < manifest.operators.size(); ++index) {
        const auto& summary = manifest.operators[index];
        const auto& instance = canonical_genome.genome->operators[index];
        if (summary.instance_id != instance.instance_id.to_string() || summary.type_id != instance.type_id ||
            summary.type_version != instance.type_version || summary.enabled != instance.enabled) {
            return manifest_error("$.operators", "operator summary does not match embedded genome order/state");
        }
    }

    const Value* output = object_field(root, "output");
    std::string kind_text;
    if (output == nullptr || !object_has_only(*output, {"kind", "canonical_pixels", "proxy_pixels", "width", "height", "pixel_format", "file_format", "complete", "cancelled"}, &unexpected) ||
        !parse_string(object_field(*output, "kind"), kind_text) ||
        !parse_bool(object_field(*output, "canonical_pixels"), manifest.canonical_pixels) ||
        !parse_bool(object_field(*output, "proxy_pixels"), manifest.proxy_pixels) ||
        !parse_integer(object_field(*output, "width"), manifest.width) ||
        !parse_integer(object_field(*output, "height"), manifest.height) ||
        !parse_string(object_field(*output, "pixel_format"), manifest.pixel_format) ||
        !parse_string(object_field(*output, "file_format"), manifest.file_format) ||
        !parse_bool(object_field(*output, "complete"), manifest.complete) ||
        !parse_bool(object_field(*output, "cancelled"), manifest.cancelled)) {
        return manifest_error("$.output", "output metadata is malformed");
    }
    const auto kind = parse_export_kind(kind_text);
    if (!kind.has_value() || manifest.width == 0U || manifest.height == 0U ||
        manifest.pixel_format != "rgba8-unorm" || manifest.file_format != "png" || manifest.proxy_pixels) {
        return manifest_error("$.output", "unsupported output kind/format/dimensions/proxy flag");
    }
    manifest.kind = *kind;
    if ((manifest.kind == ExportKind::contact_sheet) == manifest.canonical_pixels) {
        return manifest_error("$.output.canonical_pixels", "contact sheets are presentation artefacts; stills/sequences are canonical");
    }

    const Value* timeline = object_field(root, "timeline");
    if (timeline == nullptr || !object_has_only(*timeline, {"frame_start_inclusive", "frame_end_exclusive", "rate_num", "rate_den"}, &unexpected) ||
        !parse_integer(object_field(*timeline, "frame_start_inclusive"), manifest.frame_start_inclusive) ||
        !parse_integer(object_field(*timeline, "frame_end_exclusive"), manifest.frame_end_exclusive) ||
        !parse_integer(object_field(*timeline, "rate_num"), manifest.timeline_rate_numerator) ||
        !parse_integer(object_field(*timeline, "rate_den"), manifest.timeline_rate_denominator) ||
        manifest.frame_end_exclusive <= manifest.frame_start_inclusive ||
        manifest.timeline_rate_numerator == 0U || manifest.timeline_rate_denominator == 0U) {
        return manifest_error("$.timeline", "timeline range/rate is invalid");
    }

    const Value* derivation = object_field(root, "derivation");
    if (derivation == nullptr) return manifest_error("$.derivation", "derivation field is required (null when unavailable)");
    if (derivation->type != ValueType::null_value) {
        ExportDerivationSummary summary;
        if (!object_has_only(*derivation, {"kind", "parents", "policy_version", "seed", "descendant_index", "mutation_radius"}, &unexpected) ||
            !parse_string(object_field(*derivation, "kind"), summary.kind) ||
            !parse_integer(object_field(*derivation, "policy_version"), summary.policy_version)) {
            return manifest_error("$.derivation", "derivation summary is malformed");
        }
        const Value* parents = object_field(*derivation, "parents");
        if (parents == nullptr || parents->type != ValueType::array) return manifest_error("$.derivation.parents", "parents must be an array");
        for (const Value& parent : parents->array) {
            if (parent.type != ValueType::string || !is_lower_hex(parent.text, 64U)) return manifest_error("$.derivation.parents[]", "parent identity is invalid");
            summary.parent_genome_identities.push_back(parent.text);
        }
        const Value* seed = object_field(*derivation, "seed");
        if (seed != nullptr && seed->type == ValueType::string) {
            if (!core::RootSeed::parse(seed->text).has_value()) return manifest_error("$.derivation.seed", "seed is invalid");
            summary.seed = seed->text;
        } else if (seed == nullptr || seed->type != ValueType::null_value) {
            return manifest_error("$.derivation.seed", "seed must be root-seed text or null");
        }
        const Value* descendant = object_field(*derivation, "descendant_index");
        if (descendant != nullptr && descendant->type == ValueType::number) {
            std::uint64_t value{};
            if (!parse_integer(descendant, value)) return manifest_error("$.derivation.descendant_index", "descendant index is invalid");
            summary.descendant_index = value;
        } else if (descendant == nullptr || descendant->type != ValueType::null_value) {
            return manifest_error("$.derivation.descendant_index", "descendant index must be unsigned or null");
        }
        const Value* radius = object_field(*derivation, "mutation_radius");
        if (radius != nullptr && radius->type == ValueType::string) {
            if (!parse_radius(radius->text).has_value()) return manifest_error("$.derivation.mutation_radius", "mutation radius is invalid");
            summary.mutation_radius = radius->text;
        } else if (radius == nullptr || radius->type != ValueType::null_value) {
            return manifest_error("$.derivation.mutation_radius", "mutation radius must be text or null");
        }
        manifest.derivation = std::move(summary);
    }

    const Value* frames = object_field(root, "frames");
    if (frames == nullptr || frames->type != ValueType::array) return manifest_error("$.frames", "frames must be an array");
    for (const Value& value : frames->array) {
        ExportFrameRecord frame;
        if (!object_has_only(value, {"frame_index", "filename", "image_identity", "temporal_identity"}, &unexpected) ||
            !parse_integer(object_field(value, "frame_index"), frame.frame_index) ||
            !parse_string(object_field(value, "filename"), frame.filename_utf8) ||
            !parse_string(object_field(value, "image_identity"), frame.image_identity) ||
            !parse_string(object_field(value, "temporal_identity"), frame.temporal_identity) ||
            !is_lower_hex(frame.image_identity, 64U) || !is_lower_hex(frame.temporal_identity, 64U)) {
            return manifest_error("$.frames[]", "frame audit record is malformed");
        }
        manifest.frames.push_back(std::move(frame));
    }

    const Value* cells = object_field(root, "contact_cells");
    if (cells == nullptr || cells->type != ValueType::array) return manifest_error("$.contact_cells", "contact_cells must be an array");
    for (const Value& value : cells->array) {
        ExportContactCell cell;
        if (!object_has_only(value, {"cell_index", "tray_index", "descendant_index", "mutation_seed", "genome_identity"}, &unexpected) ||
            !parse_integer(object_field(value, "cell_index"), cell.cell_index) ||
            !parse_integer(object_field(value, "tray_index"), cell.tray_index) ||
            !parse_integer(object_field(value, "descendant_index"), cell.descendant_index) ||
            !parse_string(object_field(value, "mutation_seed"), cell.mutation_seed) ||
            !parse_string(object_field(value, "genome_identity"), cell.genome_identity) ||
            !core::RootSeed::parse(cell.mutation_seed).has_value() || !is_lower_hex(cell.genome_identity, 64U)) {
            return manifest_error("$.contact_cells[]", "contact-sheet mapping record is malformed");
        }
        manifest.contact_cells.push_back(std::move(cell));
    }
    return ExportManifestParseResult{std::move(manifest), std::nullopt};
}

ExportResult export_canonical_still(
    const SessionModel& session,
    const std::filesystem::path& destination_png,
    const ExportOptions& options) {
    ExportResult result;
    result.primary_path = destination_png;
    result.manifest_path = companion_manifest_path(destination_png);
    if (!session.has_source()) {
        result.error = "load a source before exporting";
        return result;
    }
    if (!preflight_path(destination_png, options.overwrite_policy, &result.error) ||
        (options.write_manifest && !preflight_path(result.manifest_path, options.overwrite_policy, &result.error))) {
        return result;
    }
    std::string render_error;
    const auto image = session.render_full(&render_error);
    if (!image.has_value()) {
        result.error = "canonical full-resolution render failed: " + render_error;
        return result;
    }
    ExportManifest manifest = base_manifest(session, ExportKind::still);
    manifest.width = image->width;
    manifest.height = image->height;
    manifest.frame_start_inclusive = session.current_frame();
    manifest.frame_end_exclusive = session.current_frame() == std::numeric_limits<std::uint64_t>::max()
        ? session.current_frame() : session.current_frame() + 1U;
    const std::string image_identity = core::source_identity_hex(*image);
    manifest.frames.push_back(ExportFrameRecord{
        session.current_frame(), path_to_utf8(destination_png.filename()), image_identity,
        core::temporal_frame_identity_hex(session.source_identity(), session.genome(), session.current_frame(), *image)});
    if (!write_png_atomic(*image, destination_png, options.overwrite_policy, &result.error)) return result;
    result.completed_files.push_back(destination_png);
    if (options.write_manifest) {
        if (!write_text_atomic(result.manifest_path, serialize_export_manifest(manifest), options.overwrite_policy, &result.error)) return result;
        result.completed_files.push_back(result.manifest_path);
    }
    result.manifest = std::move(manifest);
    result.success = true;
    return result;
}

ExportResult export_contact_sheet(
    const SessionModel& session,
    const std::vector<SpecimenTrayItem>& tray_items,
    const std::filesystem::path& destination_png,
    const ContactSheetOptions& sheet_options,
    const ExportOptions& options) {
    ExportResult result;
    result.primary_path = destination_png;
    result.manifest_path = companion_manifest_path(destination_png);
    if (!session.has_source()) {
        result.error = "load a source before exporting a contact sheet";
        return result;
    }
    if (!preflight_path(destination_png, options.overwrite_policy, &result.error) ||
        (options.write_manifest && !preflight_path(result.manifest_path, options.overwrite_policy, &result.error))) {
        return result;
    }
    std::vector<ExportContactCell> cells;
    auto sheet = make_contact_sheet_image(session, tray_items, sheet_options, cells, &result.error);
    if (!sheet.has_value()) return result;
    ExportManifest manifest = base_manifest(session, ExportKind::contact_sheet);
    manifest.canonical_pixels = false;
    manifest.width = sheet->width;
    manifest.height = sheet->height;
    manifest.frame_start_inclusive = session.current_frame();
    manifest.frame_end_exclusive = session.current_frame() == std::numeric_limits<std::uint64_t>::max()
        ? session.current_frame() : session.current_frame() + 1U;
    manifest.contact_cells = std::move(cells);
    if (!write_png_atomic(*sheet, destination_png, options.overwrite_policy, &result.error)) return result;
    result.completed_files.push_back(destination_png);
    if (options.write_manifest) {
        if (!write_text_atomic(result.manifest_path, serialize_export_manifest(manifest), options.overwrite_policy, &result.error)) return result;
        result.completed_files.push_back(result.manifest_path);
    }
    result.manifest = std::move(manifest);
    result.success = true;
    return result;
}

ExportResult export_frame_sequence(
    const SessionModel& session,
    const std::filesystem::path& sequence_base_png,
    const SequenceOptions& sequence_options,
    const ExportOptions& options,
    ExportContinueCallback continue_callback) {
    ExportResult result;
    result.primary_path = sequence_base_png;
    result.manifest_path = companion_manifest_path(sequence_base_png);
    if (!session.has_source()) {
        result.error = "load a source before exporting a frame sequence";
        return result;
    }
    if (sequence_options.frame_end_exclusive <= sequence_options.frame_start_inclusive) {
        result.error = "frame sequence requires a non-empty [start,end) range";
        return result;
    }
    const std::uint64_t frame_count = sequence_options.frame_end_exclusive - sequence_options.frame_start_inclusive;
    if (frame_count > 100000U) {
        result.error = "frame sequence exceeds the FM-012 bounded export limit of 100000 frames";
        return result;
    }
    const std::uint32_t padding = std::max(
        sequence_options.minimum_padding,
        decimal_digits(sequence_options.frame_end_exclusive - 1U));
    if (padding > 20U) {
        result.error = "frame filename padding exceeds the supported deterministic naming bound";
        return result;
    }
    if (options.write_manifest && !preflight_path(result.manifest_path, options.overwrite_policy, &result.error)) return result;
    for (std::uint64_t frame = sequence_options.frame_start_inclusive; frame < sequence_options.frame_end_exclusive; ++frame) {
        if (!preflight_path(sequence_frame_path(sequence_base_png, frame, padding), options.overwrite_policy, &result.error)) return result;
    }

    ExportManifest manifest = base_manifest(session, ExportKind::frame_sequence);
    manifest.frame_start_inclusive = sequence_options.frame_start_inclusive;
    manifest.frame_end_exclusive = sequence_options.frame_end_exclusive;
    const core::ImageBuffer* source = session.full_source();
    if (source == nullptr) {
        result.error = "full-resolution source disappeared before sequence export";
        return result;
    }
    manifest.width = source->width;
    manifest.height = source->height;

    std::uint64_t completed = 0U;
    for (std::uint64_t frame = sequence_options.frame_start_inclusive; frame < sequence_options.frame_end_exclusive; ++frame) {
        if (continue_callback && !continue_callback(ExportProgress{completed, frame_count, frame})) {
            result.cancelled = true;
            manifest.complete = false;
            manifest.cancelled = true;
            break;
        }
        std::string render_error;
        const auto image = session.render_full_at_frame(frame, &render_error);
        if (!image.has_value()) {
            result.error = "canonical frame " + std::to_string(frame) + " render failed: " + render_error;
            manifest.complete = false;
            break;
        }
        const std::filesystem::path path = sequence_frame_path(sequence_base_png, frame, padding);
        if (!write_png_atomic(*image, path, options.overwrite_policy, &result.error)) {
            manifest.complete = false;
            break;
        }
        result.completed_files.push_back(path);
        ++completed;
        manifest.frames.push_back(ExportFrameRecord{
            frame,
            path_to_utf8(path.filename()),
            core::source_identity_hex(*image),
            core::temporal_frame_identity_hex(session.source_identity(), session.genome(), frame, *image)});
    }

    if (options.write_manifest) {
        std::string manifest_error_text;
        if (!write_text_atomic(result.manifest_path, serialize_export_manifest(manifest), options.overwrite_policy, &manifest_error_text)) {
            if (result.error.empty()) result.error = manifest_error_text;
        } else {
            result.completed_files.push_back(result.manifest_path);
        }
    }
    result.manifest = manifest;
    result.success = manifest.complete && !result.cancelled && result.error.empty();
    if (result.cancelled && result.error.empty()) result.error = "frame sequence export cancelled after completed frames were safely committed";
    return result;
}

}  // namespace faultmine::app
