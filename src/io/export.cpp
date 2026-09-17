#include "faultmine/export.hpp"

#include "../core/json.hpp"
#include "faultmine/image.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/proxy.hpp"
#include "faultmine/temporal.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
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
#include <vector>

namespace faultmine::exporting {
namespace {

using core::json::Value;
using core::json::ValueType;

[[nodiscard]] std::string export_kind_name(const ExportKind kind) {
    switch (kind) {
        case ExportKind::still: return "still";
        case ExportKind::contact_sheet: return "contact-sheet";
        case ExportKind::frame_sequence: return "frame-sequence";
    }
    return "still";
}

[[nodiscard]] std::optional<ExportKind> parse_export_kind(const std::string_view text) noexcept {
    if (text == "still") return ExportKind::still;
    if (text == "contact-sheet") return ExportKind::contact_sheet;
    if (text == "frame-sequence") return ExportKind::frame_sequence;
    return std::nullopt;
}

[[nodiscard]] std::string path_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return std::string{
        reinterpret_cast<const char*>(text.data()),
        reinterpret_cast<const char*>(text.data() + text.size())};
}

[[nodiscard]] std::string json_string(const std::string_view value) {
    return "\"" + core::json::escape_string(value) + "\"";
}

[[nodiscard]] std::string embedded_genome_json(const std::string_view canonical) {
    std::string text{canonical};
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

[[nodiscard]] std::string serialize_json_value(const Value& value) {
    switch (value.type) {
        case ValueType::null_value: return "null";
        case ValueType::boolean: return value.boolean ? "true" : "false";
        case ValueType::number: return value.text;
        case ValueType::string: return json_string(value.text);
        case ValueType::array: {
            std::string output{"["};
            for (std::size_t index = 0U; index < value.array.size(); ++index) {
                if (index != 0U) output += ',';
                output += serialize_json_value(value.array[index]);
            }
            output += ']';
            return output;
        }
        case ValueType::object: {
            std::string output{"{"};
            for (std::size_t index = 0U; index < value.object.size(); ++index) {
                if (index != 0U) output += ',';
                output += json_string(value.object[index].first);
                output += ':';
                output += serialize_json_value(value.object[index].second);
            }
            output += '}';
            return output;
        }
    }
    return "null";
}

[[nodiscard]] const Value* field(const Value& object, const std::string_view name) noexcept {
    if (object.type != ValueType::object) return nullptr;
    for (const auto& pair : object.object) {
        if (pair.first == name) return &pair.second;
    }
    return nullptr;
}

[[nodiscard]] bool only_fields(
    const Value& object,
    const std::initializer_list<std::string_view> allowed,
    std::string& unexpected) {
    if (object.type != ValueType::object) return false;
    for (const auto& pair : object.object) {
        if (std::find(allowed.begin(), allowed.end(), pair.first) == allowed.end()) {
            unexpected = pair.first;
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool parse_u64(const Value& value, std::uint64_t& output) noexcept {
    if (value.type != ValueType::number || value.text.empty() || value.text.front() == '-') return false;
    const char* first = value.text.data();
    const char* last = value.text.data() + value.text.size();
    const auto result = std::from_chars(first, last, output, 10);
    return result.ec == std::errc{} && result.ptr == last;
}

[[nodiscard]] bool parse_u32(const Value& value, std::uint32_t& output) noexcept {
    std::uint64_t parsed{};
    if (!parse_u64(value, parsed) || parsed > std::numeric_limits<std::uint32_t>::max()) return false;
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool valid_hex_identity(const std::string_view text) noexcept {
    return text.size() == 64U && std::all_of(text.begin(), text.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

[[nodiscard]] ManifestParseResult manifest_fail(std::string message) {
    return ManifestParseResult{std::nullopt, std::move(message)};
}

[[nodiscard]] std::optional<core::Genome> parse_embedded_genome(
    const Value& value,
    const core::OperatorRegistry& registry,
    std::string& error) {
    if (value.type != ValueType::object) {
        error = "canonical_genome must be an object";
        return std::nullopt;
    }
    std::string text = serialize_json_value(value);
    text.push_back('\n');
    const core::GenomeParseResult parsed = core::parse_genome(text, registry);
    if (!parsed.ok()) {
        error = "canonical_genome is invalid";
        if (parsed.error.has_value()) {
            error += ": " + parsed.error->path + ": " + parsed.error->message;
        }
        return std::nullopt;
    }
    return *parsed.genome;
}

[[nodiscard]] DerivationManifest manifest_derivation(const app::SpecimenDerivation& derivation) {
    DerivationManifest result;
    result.kind = std::string{app::derivation_kind_name(derivation.kind)};
    result.parent_genome_identities = derivation.parent_genome_identities;
    result.policy_version = derivation.policy_version;
    if (derivation.seed.has_value()) result.seed = derivation.seed->to_string();
    result.descendant_index = derivation.descendant_index;
    if (derivation.mutation_radius.has_value()) {
        result.mutation_radius = std::string{core::mutation_radius_name(*derivation.mutation_radius)};
    }
    return result;
}

[[nodiscard]] ExportManifest base_manifest(const app::SessionModel& session, const ExportKind kind) {
    ExportManifest manifest;
    manifest.kind = kind;
    manifest.source_identity = session.source_identity();
    manifest.source_path = path_utf8(session.source_path());
    manifest.genome_identity = session.genome_identity();
    manifest.canonical_genome = core::serialize_canonical_genome(session.genome());
    manifest.root_seed = session.genome().root_seed.to_string();
    manifest.operators.reserve(session.genome().operators.size());
    for (const core::OperatorInstance& instance : session.genome().operators) {
        manifest.operators.push_back(OperatorManifest{
            instance.instance_id.to_string(), instance.type_id, instance.type_version, instance.enabled});
    }
    const app::SpecimenRecord* record = session.lineage().find(manifest.genome_identity);
    if (record != nullptr && record->source_identity == manifest.source_identity) {
        manifest.derivation = manifest_derivation(record->derivation);
    }
    return manifest;
}

[[nodiscard]] std::filesystem::path manifest_sidecar_path(const std::filesystem::path& output) {
    std::filesystem::path result = output;
    result += L".fmmanifest.json";
    return result;
}

[[nodiscard]] std::optional<ExportError> check_parent_directory(const std::filesystem::path& destination) {
    const std::filesystem::path parent = destination.parent_path();
    if (parent.empty()) return std::nullopt;
    std::error_code error_code;
    const bool exists = std::filesystem::exists(parent, error_code);
    if (error_code) {
        return ExportError{"inspect destination directory", parent, error_code.message()};
    }
    if (!exists) {
        return ExportError{"inspect destination directory", parent, "destination directory does not exist"};
    }
    const bool directory = std::filesystem::is_directory(parent, error_code);
    if (error_code) {
        return ExportError{"inspect destination directory", parent, error_code.message()};
    }
    if (!directory) {
        return ExportError{"inspect destination directory", parent, "destination parent is not a directory"};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ExportError> preflight_collision(
    const std::filesystem::path& destination,
    const CollisionPolicy policy) {
    if (auto error = check_parent_directory(destination); error.has_value()) return error;
    if (policy == CollisionPolicy::overwrite) return std::nullopt;
    std::error_code error_code;
    const bool exists = std::filesystem::exists(destination, error_code);
    if (error_code) {
        return ExportError{"inspect destination", destination, error_code.message()};
    }
    if (exists) {
        return ExportError{"collision check", destination, "destination already exists and overwrite was not requested"};
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::filesystem::path> choose_temp_path(
    const std::filesystem::path& destination,
    std::string& error) {
    for (std::uint32_t index = 0U; index < 1024U; ++index) {
        std::filesystem::path candidate = destination;
        candidate += L".faultmine.tmp.";
        candidate += std::to_wstring(index);
        std::error_code error_code;
        const bool exists = std::filesystem::exists(candidate, error_code);
        if (error_code) {
            error = "could not inspect temporary export path: " + error_code.message();
            return std::nullopt;
        }
        if (!exists) return candidate;
    }
    error = "could not reserve a temporary export path after 1024 collision attempts";
    return std::nullopt;
}

[[nodiscard]] std::optional<ExportError> commit_temp_file(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination,
    const CollisionPolicy policy,
    const std::string_view operation) {
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (policy == CollisionPolicy::overwrite) flags |= MOVEFILE_REPLACE_EXISTING;
    if (MoveFileExW(temporary.c_str(), destination.c_str(), flags) != FALSE) return std::nullopt;

    const DWORD win32_error = GetLastError();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return ExportError{
        std::string{operation},
        destination,
        "could not atomically commit completed output (Win32 error " + std::to_string(win32_error) + ")"};
}

[[nodiscard]] std::optional<ExportError> atomic_write_text(
    const std::filesystem::path& destination,
    const std::string_view text,
    const CollisionPolicy policy) {
    if (auto error = preflight_collision(destination, policy); error.has_value()) return error;
    std::string temp_error;
    const auto temporary = choose_temp_path(destination, temp_error);
    if (!temporary.has_value()) return ExportError{"create manifest temporary file", destination, temp_error};

    {
        std::ofstream stream(*temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return ExportError{"open manifest temporary file", *temporary, "could not open temporary file for writing"};
        }
        stream.write(text.data(), static_cast<std::streamsize>(text.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(*temporary, ignored);
            return ExportError{"write manifest", *temporary, "manifest write failed before all bytes were stored"};
        }
    }
    return commit_temp_file(*temporary, destination, policy, "commit manifest");
}

[[nodiscard]] std::optional<ExportError> atomic_write_png(
    const core::ImageBuffer& image,
    const std::filesystem::path& destination,
    const CollisionPolicy policy) {
    if (auto error = preflight_collision(destination, policy); error.has_value()) return error;
    std::string temp_error;
    const auto temporary = choose_temp_path(destination, temp_error);
    if (!temporary.has_value()) return ExportError{"create PNG temporary file", destination, temp_error};

    if (auto error = io::save_wic_png(image, *temporary); error.has_value()) {
        std::error_code ignored;
        std::filesystem::remove(*temporary, ignored);
        return ExportError{
            "encode PNG",
            destination,
            error->operation + ": " + error->message};
    }
    return commit_temp_file(*temporary, destination, policy, "commit PNG");
}

[[nodiscard]] std::string render_error_text(const core::RenderResult& rendered) {
    if (!rendered.error.has_value()) return "canonical pipeline render failed";
    std::string message = rendered.error->message;
    if (!rendered.error->operator_type.empty()) {
        message = rendered.error->operator_type + " at stack index " +
            std::to_string(rendered.error->operator_index) + ": " + message;
    }
    return message;
}

[[nodiscard]] bool copy_proxy_into_cell(
    core::ImageBuffer& sheet,
    const core::ImageBuffer& thumbnail,
    const std::uint32_t cell_x,
    const std::uint32_t cell_y,
    const std::uint32_t cell_width,
    const std::uint32_t cell_height) {
    if (thumbnail.width > cell_width || thumbnail.height > cell_height) return false;
    const std::uint32_t x_offset = cell_x + (cell_width - thumbnail.width) / 2U;
    const std::uint32_t y_offset = cell_y + (cell_height - thumbnail.height) / 2U;
    for (std::uint32_t y = 0U; y < thumbnail.height; ++y) {
        const std::size_t source = static_cast<std::size_t>(y) * static_cast<std::size_t>(thumbnail.row_stride);
        const std::size_t destination =
            static_cast<std::size_t>(y_offset + y) * static_cast<std::size_t>(sheet.row_stride) +
            static_cast<std::size_t>(x_offset) * 4U;
        const std::size_t bytes = static_cast<std::size_t>(thumbnail.width) * 4U;
        std::copy_n(thumbnail.bytes.data() + source, bytes, sheet.bytes.data() + destination);
    }
    return true;
}

[[nodiscard]] std::optional<DerivationManifest> parse_derivation_manifest(
    const Value& value,
    std::string& error) {
    if (value.type == ValueType::null_value) return std::nullopt;
    if (value.type != ValueType::object) {
        error = "derivation must be null or an object";
        return std::nullopt;
    }
    std::string unexpected;
    if (!only_fields(value, {"kind", "parents", "policy_version", "seed", "descendant_index", "mutation_radius"}, unexpected)) {
        error = "unexpected derivation field: " + unexpected;
        return std::nullopt;
    }
    const Value* kind = field(value, "kind");
    const Value* parents = field(value, "parents");
    const Value* policy = field(value, "policy_version");
    const Value* seed = field(value, "seed");
    const Value* descendant = field(value, "descendant_index");
    const Value* radius = field(value, "mutation_radius");
    if (kind == nullptr || parents == nullptr || policy == nullptr || seed == nullptr || descendant == nullptr || radius == nullptr ||
        kind->type != ValueType::string || parents->type != ValueType::array) {
        error = "derivation is missing required fields or field types are invalid";
        return std::nullopt;
    }
    if (!app::parse_derivation_kind(kind->text).has_value()) {
        error = "derivation kind is not supported";
        return std::nullopt;
    }
    DerivationManifest output;
    output.kind = kind->text;
    if (!parse_u32(*policy, output.policy_version)) {
        error = "derivation policy_version must be an unsigned 32-bit integer";
        return std::nullopt;
    }
    for (const Value& parent : parents->array) {
        if (parent.type != ValueType::string || !valid_hex_identity(parent.text)) {
            error = "derivation parent identity must be a lower-case SHA-256 string";
            return std::nullopt;
        }
        output.parent_genome_identities.push_back(parent.text);
    }
    if (seed->type == ValueType::string) {
        if (!core::RootSeed::parse(seed->text).has_value()) {
            error = "derivation seed is invalid";
            return std::nullopt;
        }
        output.seed = seed->text;
    } else if (seed->type != ValueType::null_value) {
        error = "derivation seed must be null or a root-seed string";
        return std::nullopt;
    }
    if (descendant->type == ValueType::number) {
        std::uint64_t parsed{};
        if (!parse_u64(*descendant, parsed)) {
            error = "derivation descendant_index is invalid";
            return std::nullopt;
        }
        output.descendant_index = parsed;
    } else if (descendant->type != ValueType::null_value) {
        error = "derivation descendant_index must be null or an unsigned integer";
        return std::nullopt;
    }
    if (radius->type == ValueType::string) {
        if (radius->text != "low" && radius->text != "medium" && radius->text != "high") {
            error = "derivation mutation_radius is invalid";
            return std::nullopt;
        }
        output.mutation_radius = radius->text;
    } else if (radius->type != ValueType::null_value) {
        error = "derivation mutation_radius must be null or a radius string";
        return std::nullopt;
    }
    return output;
}

[[nodiscard]] bool parse_operator_manifest(
    const Value& value,
    OperatorManifest& output,
    std::string& error) {
    if (value.type != ValueType::object) {
        error = "operator summary must be an object";
        return false;
    }
    std::string unexpected;
    if (!only_fields(value, {"instance_id", "type_id", "type_version", "enabled"}, unexpected)) {
        error = "unexpected operator summary field: " + unexpected;
        return false;
    }
    const Value* instance = field(value, "instance_id");
    const Value* type = field(value, "type_id");
    const Value* version = field(value, "type_version");
    const Value* enabled = field(value, "enabled");
    if (instance == nullptr || type == nullptr || version == nullptr || enabled == nullptr ||
        instance->type != ValueType::string || type->type != ValueType::string || enabled->type != ValueType::boolean ||
        !core::InstanceId::parse(instance->text).has_value() || !parse_u32(*version, output.type_version)) {
        error = "operator summary fields are invalid";
        return false;
    }
    output.instance_id = instance->text;
    output.type_id = type->text;
    output.enabled = enabled->boolean;
    return true;
}

[[nodiscard]] bool parse_contact_cell(
    const Value& value,
    const core::OperatorRegistry& registry,
    ContactCellManifest& output,
    std::string& error) {
    if (value.type != ValueType::object) {
        error = "contact cell must be an object";
        return false;
    }
    std::string unexpected;
    if (!only_fields(value, {"cell_index", "order_key", "genome_identity", "canonical_genome", "image_identity"}, unexpected)) {
        error = "unexpected contact cell field: " + unexpected;
        return false;
    }
    const Value* cell = field(value, "cell_index");
    const Value* order = field(value, "order_key");
    const Value* identity = field(value, "genome_identity");
    const Value* genome_value = field(value, "canonical_genome");
    const Value* image = field(value, "image_identity");
    if (cell == nullptr || order == nullptr || identity == nullptr || genome_value == nullptr || image == nullptr ||
        identity->type != ValueType::string || image->type != ValueType::string ||
        !parse_u64(*cell, output.cell_index) || !parse_u64(*order, output.order_key) ||
        !valid_hex_identity(identity->text) || !valid_hex_identity(image->text)) {
        error = "contact cell fields are invalid";
        return false;
    }
    const auto genome = parse_embedded_genome(*genome_value, registry, error);
    if (!genome.has_value()) return false;
    if (core::genome_identity_hex(*genome) != identity->text) {
        error = "contact cell genome identity does not match its canonical genome";
        return false;
    }
    output.genome_identity = identity->text;
    output.canonical_genome = core::serialize_canonical_genome(*genome);
    output.image_identity = image->text;
    return true;
}

[[nodiscard]] bool parse_frame_manifest(const Value& value, FrameManifest& output, std::string& error) {
    if (value.type != ValueType::object) {
        error = "frame manifest must be an object";
        return false;
    }
    std::string unexpected;
    if (!only_fields(value, {"frame_index", "filename", "image_identity", "frame_identity"}, unexpected)) {
        error = "unexpected frame manifest field: " + unexpected;
        return false;
    }
    const Value* frame = field(value, "frame_index");
    const Value* filename = field(value, "filename");
    const Value* image = field(value, "image_identity");
    const Value* frame_identity = field(value, "frame_identity");
    if (frame == nullptr || filename == nullptr || image == nullptr || frame_identity == nullptr ||
        filename->type != ValueType::string || image->type != ValueType::string || frame_identity->type != ValueType::string ||
        !parse_u64(*frame, output.frame_index) || filename->text.empty() ||
        !valid_hex_identity(image->text) || !valid_hex_identity(frame_identity->text)) {
        error = "frame manifest fields are invalid";
        return false;
    }
    output.filename = filename->text;
    output.image_identity = image->text;
    output.frame_identity = frame_identity->text;
    return true;
}

}  // namespace

std::string serialize_export_manifest(const ExportManifest& manifest) {
    std::string output{"{\"manifest_version\":"};
    output += std::to_string(manifest.manifest_version);
    output += ",\"kind\":" + json_string(export_kind_name(manifest.kind));
    output += ",\"application_version\":" + json_string(manifest.application_version);
    output += ",\"engine_contract_version\":" + std::to_string(manifest.engine_contract_version);
    output += ",\"genome_schema_version\":" + std::to_string(manifest.genome_schema_version);
    output += ",\"source\":{\"identity\":" + json_string(manifest.source_identity);
    output += ",\"path\":" + json_string(manifest.source_path) + '}';
    output += ",\"genome\":{\"identity\":" + json_string(manifest.genome_identity);
    output += ",\"canonical\":" + embedded_genome_json(manifest.canonical_genome);
    output += ",\"root_seed\":" + json_string(manifest.root_seed);
    output += ",\"operators\":[";
    for (std::size_t index = 0U; index < manifest.operators.size(); ++index) {
        if (index != 0U) output += ',';
        const OperatorManifest& op = manifest.operators[index];
        output += "{\"instance_id\":" + json_string(op.instance_id);
        output += ",\"type_id\":" + json_string(op.type_id);
        output += ",\"type_version\":" + std::to_string(op.type_version);
        output += ",\"enabled\":";
        output += op.enabled ? "true" : "false";
        output += '}';
    }
    output += "]}";
    output += ",\"output\":{\"canonical_full_resolution\":";
    output += manifest.canonical_full_resolution ? "true" : "false";
    output += ",\"width\":" + std::to_string(manifest.output_width);
    output += ",\"height\":" + std::to_string(manifest.output_height);
    output += ",\"format\":" + json_string(manifest.output_format);
    output += ",\"image_identity\":" + json_string(manifest.output_image_identity) + '}';

    output += ",\"derivation\":";
    if (!manifest.derivation.has_value()) {
        output += "null";
    } else {
        const DerivationManifest& derivation = *manifest.derivation;
        output += "{\"kind\":" + json_string(derivation.kind) + ",\"parents\":[";
        for (std::size_t index = 0U; index < derivation.parent_genome_identities.size(); ++index) {
            if (index != 0U) output += ',';
            output += json_string(derivation.parent_genome_identities[index]);
        }
        output += "],\"policy_version\":" + std::to_string(derivation.policy_version);
        output += ",\"seed\":";
        output += derivation.seed.has_value() ? json_string(*derivation.seed) : "null";
        output += ",\"descendant_index\":";
        output += derivation.descendant_index.has_value() ? std::to_string(*derivation.descendant_index) : "null";
        output += ",\"mutation_radius\":";
        output += derivation.mutation_radius.has_value() ? json_string(*derivation.mutation_radius) : "null";
        output += '}';
    }

    output += ",\"still\":";
    if (manifest.still_frame.has_value()) {
        output += "{\"frame\":" + std::to_string(*manifest.still_frame) + '}';
    } else {
        output += "null";
    }

    output += ",\"contact\":";
    if (manifest.kind == ExportKind::contact_sheet) {
        output += "{\"columns\":" + std::to_string(manifest.contact_columns);
        output += ",\"cell_width\":" + std::to_string(manifest.contact_cell_width);
        output += ",\"cell_height\":" + std::to_string(manifest.contact_cell_height);
        output += ",\"cells\":[";
        for (std::size_t index = 0U; index < manifest.contact_cells.size(); ++index) {
            if (index != 0U) output += ',';
            const ContactCellManifest& cell = manifest.contact_cells[index];
            output += "{\"cell_index\":" + std::to_string(cell.cell_index);
            output += ",\"order_key\":" + std::to_string(cell.order_key);
            output += ",\"genome_identity\":" + json_string(cell.genome_identity);
            output += ",\"canonical_genome\":" + embedded_genome_json(cell.canonical_genome);
            output += ",\"image_identity\":" + json_string(cell.image_identity) + '}';
        }
        output += "]}";
    } else {
        output += "null";
    }

    output += ",\"sequence\":";
    if (manifest.kind == ExportKind::frame_sequence) {
        output += "{\"frame_begin\":" + std::to_string(manifest.frame_begin.value_or(0U));
        output += ",\"frame_end_exclusive\":" + std::to_string(manifest.frame_end_exclusive.value_or(0U));
        output += ",\"rate_numerator\":" + std::to_string(manifest.rate_numerator.value_or(0U));
        output += ",\"rate_denominator\":" + std::to_string(manifest.rate_denominator.value_or(0U));
        output += ",\"filename_padding\":" + std::to_string(manifest.filename_padding);
        output += ",\"complete\":";
        output += manifest.sequence_complete ? "true" : "false";
        output += ",\"cancelled\":";
        output += manifest.sequence_cancelled ? "true" : "false";
        output += ",\"frames\":[";
        for (std::size_t index = 0U; index < manifest.frames.size(); ++index) {
            if (index != 0U) output += ',';
            const FrameManifest& frame = manifest.frames[index];
            output += "{\"frame_index\":" + std::to_string(frame.frame_index);
            output += ",\"filename\":" + json_string(frame.filename);
            output += ",\"image_identity\":" + json_string(frame.image_identity);
            output += ",\"frame_identity\":" + json_string(frame.frame_identity) + '}';
        }
        output += "]}";
    } else {
        output += "null";
    }
    output += "}\n";
    return output;
}

ManifestParseResult parse_export_manifest(
    const std::string_view text,
    const core::OperatorRegistry& registry) {
    const core::json::ParseResult parsed = core::json::parse(text);
    if (!parsed.value.has_value()) {
        return manifest_fail(parsed.error.has_value()
            ? "manifest JSON parse failed at byte " + std::to_string(parsed.error->offset) + ": " + parsed.error->message
            : "manifest JSON parse failed");
    }
    const Value& root = *parsed.value;
    if (root.type != ValueType::object) return manifest_fail("manifest root must be an object");
    std::string unexpected;
    if (!only_fields(root, {
            "manifest_version", "kind", "application_version", "engine_contract_version", "genome_schema_version",
            "source", "genome", "output", "derivation", "still", "contact", "sequence"}, unexpected)) {
        return manifest_fail("unexpected manifest field: " + unexpected);
    }

    const Value* manifest_version = field(root, "manifest_version");
    const Value* kind_value = field(root, "kind");
    const Value* application = field(root, "application_version");
    const Value* engine = field(root, "engine_contract_version");
    const Value* schema = field(root, "genome_schema_version");
    const Value* source = field(root, "source");
    const Value* genome_object = field(root, "genome");
    const Value* output_object = field(root, "output");
    const Value* derivation_value = field(root, "derivation");
    const Value* still = field(root, "still");
    const Value* contact = field(root, "contact");
    const Value* sequence = field(root, "sequence");
    if (manifest_version == nullptr || kind_value == nullptr || application == nullptr || engine == nullptr || schema == nullptr ||
        source == nullptr || genome_object == nullptr || output_object == nullptr || derivation_value == nullptr ||
        still == nullptr || contact == nullptr || sequence == nullptr) {
        return manifest_fail("manifest is missing one or more required top-level fields");
    }

    ExportManifest manifest;
    if (!parse_u32(*manifest_version, manifest.manifest_version) || manifest.manifest_version != kExportManifestSchemaVersion) {
        return manifest_fail("unsupported export manifest version");
    }
    if (kind_value->type != ValueType::string) return manifest_fail("kind must be a string");
    const auto kind = parse_export_kind(kind_value->text);
    if (!kind.has_value()) return manifest_fail("unsupported export kind");
    manifest.kind = *kind;
    if (application->type != ValueType::string || application->text.empty()) return manifest_fail("application_version must be a non-empty string");
    manifest.application_version = application->text;
    if (!parse_u32(*engine, manifest.engine_contract_version) || !parse_u32(*schema, manifest.genome_schema_version)) {
        return manifest_fail("engine/schema versions must be unsigned 32-bit integers");
    }

    if (source->type != ValueType::object || !only_fields(*source, {"identity", "path"}, unexpected)) {
        return manifest_fail(source->type == ValueType::object ? "unexpected source field: " + unexpected : "source must be an object");
    }
    const Value* source_identity = field(*source, "identity");
    const Value* source_path = field(*source, "path");
    if (source_identity == nullptr || source_path == nullptr || source_identity->type != ValueType::string ||
        source_path->type != ValueType::string || !valid_hex_identity(source_identity->text)) {
        return manifest_fail("source identity/path fields are invalid");
    }
    manifest.source_identity = source_identity->text;
    manifest.source_path = source_path->text;

    if (genome_object->type != ValueType::object ||
        !only_fields(*genome_object, {"identity", "canonical", "root_seed", "operators"}, unexpected)) {
        return manifest_fail(genome_object->type == ValueType::object ? "unexpected genome field: " + unexpected : "genome must be an object");
    }
    const Value* genome_identity = field(*genome_object, "identity");
    const Value* canonical_genome = field(*genome_object, "canonical");
    const Value* root_seed = field(*genome_object, "root_seed");
    const Value* operators = field(*genome_object, "operators");
    if (genome_identity == nullptr || canonical_genome == nullptr || root_seed == nullptr || operators == nullptr ||
        genome_identity->type != ValueType::string || root_seed->type != ValueType::string || operators->type != ValueType::array ||
        !valid_hex_identity(genome_identity->text)) {
        return manifest_fail("genome identity/root_seed/operators fields are invalid");
    }
    std::string embedded_error;
    const auto genome = parse_embedded_genome(*canonical_genome, registry, embedded_error);
    if (!genome.has_value()) return manifest_fail(embedded_error);
    if (core::genome_identity_hex(*genome) != genome_identity->text) return manifest_fail("genome identity does not match canonical genome bytes");
    if (genome->root_seed.to_string() != root_seed->text) return manifest_fail("root_seed does not match canonical genome");
    if (genome->engine_contract_version != manifest.engine_contract_version || genome->schema_version != manifest.genome_schema_version) {
        return manifest_fail("top-level engine/schema versions do not match canonical genome");
    }
    manifest.genome_identity = genome_identity->text;
    manifest.canonical_genome = core::serialize_canonical_genome(*genome);
    manifest.root_seed = root_seed->text;
    for (const Value& value : operators->array) {
        OperatorManifest op;
        if (!parse_operator_manifest(value, op, embedded_error)) return manifest_fail(embedded_error);
        manifest.operators.push_back(std::move(op));
    }
    if (manifest.operators.size() != genome->operators.size()) return manifest_fail("operator summary count does not match canonical genome");
    for (std::size_t index = 0U; index < manifest.operators.size(); ++index) {
        const OperatorManifest& summary = manifest.operators[index];
        const core::OperatorInstance& instance = genome->operators[index];
        if (summary.instance_id != instance.instance_id.to_string() || summary.type_id != instance.type_id ||
            summary.type_version != instance.type_version || summary.enabled != instance.enabled) {
            return manifest_fail("operator summary does not match canonical genome at index " + std::to_string(index));
        }
    }

    if (output_object->type != ValueType::object ||
        !only_fields(*output_object, {"canonical_full_resolution", "width", "height", "format", "image_identity"}, unexpected)) {
        return manifest_fail(output_object->type == ValueType::object ? "unexpected output field: " + unexpected : "output must be an object");
    }
    const Value* canonical = field(*output_object, "canonical_full_resolution");
    const Value* width = field(*output_object, "width");
    const Value* height = field(*output_object, "height");
    const Value* format = field(*output_object, "format");
    const Value* image_identity = field(*output_object, "image_identity");
    if (canonical == nullptr || width == nullptr || height == nullptr || format == nullptr || image_identity == nullptr ||
        canonical->type != ValueType::boolean || format->type != ValueType::string || image_identity->type != ValueType::string ||
        !parse_u32(*width, manifest.output_width) || !parse_u32(*height, manifest.output_height)) {
        return manifest_fail("output fields are invalid");
    }
    if (!image_identity->text.empty() && !valid_hex_identity(image_identity->text)) return manifest_fail("output image identity is invalid");
    manifest.canonical_full_resolution = canonical->boolean;
    manifest.output_format = format->text;
    manifest.output_image_identity = image_identity->text;

    if (derivation_value->type != ValueType::null_value) {
        manifest.derivation = parse_derivation_manifest(*derivation_value, embedded_error);
        if (!manifest.derivation.has_value()) return manifest_fail(embedded_error);
    }

    if (still->type == ValueType::object) {
        if (!only_fields(*still, {"frame"}, unexpected)) return manifest_fail("unexpected still field: " + unexpected);
        const Value* frame = field(*still, "frame");
        std::uint64_t parsed_frame{};
        if (frame == nullptr || !parse_u64(*frame, parsed_frame)) return manifest_fail("still frame is invalid");
        manifest.still_frame = parsed_frame;
    } else if (still->type != ValueType::null_value) {
        return manifest_fail("still must be null or an object");
    }

    if (contact->type == ValueType::object) {
        if (!only_fields(*contact, {"columns", "cell_width", "cell_height", "cells"}, unexpected)) {
            return manifest_fail("unexpected contact field: " + unexpected);
        }
        const Value* columns = field(*contact, "columns");
        const Value* cell_width = field(*contact, "cell_width");
        const Value* cell_height = field(*contact, "cell_height");
        const Value* cells = field(*contact, "cells");
        if (columns == nullptr || cell_width == nullptr || cell_height == nullptr || cells == nullptr || cells->type != ValueType::array ||
            !parse_u32(*columns, manifest.contact_columns) || !parse_u32(*cell_width, manifest.contact_cell_width) ||
            !parse_u32(*cell_height, manifest.contact_cell_height)) {
            return manifest_fail("contact fields are invalid");
        }
        for (const Value& value : cells->array) {
            ContactCellManifest cell;
            if (!parse_contact_cell(value, registry, cell, embedded_error)) return manifest_fail(embedded_error);
            manifest.contact_cells.push_back(std::move(cell));
        }
    } else if (contact->type != ValueType::null_value) {
        return manifest_fail("contact must be null or an object");
    }

    if (sequence->type == ValueType::object) {
        if (!only_fields(*sequence, {
                "frame_begin", "frame_end_exclusive", "rate_numerator", "rate_denominator", "filename_padding",
                "complete", "cancelled", "frames"}, unexpected)) {
            return manifest_fail("unexpected sequence field: " + unexpected);
        }
        const Value* begin = field(*sequence, "frame_begin");
        const Value* end = field(*sequence, "frame_end_exclusive");
        const Value* rate_num = field(*sequence, "rate_numerator");
        const Value* rate_den = field(*sequence, "rate_denominator");
        const Value* padding = field(*sequence, "filename_padding");
        const Value* complete = field(*sequence, "complete");
        const Value* cancelled = field(*sequence, "cancelled");
        const Value* frames = field(*sequence, "frames");
        std::uint64_t parsed_begin{};
        std::uint64_t parsed_end{};
        std::uint64_t parsed_num{};
        std::uint64_t parsed_den{};
        if (begin == nullptr || end == nullptr || rate_num == nullptr || rate_den == nullptr || padding == nullptr || complete == nullptr ||
            cancelled == nullptr || frames == nullptr || complete->type != ValueType::boolean || cancelled->type != ValueType::boolean ||
            frames->type != ValueType::array || !parse_u64(*begin, parsed_begin) || !parse_u64(*end, parsed_end) ||
            !parse_u64(*rate_num, parsed_num) || !parse_u64(*rate_den, parsed_den) || !parse_u32(*padding, manifest.filename_padding) ||
            parsed_begin >= parsed_end || parsed_num == 0U || parsed_den == 0U) {
            return manifest_fail("sequence fields are invalid");
        }
        manifest.frame_begin = parsed_begin;
        manifest.frame_end_exclusive = parsed_end;
        manifest.rate_numerator = parsed_num;
        manifest.rate_denominator = parsed_den;
        manifest.sequence_complete = complete->boolean;
        manifest.sequence_cancelled = cancelled->boolean;
        for (const Value& value : frames->array) {
            FrameManifest frame;
            if (!parse_frame_manifest(value, frame, embedded_error)) return manifest_fail(embedded_error);
            manifest.frames.push_back(std::move(frame));
        }
    } else if (sequence->type != ValueType::null_value) {
        return manifest_fail("sequence must be null or an object");
    }

    if (manifest.kind == ExportKind::still && (!manifest.still_frame.has_value() || contact->type != ValueType::null_value || sequence->type != ValueType::null_value)) {
        return manifest_fail("still manifest has inconsistent kind-specific sections");
    }
    if (manifest.kind == ExportKind::contact_sheet && (contact->type != ValueType::object || still->type != ValueType::null_value || sequence->type != ValueType::null_value)) {
        return manifest_fail("contact-sheet manifest has inconsistent kind-specific sections");
    }
    if (manifest.kind == ExportKind::frame_sequence && (sequence->type != ValueType::object || still->type != ValueType::null_value || contact->type != ValueType::null_value)) {
        return manifest_fail("frame-sequence manifest has inconsistent kind-specific sections");
    }
    return ManifestParseResult{std::move(manifest), {}};
}

std::string sequence_frame_filename(
    const std::string_view stem,
    const std::uint64_t frame_index,
    const std::uint64_t frame_end_exclusive,
    const std::uint32_t minimum_padding) {
    std::uint32_t digits = 1U;
    std::uint64_t value = frame_end_exclusive == 0U ? frame_index : std::max(frame_index, frame_end_exclusive - 1U);
    while (value >= 10U) {
        value /= 10U;
        ++digits;
    }
    const std::uint32_t padding = std::max(minimum_padding, digits);
    std::ostringstream stream;
    stream << stem << '_' << std::setw(static_cast<int>(padding)) << std::setfill('0') << frame_index << ".png";
    return stream.str();
}

ExportResult export_still(
    const app::SessionModel& session,
    const StillExportRequest& request) {
    ExportResult result;
    if (!session.has_source() || session.full_source() == nullptr) {
        result.error = ExportError{"render canonical still", request.destination, "no source image is loaded"};
        return result;
    }
    if (request.destination.empty()) {
        result.error = ExportError{"validate still destination", request.destination, "destination path is empty"};
        return result;
    }
    const std::filesystem::path manifest_path = manifest_sidecar_path(request.destination);
    if (auto error = preflight_collision(request.destination, request.collision); error.has_value()) {
        result.error = std::move(error);
        return result;
    }
    if (request.write_manifest) {
        if (auto error = preflight_collision(manifest_path, request.collision); error.has_value()) {
            result.error = std::move(error);
            return result;
        }
    }

    const std::uint64_t frame = request.frame_index.value_or(session.current_frame());
    std::string render_error;
    auto image = session.render_full_at_frame(frame, &render_error);
    if (!image.has_value()) {
        result.error = ExportError{"render canonical still", request.destination, std::move(render_error)};
        return result;
    }

    ExportManifest manifest = base_manifest(session, ExportKind::still);
    manifest.still_frame = frame;
    manifest.output_width = image->width;
    manifest.output_height = image->height;
    manifest.output_image_identity = core::source_identity_hex(*image);

    if (auto error = atomic_write_png(*image, request.destination, request.collision); error.has_value()) {
        result.error = std::move(error);
        return result;
    }
    result.completed_files.push_back(request.destination);

    if (request.write_manifest) {
        if (auto error = atomic_write_text(manifest_path, serialize_export_manifest(manifest), request.collision); error.has_value()) {
            result.error = std::move(error);
            result.manifest = std::move(manifest);
            return result;
        }
        result.completed_files.push_back(manifest_path);
        result.manifest_path = manifest_path;
    }
    result.manifest = std::move(manifest);
    return result;
}

ExportResult export_contact_sheet(
    const app::SessionModel& session,
    const ContactSheetRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result;
    const core::ImageBuffer* source = session.full_source();
    if (!session.has_source() || source == nullptr) {
        result.error = ExportError{"render contact sheet", request.destination, "no source image is loaded"};
        return result;
    }
    if (request.destination.empty() || request.specimens.empty() || request.columns == 0U || request.cell_width == 0U || request.cell_height == 0U) {
        result.error = ExportError{"validate contact sheet request", request.destination, "destination, specimens, columns and cell dimensions must be non-empty/non-zero"};
        return result;
    }
    const std::uint64_t rows = (static_cast<std::uint64_t>(request.specimens.size()) + request.columns - 1U) / request.columns;
    const std::uint64_t width = static_cast<std::uint64_t>(request.columns) * request.cell_width;
    const std::uint64_t height = rows * request.cell_height;
    if (width == 0U || height == 0U || width > std::numeric_limits<std::uint32_t>::max() || height > std::numeric_limits<std::uint32_t>::max()) {
        result.error = ExportError{"validate contact sheet dimensions", request.destination, "contact sheet dimensions exceed the canonical image contract"};
        return result;
    }
    const std::filesystem::path manifest_path = manifest_sidecar_path(request.destination);
    if (auto error = preflight_collision(request.destination, request.collision); error.has_value()) {
        result.error = std::move(error);
        return result;
    }
    if (request.write_manifest) {
        if (auto error = preflight_collision(manifest_path, request.collision); error.has_value()) {
            result.error = std::move(error);
            return result;
        }
    }

    auto sheet_created = core::make_rgba8_image(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    if (!sheet_created.ok()) {
        result.error = ExportError{"allocate contact sheet", request.destination, sheet_created.error->message};
        return result;
    }
    core::ImageBuffer sheet = std::move(*sheet_created.image);
    for (std::size_t offset = 0U; offset < sheet.bytes.size(); offset += 4U) {
        sheet.bytes[offset + 0U] = 32U;
        sheet.bytes[offset + 1U] = 32U;
        sheet.bytes[offset + 2U] = 32U;
        sheet.bytes[offset + 3U] = 255U;
    }

    ExportManifest manifest = base_manifest(session, ExportKind::contact_sheet);
    manifest.canonical_full_resolution = false;
    manifest.output_width = sheet.width;
    manifest.output_height = sheet.height;
    manifest.output_format = "png-rgba8-contact-sheet-presentation";
    manifest.contact_columns = request.columns;
    manifest.contact_cell_width = request.cell_width;
    manifest.contact_cell_height = request.cell_height;
    manifest.contact_cells.reserve(request.specimens.size());

    for (std::size_t index = 0U; index < request.specimens.size(); ++index) {
        if (should_cancel && should_cancel()) {
            result.cancelled = true;
            result.manifest = std::move(manifest);
            return result;
        }
        const ContactSheetSpecimen& specimen = request.specimens[index];
        const std::string identity = core::genome_identity_hex(specimen.genome);
        if (identity != specimen.genome_identity) {
            result.error = ExportError{"validate contact specimen", request.destination, "specimen genome identity mismatch at ordered cell " + std::to_string(index)};
            return result;
        }
        if (const auto validation = core::validate_genome(specimen.genome, session.registry().schema_registry()); validation.has_value()) {
            result.error = ExportError{"validate contact specimen", request.destination, "invalid specimen at ordered cell " + std::to_string(index) + ": " + validation->message};
            return result;
        }
        core::RenderResult rendered = core::render_pipeline_at_frame(*source, specimen.genome, session.registry(), request.frame_index);
        if (!rendered.ok()) {
            result.error = ExportError{"render contact specimen", request.destination, "cell " + std::to_string(index) + ": " + render_error_text(rendered)};
            return result;
        }
        const std::string image_identity = core::source_identity_hex(*rendered.image);
        const core::ProxySpec proxy_spec{request.cell_width, request.cell_height, core::kProxyMethodVersion};
        core::ProxyResult thumbnail = core::make_nearest_proxy(*rendered.image, image_identity, proxy_spec);
        if (!thumbnail.ok()) {
            result.error = ExportError{"scale contact specimen", request.destination, "cell " + std::to_string(index) + ": " + thumbnail.error->message};
            return result;
        }
        const std::uint32_t column = static_cast<std::uint32_t>(index % request.columns);
        const std::uint32_t row = static_cast<std::uint32_t>(index / request.columns);
        if (!copy_proxy_into_cell(
                sheet,
                thumbnail.proxy->image,
                column * request.cell_width,
                row * request.cell_height,
                request.cell_width,
                request.cell_height)) {
            result.error = ExportError{"compose contact sheet", request.destination, "scaled specimen exceeded its deterministic cell bounds"};
            return result;
        }
        manifest.contact_cells.push_back(ContactCellManifest{
            static_cast<std::uint64_t>(index), specimen.order_key, identity,
            core::serialize_canonical_genome(specimen.genome), image_identity});
        if (progress) progress(index + 1U, request.specimens.size(), request.destination);
    }

    manifest.output_image_identity = core::source_identity_hex(sheet);
    if (auto error = atomic_write_png(sheet, request.destination, request.collision); error.has_value()) {
        result.error = std::move(error);
        result.manifest = std::move(manifest);
        return result;
    }
    result.completed_files.push_back(request.destination);
    if (request.write_manifest) {
        if (auto error = atomic_write_text(manifest_path, serialize_export_manifest(manifest), request.collision); error.has_value()) {
            result.error = std::move(error);
            result.manifest = std::move(manifest);
            return result;
        }
        result.completed_files.push_back(manifest_path);
        result.manifest_path = manifest_path;
    }
    result.manifest = std::move(manifest);
    return result;
}

ExportResult export_frame_sequence(
    const app::SessionModel& session,
    const FrameSequenceRequest& request,
    CancelCallback should_cancel,
    ProgressCallback progress) {
    ExportResult result;
    const core::ImageBuffer* source = session.full_source();
    if (!session.has_source() || source == nullptr) {
        result.error = ExportError{"render frame sequence", request.directory, "no source image is loaded"};
        return result;
    }
    if (request.directory.empty() || request.stem.empty() || request.frame_begin >= request.frame_end_exclusive ||
        request.minimum_padding == 0U || request.minimum_padding > 20U ||
        request.stem.find('/') != std::string::npos || request.stem.find('\\') != std::string::npos) {
        result.error = ExportError{"validate frame sequence request", request.directory, "directory/stem/range/padding is invalid"};
        return result;
    }
    const std::uint64_t total_u64 = request.frame_end_exclusive - request.frame_begin;
    if (total_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        result.error = ExportError{"validate frame sequence request", request.directory, "frame count exceeds host size limits"};
        return result;
    }
    const std::size_t total = static_cast<std::size_t>(total_u64);

    std::error_code fs_error;
    if (!std::filesystem::exists(request.directory, fs_error)) {
        if (fs_error) {
            result.error = ExportError{"inspect frame sequence directory", request.directory, fs_error.message()};
            return result;
        }
        if (!std::filesystem::create_directories(request.directory, fs_error) || fs_error) {
            result.error = ExportError{"create frame sequence directory", request.directory, fs_error ? fs_error.message() : "directory creation failed"};
            return result;
        }
    }
    if (!std::filesystem::is_directory(request.directory, fs_error) || fs_error) {
        result.error = ExportError{"inspect frame sequence directory", request.directory, fs_error ? fs_error.message() : "destination is not a directory"};
        return result;
    }

    const std::string first_name = sequence_frame_filename(request.stem, request.frame_begin, request.frame_end_exclusive, request.minimum_padding);
    const std::size_t underscore = first_name.rfind('_');
    const std::size_t dot = first_name.rfind('.');
    const std::uint32_t padding = underscore == std::string::npos || dot == std::string::npos || dot <= underscore + 1U
        ? request.minimum_padding
        : static_cast<std::uint32_t>(dot - underscore - 1U);
    const std::filesystem::path manifest_path = request.directory / (request.stem + ".fmmanifest.json");

    if (request.write_manifest) {
        if (auto error = preflight_collision(manifest_path, request.collision); error.has_value()) {
            result.error = std::move(error);
            return result;
        }
    }
    if (request.collision == CollisionPolicy::fail_if_exists) {
        for (std::uint64_t frame = request.frame_begin; frame < request.frame_end_exclusive; ++frame) {
            const std::filesystem::path destination = request.directory /
                sequence_frame_filename(request.stem, frame, request.frame_end_exclusive, request.minimum_padding);
            if (auto error = preflight_collision(destination, request.collision); error.has_value()) {
                result.error = std::move(error);
                return result;
            }
        }
    }

    ExportManifest manifest = base_manifest(session, ExportKind::frame_sequence);
    manifest.output_width = source->width;
    manifest.output_height = source->height;
    manifest.output_format = "png-rgba8-frame-sequence";
    manifest.frame_begin = request.frame_begin;
    manifest.frame_end_exclusive = request.frame_end_exclusive;
    const app::TimelineRate rate = session.semantic_timeline_rate();
    manifest.rate_numerator = rate.numerator;
    manifest.rate_denominator = rate.denominator;
    manifest.filename_padding = padding;
    manifest.sequence_complete = false;
    manifest.sequence_cancelled = false;
    manifest.frames.reserve(total);

    std::size_t completed = 0U;
    for (std::uint64_t frame = request.frame_begin; frame < request.frame_end_exclusive; ++frame) {
        if (should_cancel && should_cancel()) {
            result.cancelled = true;
            manifest.sequence_cancelled = true;
            if (request.write_manifest) {
                if (auto error = atomic_write_text(manifest_path, serialize_export_manifest(manifest), request.collision); error.has_value()) {
                    result.error = std::move(error);
                } else {
                    result.completed_files.push_back(manifest_path);
                    result.manifest_path = manifest_path;
                }
            }
            result.manifest = std::move(manifest);
            return result;
        }
        const std::string filename = sequence_frame_filename(request.stem, frame, request.frame_end_exclusive, request.minimum_padding);
        const std::filesystem::path destination = request.directory / filename;
        std::string render_error;
        auto image = session.render_full_at_frame(frame, &render_error);
        if (!image.has_value()) {
            result.error = ExportError{"render canonical sequence frame", destination, std::move(render_error)};
            result.manifest = std::move(manifest);
            return result;
        }
        if (image->width != source->width || image->height != source->height) {
            result.error = ExportError{"validate sequence frame dimensions", destination, "canonical frame dimensions changed unexpectedly"};
            result.manifest = std::move(manifest);
            return result;
        }
        if (auto error = atomic_write_png(*image, destination, request.collision); error.has_value()) {
            result.error = std::move(error);
            result.manifest = std::move(manifest);
            return result;
        }
        result.completed_files.push_back(destination);
        const std::string image_identity = core::source_identity_hex(*image);
        manifest.frames.push_back(FrameManifest{
            frame,
            filename,
            image_identity,
            core::temporal_frame_identity_hex(session.source_identity(), session.genome(), frame, *image)});
        ++completed;
        if (progress) progress(completed, total, destination);
    }

    manifest.sequence_complete = true;
    if (request.write_manifest) {
        if (auto error = atomic_write_text(manifest_path, serialize_export_manifest(manifest), request.collision); error.has_value()) {
            result.error = std::move(error);
            result.manifest = std::move(manifest);
            return result;
        }
        result.completed_files.push_back(manifest_path);
        result.manifest_path = manifest_path;
    }
    result.manifest = std::move(manifest);
    return result;
}

}  // namespace faultmine::exporting
