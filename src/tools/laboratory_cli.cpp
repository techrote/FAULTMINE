#include "faultmine/laboratory.hpp"
#include "faultmine/laboratory_worker.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/wic_io.hpp"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] bool read_file_bounded(
    const std::filesystem::path& path,
    const std::size_t maximum,
    std::vector<std::uint8_t>& output,
    std::string& error) {
    std::error_code filesystem_error;
    const std::uintmax_t size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error) {
        error = filesystem_error.message();
        return false;
    }
    if (size > maximum) {
        error = "file exceeds the requested laboratory resource limit";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "could not open file";
        return false;
    }
    output.resize(static_cast<std::size_t>(size));
    if (!output.empty()) stream.read(reinterpret_cast<char*>(output.data()), static_cast<std::streamsize>(output.size()));
    if (!stream) {
        error = "could not read complete file";
        return false;
    }
    return true;
}

[[nodiscard]] bool read_text(
    const std::filesystem::path& path,
    const std::size_t maximum,
    std::string& output,
    std::string& error) {
    std::vector<std::uint8_t> bytes;
    if (!read_file_bounded(path, maximum, bytes, error)) return false;
    output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

[[nodiscard]] bool write_text(const std::filesystem::path& path, const std::string_view text, std::string& error) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "could not open output text file";
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream) {
        error = "could not commit output text file";
        return false;
    }
    return true;
}

[[nodiscard]] std::optional<std::uint64_t> parse_u64(const std::wstring_view text) noexcept {
    if (text.empty()) return std::nullopt;
    std::string narrow;
    narrow.reserve(text.size());
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') return std::nullopt;
        narrow.push_back(static_cast<char>(character));
    }
    std::uint64_t value{};
    const auto parsed = std::from_chars(narrow.data(), narrow.data() + narrow.size(), value, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != narrow.data() + narrow.size()) return std::nullopt;
    return value;
}

[[nodiscard]] std::filesystem::path worker_sibling(const std::filesystem::path& executable) {
    return executable.parent_path() / L"FAULTMINE-lab-worker.exe";
}

void usage() {
    std::wcerr
        << L"FAULTMINE laboratory 0.13.0\n"
        << L"External-decoder results are NON-CANONICAL until their normalized pixels are materialized.\n\n"
        << L"Rerun malformed-codec experiment:\n"
        << L"  FAULTMINE-lab decode <encoded-input> <materialized.png> [seed] [flip-count] [protected-prefix]\n\n"
        << L"Reuse frozen result without decoder:\n"
        << L"  FAULTMINE-lab reuse <source.fmlabsource.json> <materialized.png>\n"
        << L"  FAULTMINE-lab reuse-project <project.fmproj> <materialized.png>\n\n"
        << L"Canonical arbitrary-binary interpretation (no worker/external decoder):\n"
        << L"  FAULTMINE-lab raw <binary-input> <output.png> <width> <height> <offset> <stride> <gray8|rgb8|rgba8|bgra8> <drop|wrap|fill> [fill-byte]\n";
}

[[nodiscard]] std::optional<faultmine::laboratory::RawFormat> raw_format(const std::wstring_view text) noexcept {
    using faultmine::laboratory::RawFormat;
    if (text == L"gray8") return RawFormat::gray8;
    if (text == L"rgb8") return RawFormat::rgb8;
    if (text == L"rgba8") return RawFormat::rgba8;
    if (text == L"bgra8") return RawFormat::bgra8;
    return std::nullopt;
}

[[nodiscard]] std::optional<faultmine::laboratory::RawBoundaryPolicy> raw_boundary(const std::wstring_view text) noexcept {
    using faultmine::laboratory::RawBoundaryPolicy;
    if (text == L"drop") return RawBoundaryPolicy::drop;
    if (text == L"wrap") return RawBoundaryPolicy::wrap;
    if (text == L"fill") return RawBoundaryPolicy::fill;
    return std::nullopt;
}

