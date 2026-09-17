#include "faultmine/export.hpp"
#include "faultmine/lab_worker.hpp"
#include "faultmine/laboratory.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] std::optional<std::vector<std::uint8_t>> read_bounded(
    const std::filesystem::path& path,
    const std::uint64_t maximum) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size == 0U || size > maximum || size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max())) return std::nullopt;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) return std::nullopt;
    return bytes;
}

[[nodiscard]] bool write_text(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] std::filesystem::path sibling_worker() {
    std::wstring buffer(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) return {};
    buffer.resize(length);
    return std::filesystem::path{buffer}.parent_path() / L"FAULTMINE-lab-worker.exe";
}

template <typename Integer>
[[nodiscard]] bool parse_integer(const std::wstring_view text, Integer& output) {
    std::string narrow(text.begin(), text.end());
    const char* begin = narrow.data();
    const char* end = begin + narrow.size();
    const auto result = std::from_chars(begin, end, output, 0);
    return result.ec == std::errc{} && result.ptr == end;
}

void print_usage() {
    std::wcerr <<
        L"FAULTMINE laboratory 0.13.0\n\n"
        L"External decoder (isolated; output frozen before canonical use):\n"
        L"  FAULTMINE-lab decode <encoded-input> <materialized.png> [seed-u64] [bit-flips] [protected-prefix]\n\n"
        L"Arbitrary binary interpretation (pure deterministic core):\n"
        L"  FAULTMINE-lab raw <binary-input> <output.png> <width> <height-or-0> <bytes-per-pixel> [offset] [stride]\n";
}

int decode_command(const int argc, wchar_t** argv) {
    if (argc < 4 || argc > 7) {
        print_usage();
        return 2;
    }
    const std::filesystem::path input_path{argv[2]};
    const std::filesystem::path output_path{argv[3]};
    auto encoded = read_bounded(input_path, faultmine::laboratory::kLabWorkerMaximumEncodedBytes);
    if (!encoded.has_value()) {
        std::wcerr << L"Input is missing, empty, unreadable, or exceeds the 64 MiB laboratory bound.\n";
        return 3;
    }

    std::uint64_t seed_value = 0x464d4c4142303133ULL;
    std::uint64_t flips = 1U;
    std::uint64_t protect = 0U;
    if (argc >= 5 && !parse_integer<std::uint64_t>(argv[4], seed_value)) return 4;
    if (argc >= 6 && !parse_integer<std::uint64_t>(argv[5], flips)) return 4;
    if (argc >= 7 && !parse_integer<std::uint64_t>(argv[6], protect)) return 4;

    faultmine::core::EncodedMutationPlan plan;
    plan.protected_prefix_bytes = protect;
    plan.random_bit_flips = flips;
    std::string error;
    auto mutation = faultmine::core::mutate_encoded_bytes(*encoded, plan, faultmine::core::RootSeed{seed_value}, &error);
    if (!mutation.has_value()) {
        std::cerr << "Mutation rejected: " << error << '\n';
        return 5;
    }

    const std::filesystem::path worker = sibling_worker();
    if (worker.empty() || !std::filesystem::exists(worker)) {
        std::wcerr << L"FAULTMINE-lab-worker.exe is not beside the laboratory executable.\n";
        return 6;
    }
    std::cout << "EXTERNAL DECODER: mutated encoded bytes are being decoded in a bounded worker. Decoder pixels are not canonical yet.\n";
    faultmine::laboratory::WorkerRequest request;
    request.worker_executable = worker;
    request.encoded_bytes = mutation->bytes;
    request.mode = faultmine::laboratory::WorkerMode::decode_wic;
    request.timeout_ms = 5000U;
    const faultmine::laboratory::WorkerResult worker_result = faultmine::laboratory::run_lab_worker(request);
    if (!worker_result.success()) {
        std::cerr << "Worker outcome: " << faultmine::core::laboratory_outcome_name(worker_result.outcome)
                  << " | " << worker_result.diagnostic << '\n';
        return 7;
    }

    const std::string materialized_identity = faultmine::core::source_identity_hex(*worker_result.image);
    faultmine::core::LaboratoryProvenance provenance;
    provenance.original_encoded_identity = mutation->original_identity;
    provenance.mutated_encoded_identity = mutation->mutated_identity;
    provenance.mutation_seed = faultmine::core::RootSeed{seed_value}.to_string();
    provenance.mutation_parameters = faultmine::core::encoded_mutation_plan_text(plan);
    provenance.worker_protocol_version = faultmine::laboratory::kLabWorkerProtocolVersion;
    provenance.worker_application_version = std::string{faultmine::exporting::kApplicationVersion};
    provenance.decoder_identifier = worker_result.decoder_identifier;
    provenance.os_build = worker_result.os_build;
    provenance.outcome = faultmine::core::LaboratoryOutcome::success;
    provenance.diagnostic = worker_result.diagnostic;
    provenance.materialized_source_identity = materialized_identity;
    faultmine::core::MaterializedLaboratorySource materialized{*worker_result.image, provenance};
    if (!faultmine::core::validate_materialized_laboratory_source(materialized, &error)) {
        std::cerr << "Worker pixels were not materializable: " << error << '\n';
        return 8;
    }

    if (const auto save_error = faultmine::io::save_wic_png(materialized.image, output_path); save_error.has_value()) {
        std::cerr << "Could not write frozen PNG: " << save_error->message << '\n';
        return 9;
    }

    faultmine::app::SessionModel session;
    if (!session.set_materialized_laboratory_source(std::move(materialized), output_path, &error)) {
        std::cerr << "Could not adopt frozen source: " << error << '\n';
        return 10;
    }
    auto project = session.make_project_document(&error);
    if (!project.has_value()) {
        std::cerr << "Could not create frozen-source project: " << error << '\n';
        return 11;
    }
    std::filesystem::path project_path = output_path;
    project_path += L".fmproj";
    const std::string project_text = faultmine::app::serialize_project_canonical(*project);
    if (project_text.empty() || !write_text(project_path, project_text)) {
        std::wcerr << L"Could not write frozen-source project.\n";
        return 12;
    }

    std::cout << "FROZEN/MATERIALIZED: normalized RGBA8 pixels now have canonical source identity " << materialized_identity << "\n";
    std::wcout << L"PNG: " << output_path << L"\nProject: " << project_path << L"\n";
    std::cout << "Reopening the project uses the frozen PNG/embedded v3 pixels; it never needs to rerun the malformed decoder.\n";
    return 0;
}