int save_materialized_outputs(
    const faultmine::laboratory::MaterializedSource& materialized,
    const std::filesystem::path& png_path) {
    using namespace faultmine;
    if (const auto save = io::save_wic_png(materialized.image, png_path); save.has_value()) {
        std::cerr << "Materialized PNG write failed: " << save->message << '\n';
        return 20;
    }
    std::string error;
    std::filesystem::path bundle = png_path;
    bundle += L".fmlabsource.json";
    if (!write_text(bundle, laboratory::serialize_materialized_source(materialized), error)) {
        std::cerr << "Frozen source bundle write failed: " << error << '\n';
        return 21;
    }

    app::SessionModel session;
    if (!session.set_materialized_laboratory_source(materialized, png_path, &error)) {
        std::cerr << "Session rejected materialized laboratory source: " << error << '\n';
        return 22;
    }
    const auto project = session.make_project_document(&error);
    if (!project.has_value()) {
        std::cerr << "Could not create provenance-bearing project: " << error << '\n';
        return 23;
    }
    std::filesystem::path project_path = png_path;
    project_path += L".fmproj";
    const std::string project_text = app::serialize_project_canonical(*project);
    if (project_text.empty() || !write_text(project_path, project_text, error)) {
        std::cerr << "Provenance-bearing project write failed: " << error << '\n';
        return 24;
    }
    std::cout << "Materialized/frozen source identity: " << core::source_identity_hex(materialized.image) << '\n';
    std::cout << "Frozen source bundle and .fmproj preserve external-decoder provenance; downstream FAULTMINE transforms are canonical from these pixels.\n";
    return 0;
}

int run_decode(const int argc, wchar_t** argv) {
    using namespace faultmine;
    if (argc < 4 || argc > 7) {
        usage();
        return 2;
    }
    std::vector<std::uint8_t> encoded;
    std::string error;
    if (!read_file_bounded(argv[2], laboratory::kMaxLaboratoryEncodedBytes, encoded, error)) {
        std::cerr << "Encoded input read failed: " << error << '\n';
        return 3;
    }
    const std::uint64_t seed_value = argc >= 5 ? parse_u64(argv[4]).value_or(std::numeric_limits<std::uint64_t>::max()) : 1U;
    const std::uint64_t flips = argc >= 6 ? parse_u64(argv[5]).value_or(0U) : 8U;
    const std::uint64_t protect = argc >= 7 ? parse_u64(argv[6]).value_or(std::numeric_limits<std::uint64_t>::max()) : 16U;
    if (seed_value == std::numeric_limits<std::uint64_t>::max() || flips == 0U ||
        protect == std::numeric_limits<std::uint64_t>::max() || protect >= encoded.size()) {
        std::cerr << "Invalid seed/flip-count/protected-prefix parameters.\n";
        return 4;
    }

    laboratory::ByteMutationPlan plan;
    plan.seed = core::RootSeed{seed_value};
    plan.protected_prefix = protect;
    plan.operations.push_back(laboratory::ByteMutationOperation{
        laboratory::ByteMutationKind::bit_flip,
        protect,
        static_cast<std::uint64_t>(encoded.size()) - protect,
        0U,
        flips});
    const laboratory::ByteMutationResult mutation = laboratory::mutate_encoded_bytes(encoded, plan);
    if (!mutation.ok()) {
        std::cerr << "Deterministic byte mutation failed: " << mutation.error << '\n';
        return 5;
    }

    laboratory::WorkerOptions options;
    options.executable = worker_sibling(std::filesystem::absolute(argv[0]));
    options.timeout_ms = 5000U;
    const laboratory::WorkerResult result = laboratory::run_decoder_worker(
        mutation.bytes,
        mutation.original_identity,
        mutation.canonical_recipe,
        plan.seed,
        options);
    std::cout << "External-decoder experiment outcome: " << laboratory::outcome_name(result.provenance.outcome) << '\n';
    if (!result.provenance.diagnostic.empty()) std::cout << "Worker: " << result.provenance.diagnostic << '\n';
    if (!result.ok()) {
        if (!result.host_error.empty()) std::cerr << "Containment/IPC error: " << result.host_error << '\n';
        return result.provenance.outcome == laboratory::Outcome::decode_failure ? 10 : 11;
    }
    return save_materialized_outputs(*result.materialized, argv[3]);
}

int run_reuse(const int argc, wchar_t** argv) {
    using namespace faultmine;
    if (argc != 4) {
        usage();
        return 2;
    }
    std::string text;
    std::string error;
    if (!read_text(argv[2], laboratory::kMaxLaboratoryPixelBytes * 2U + 64U * 1024U, text, error)) {
        std::cerr << "Frozen source read failed: " << error << '\n';
        return 3;
    }
    const laboratory::MaterializedParseResult parsed = laboratory::parse_materialized_source(text);
    if (!parsed.ok()) {
        std::cerr << "Frozen source rejected: " << parsed.error << '\n';
        return 4;
    }
    if (const auto save = io::save_wic_png(parsed.source->image, argv[3]); save.has_value()) {
        std::cerr << "Frozen source PNG write failed: " << save->message << '\n';
        return 5;
    }
    std::cout << "Reused frozen normalized pixels without launching an external decoder.\n";
    return 0;
}

int run_reuse_project(const int argc, wchar_t** argv) {
    using namespace faultmine;
    if (argc != 4) {
        usage();
        return 2;
    }
    std::string text;
    std::string error;
    if (!read_text(argv[2], laboratory::kMaxLaboratoryPixelBytes * 2U + 2U * 1024U * 1024U, text, error)) {
        std::cerr << "Project read failed: " << error << '\n';
        return 3;
    }
    app::SessionModel registry_source;
    const app::ProjectParseResult project = app::parse_project(text, registry_source.registry().schema_registry());
    if (!project.ok()) {
        std::cerr << "Project rejected: " << (project.error.has_value() ? project.error->message : "unknown error") << '\n';
        return 4;
    }
    if (!project.project->source.laboratory.has_value()) {
        std::cerr << "Project does not contain a frozen laboratory source.\n";
        return 5;
    }
    if (const auto save = io::save_wic_png(project.project->source.laboratory->image, argv[3]); save.has_value()) {
        std::cerr << "Frozen project source PNG write failed: " << save->message << '\n';
        return 6;
    }
    std::cout << "Reused project-embedded frozen normalized pixels without rerunning the malformed-codec worker.\n";
    return 0;
}

int run_raw(const int argc, wchar_t** argv) {
    using namespace faultmine;
    if (argc < 10 || argc > 11) {
        usage();
        return 2;
    }
    const auto width = parse_u64(argv[4]);
    const auto height = parse_u64(argv[5]);
    const auto offset = parse_u64(argv[6]);
    const auto stride = parse_u64(argv[7]);
    const auto format = raw_format(argv[8]);
    const auto boundary = raw_boundary(argv[9]);
    const auto fill = argc == 11 ? parse_u64(argv[10]) : std::optional<std::uint64_t>{0U};
    if (!width.has_value() || *width == 0U || *width > std::numeric_limits<std::uint32_t>::max() ||
        !height.has_value() || *height > std::numeric_limits<std::uint32_t>::max() ||
        !offset.has_value() || !stride.has_value() || !format.has_value() || !boundary.has_value() ||
        !fill.has_value() || *fill > 255U) {
        std::cerr << "Invalid raw interpretation parameters.\n";
        return 3;
    }
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!read_file_bounded(argv[2], laboratory::kMaxLaboratoryEncodedBytes, bytes, error)) {
        std::cerr << "Raw input read failed: " << error << '\n';
        return 4;
    }
    laboratory::RawBinarySpec spec;
    spec.width = static_cast<std::uint32_t>(*width);
    spec.height = static_cast<std::uint32_t>(*height);
    spec.offset = *offset;
    spec.stride = *stride;
    spec.format = *format;
    spec.boundary = *boundary;
    spec.fill_byte = static_cast<std::uint8_t>(*fill);
    const laboratory::RawInterpretResult interpreted = laboratory::interpret_binary_as_image(bytes, spec);
    if (!interpreted.ok()) {
        std::cerr << "Canonical raw interpretation failed: " << interpreted.error << '\n';
        return 5;
    }
    if (const auto save = io::save_wic_png(*interpreted.image, argv[3]); save.has_value()) {
        std::cerr << "Raw interpretation PNG write failed: " << save->message << '\n';
        return 6;
    }
    std::cout << "Canonical arbitrary-binary interpretation completed in-process; no external decoder worker was used.\n";
    std::cout << "Source identity: " << core::source_identity_hex(*interpreted.image) << '\n';
    return 0;
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc < 2) {
        usage();
        return 2;
    }
    const std::wstring_view command{argv[1]};
    if (command == L"decode") return run_decode(argc, argv);
    if (command == L"reuse") return run_reuse(argc, argv);
    if (command == L"reuse-project") return run_reuse_project(argc, argv);
    if (command == L"raw") return run_raw(argc, argv);
    usage();
    return 2;
}