int raw_command(const int argc, wchar_t** argv) {
    if (argc < 7 || argc > 9) {
        print_usage();
        return 2;
    }
    auto input = read_bounded(argv[2], faultmine::laboratory::kLabWorkerMaximumEncodedBytes);
    if (!input.has_value()) return 3;
    faultmine::core::RawBinarySpec spec;
    if (!parse_integer<std::uint32_t>(argv[4], spec.width) || !parse_integer<std::uint32_t>(argv[5], spec.height) ||
        !parse_integer<std::uint32_t>(argv[6], spec.bytes_per_pixel)) return 4;
    if (argc >= 8 && !parse_integer<std::uint64_t>(argv[7], spec.offset)) return 4;
    if (argc >= 9 && !parse_integer<std::uint64_t>(argv[8], spec.stride)) return 4;
    spec.boundary = faultmine::core::RawBinaryBoundary::wrap;
    std::string error;
    auto interpreted = faultmine::core::interpret_raw_binary(*input, spec, &error);
    if (!interpreted.has_value()) {
        std::cerr << "Raw interpretation rejected: " << error << '\n';
        return 5;
    }
    if (const auto save_error = faultmine::io::save_wic_png(interpreted->image, argv[3]); save_error.has_value()) {
        std::cerr << "Could not write interpreted PNG: " << save_error->message << '\n';
        return 6;
    }
    std::cout << "Deterministic raw-binary interpretation source identity: " << interpreted->source_identity << '\n';
    return 0;
}

}  // namespace

int wmain(const int argc, wchar_t** argv) {
    if (argc < 2) {
        print_usage();
        return 2;
    }
    const std::wstring_view command{argv[1]};
    if (command == L"decode") return decode_command(argc, argv);
    if (command == L"raw") return raw_command(argc, argv);
    print_usage();
    return 2;
}
