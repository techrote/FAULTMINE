#include "faultmine/export.hpp"
#include "faultmine/laboratory.hpp"
#include "faultmine/laboratory_worker.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/sha256.hpp"
#include "faultmine/wic_io.hpp"
#include "io/laboratory_protocol.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_failures = 0;

void expect(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Left, typename Right>
void expect_equal(const Left& left, const Right& right, const std::string_view message) {
    expect(left == right, message);
}

[[nodiscard]] std::filesystem::path executable_directory() {
    std::vector<wchar_t> buffer(32768U);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0U || length >= buffer.size()) throw std::runtime_error("GetModuleFileNameW failed");
    return std::filesystem::path(buffer.data()).parent_path();
}

[[nodiscard]] std::filesystem::path worker_path() {
    return executable_directory() / L"FAULTMINE-lab-worker.exe";
}

[[nodiscard]] std::filesystem::path fresh_directory(const std::string_view name) {
    std::filesystem::path result = std::filesystem::temp_directory_path() / "faultmine-fm013-tests" / std::string{name};
    std::error_code ignored;
    std::filesystem::remove_all(result, ignored);
    std::filesystem::create_directories(result);
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> read_binary(const std::filesystem::path& path) {
    const std::uintmax_t size = std::filesystem::file_size(path);
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("could not open test file");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("could not read test file");
    return bytes;
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> bytes = read_binary(path);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

[[nodiscard]] faultmine::laboratory::WorkerResult synthetic(
    const faultmine::laboratory::WorkerSyntheticMode mode,
    const std::uint32_t timeout_ms = 1000U,
    std::function<bool()> cancel = {}) {
    using namespace faultmine;
    const std::array<std::uint8_t, 4> payload{1U, 2U, 3U, 4U};
    laboratory::WorkerOptions options;
    options.executable = worker_path();
    options.synthetic_mode = mode;
    options.timeout_ms = timeout_ms;
    options.should_cancel = std::move(cancel);
    return laboratory::run_decoder_worker(
        payload,
        core::sha256_hex(payload),
        "{\"test\":true}",
        core::RootSeed{1U},
        options);
}

void test_deterministic_encoded_mutation() {
    using namespace faultmine;
    const std::array<std::uint8_t, 4> input{0x10U, 0x20U, 0x30U, 0x40U};
    laboratory::ByteMutationPlan plan;
    plan.seed = core::RootSeed{1U};
    plan.protected_prefix = 3U;
    plan.operations.push_back(laboratory::ByteMutationOperation{
        laboratory::ByteMutationKind::bit_flip, 3U, 1U, 0U, 1U});
    const laboratory::ByteMutationResult first = laboratory::mutate_encoded_bytes(input, plan);
    const laboratory::ByteMutationResult second = laboratory::mutate_encoded_bytes(input, plan);
    expect(first.ok() && second.ok(), "deterministic encoded-byte mutation succeeds");
    expect_equal(first.bytes, std::vector<std::uint8_t>({0x10U, 0x20U, 0x30U, 0x00U}),
        "bit-flip mutation has an exact known-answer vector");
    expect_equal(first.bytes, second.bytes, "same mutation seed/parameters produce identical bytes");
    expect_equal(first.mutated_identity, second.mutated_identity, "same mutation address produces identical mutated identity");
    expect(first.canonical_recipe.find("\"protected_prefix\":3") != std::string::npos,
        "mutation provenance records the protected header prefix");

    plan.operations[0].offset = 2U;
    const laboratory::ByteMutationResult protected_result = laboratory::mutate_encoded_bytes(input, plan);
    expect(!protected_result.ok(), "mutation may not cross the explicitly protected header prefix");
}

void test_raw_binary_interpretation() {
    using namespace faultmine;
    const std::array<std::uint8_t, 6> rgb{10U, 20U, 30U, 40U, 50U, 60U};
    laboratory::RawBinarySpec spec;
    spec.width = 2U;
    spec.height = 1U;
    spec.format = laboratory::RawFormat::rgb8;
    const auto direct = laboratory::interpret_binary_as_image(rgb, spec);
    expect(direct.ok(), "bounded RGB raw interpretation succeeds");
    if (direct.ok()) {
        expect_equal(direct.image->bytes,
            std::vector<std::uint8_t>({10U, 20U, 30U, 255U, 40U, 50U, 60U, 255U}),
            "raw RGB interpretation has exact channel semantics");
    }

    const std::array<std::uint8_t, 2> short_input{7U, 9U};
    spec.width = 2U;
    spec.height = 1U;
    spec.format = laboratory::RawFormat::rgba8;
    spec.boundary = laboratory::RawBoundaryPolicy::drop;
    expect(!laboratory::interpret_binary_as_image(short_input, spec).ok(),
        "drop policy rejects an incomplete explicit image instead of truncating silently");
    spec.boundary = laboratory::RawBoundaryPolicy::fill;
    spec.fill_byte = 0xAAU;
    const auto filled = laboratory::interpret_binary_as_image(short_input, spec);
    expect(filled.ok(), "fill policy safely materializes missing bytes");
    if (filled.ok()) {
        expect_equal(filled.image->bytes,
            std::vector<std::uint8_t>({7U, 9U, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU, 0xAAU}),
            "fill policy is exact and deterministic");
    }
    spec.boundary = laboratory::RawBoundaryPolicy::wrap;
    const auto wrapped = laboratory::interpret_binary_as_image(short_input, spec);
    expect(wrapped.ok(), "wrap policy safely maps logical bytes modulo the input extent");
    if (wrapped.ok()) {
        expect_equal(wrapped.image->bytes,
            std::vector<std::uint8_t>({7U, 9U, 7U, 9U, 7U, 9U, 7U, 9U}),
            "wrap policy has exact deterministic mapping");
    }

    spec.width = std::numeric_limits<std::uint32_t>::max();
    spec.height = std::numeric_limits<std::uint32_t>::max();
    spec.stride = std::numeric_limits<std::uint64_t>::max();
    expect(!laboratory::interpret_binary_as_image(short_input, spec).ok(),
        "hostile raw dimensions/stride fail before unsafe allocation or indexing");
}

void test_ipc_validation_helpers() {
    using namespace faultmine;
    namespace protocol = laboratory::protocol;
    protocol::RequestRecord request;
    std::string error;
    const std::array<std::uint8_t, 3> malformed{'b', 'a', 'd'};
    expect(!protocol::decode_request(malformed, request, error), "malformed IPC request header is rejected");

    const std::array<std::uint8_t, 2> payload{1U, 2U};
    std::vector<std::uint8_t> encoded = protocol::encode_request(payload, laboratory::WorkerSyntheticMode::success);
    encoded[20] = 3U; // payload length low byte claims three rather than two bytes
    error.clear();
    expect(!protocol::decode_request(encoded, request, error), "IPC request length mismatch is rejected before allocation/use");

    protocol::ResponseRecord response;
    expect(!protocol::decode_response(malformed, response, error), "malformed IPC response header is rejected");
}

void test_worker_containment_and_recovery() {
    using namespace faultmine;
    expect(std::filesystem::exists(worker_path()), "test can locate the dedicated worker executable");

    const laboratory::WorkerResult success = synthetic(laboratory::WorkerSyntheticMode::success);
    expect(success.ok(), "synthetic worker success returns a validated materialized image");
    if (success.ok()) {
        expect_equal(success.materialized->image.width, std::uint32_t{2U}, "synthetic success width crosses IPC exactly");
        expect_equal(success.materialized->image.height, std::uint32_t{2U}, "synthetic success height crosses IPC exactly");
        expect(success.provenance.materialized_source_identity.has_value(), "worker pixels are hashed before admission");
        expect_equal(*success.provenance.materialized_source_identity,
            core::source_identity_hex(success.materialized->image),
            "materialized identity binds exactly to validated returned pixels");
    }

    const laboratory::WorkerResult failure = synthetic(laboratory::WorkerSyntheticMode::decode_failure);
    expect(!failure.ok() && failure.provenance.outcome == laboratory::Outcome::decode_failure,
        "structured decoder failure is an experiment outcome, not a host crash");

    const laboratory::WorkerResult crash = synthetic(laboratory::WorkerSyntheticMode::crash);
    expect(!crash.ok() && crash.provenance.outcome == laboratory::Outcome::worker_crash,
        "worker process crash is contained and classified");

    const laboratory::WorkerResult timeout = synthetic(laboratory::WorkerSyntheticMode::hang, 80U);
    expect(!timeout.ok() && timeout.provenance.outcome == laboratory::Outcome::timeout,
        "hung worker is terminated at its explicit deadline");

    const laboratory::WorkerResult cancelled = synthetic(
        laboratory::WorkerSyntheticMode::hang,
        1000U,
        [] { return true; });
    expect(!cancelled.ok() && cancelled.provenance.outcome == laboratory::Outcome::cancelled,
        "host cancellation terminates the job and returns a structured outcome");

    for (const laboratory::WorkerSyntheticMode mode : {
            laboratory::WorkerSyntheticMode::oversized_payload,
            laboratory::WorkerSyntheticMode::malformed_response,
            laboratory::WorkerSyntheticMode::invalid_dimensions,
            laboratory::WorkerSyntheticMode::invalid_stride,
            laboratory::WorkerSyntheticMode::invalid_byte_count}) {
        const laboratory::WorkerResult rejected = synthetic(mode);
        expect(!rejected.ok() && rejected.provenance.outcome == laboratory::Outcome::rejected_response,
            "oversized/malformed/inconsistent worker response is rejected by the host boundary");
    }

    const laboratory::WorkerResult recovered = synthetic(laboratory::WorkerSyntheticMode::success);
    expect(recovered.ok(), "fresh worker invocation recovers immediately after crash/hang/rejected-response tests");
}

void test_actual_worker_decode_and_freeze() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_directory("actual-decode");
    auto created = core::make_rgba8_image(2U, 1U);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    created.image->bytes = {5U, 6U, 7U, 255U, 8U, 9U, 10U, 255U};
    const std::filesystem::path source_png = directory / "valid.png";
    const auto save_error = io::save_wic_png(*created.image, source_png);
    expect(!save_error.has_value(), "test fixture PNG is encoded through normal WIC path");
    if (save_error.has_value()) return;
    const std::vector<std::uint8_t> encoded = read_binary(source_png);

    laboratory::WorkerOptions options;
    options.executable = worker_path();
    options.timeout_ms = 2000U;
    const laboratory::WorkerResult result = laboratory::run_decoder_worker(
        encoded,
        core::sha256_hex(encoded),
        "{\"version\":1,\"operations\":[]}",
        core::RootSeed{42U},
        options);
    expect(result.ok(), "actual WIC decode executes successfully inside the dedicated worker process");
    if (!result.ok()) return;
    expect_equal(result.materialized->image, *created.image,
        "actual worker normalizes valid encoded input to exact canonical pixels");
    expect(result.provenance.decoder_metadata.find("WIC") != std::string::npos,
        "external decoder provenance names the worker decoder boundary");

    const std::string bundle = laboratory::serialize_materialized_source(*result.materialized);
    const laboratory::MaterializedParseResult reparsed = laboratory::parse_materialized_source(bundle);
    expect(reparsed.ok(), "frozen materialized source strictly round-trips without invoking a decoder");
    if (reparsed.ok()) expect_equal(reparsed.source->image, result.materialized->image, "frozen bundle preserves normalized pixels exactly");
}

void test_project_and_export_provenance() {
    using namespace faultmine;
    const laboratory::WorkerResult worker = synthetic(laboratory::WorkerSyntheticMode::success);
    if (!worker.ok()) {
        expect(false, "project/export provenance setup requires synthetic worker success");
        return;
    }
    const std::filesystem::path directory = fresh_directory("persistence");
    const std::filesystem::path source_path = directory / "materialized.png";
    expect(!io::save_wic_png(worker.materialized->image, source_path).has_value(),
        "materialized source PNG can be written for ordinary project relinking");

    app::SessionModel session;
    std::string error;
    expect(session.set_materialized_laboratory_source(*worker.materialized, source_path, &error),
        "normal deterministic session accepts a validated materialized laboratory source");
    const auto document = session.make_project_document(&error);
    expect(document.has_value() && document->source.laboratory.has_value(),
        "project document embeds frozen normalized pixels plus decoder-dependent provenance");
    if (!document.has_value()) return;

    const std::string project_text = app::serialize_project_canonical(*document);
    expect(!project_text.empty(), "provenance-bearing project serializes canonically");
    const auto parsed = app::parse_project(project_text, session.registry().schema_registry());
    expect(parsed.ok() && parsed.project->source.laboratory.has_value(),
        "project parser restores frozen laboratory source/provenance");
    if (parsed.ok() && parsed.project->source.laboratory.has_value()) {
        app::SessionModel restored;
        expect(restored.load_project_state(
            *parsed.project,
            parsed.project->source.laboratory->image,
            directory / "missing-original-malformed-input.jpg",
            &error),
            "project reload can use embedded frozen pixels without rerunning the external decoder");
        expect(restored.laboratory_provenance().has_value(),
            "restored deterministic session retains external-decoder provenance distinction");
    }

    exporting::StillExportRequest request;
    request.destination = directory / "export.png";
    request.write_manifest = true;
    const exporting::ExportResult exported = exporting::export_still(session, request);
    expect(exported.ok() && exported.manifest.has_value() && exported.manifest->laboratory.has_value(),
        "canonical downstream export carries source laboratory provenance");
    if (exported.manifest.has_value()) {
        expect_equal(exported.manifest->laboratory->materialized_source_identity,
            std::optional<std::string>{session.source_identity()},
            "export provenance binds external-decoder derivation to the frozen canonical source identity");
        const std::string manifest_text = exporting::serialize_export_manifest(*exported.manifest);
        const auto manifest = exporting::parse_export_manifest(manifest_text, session.registry().schema_registry());
        expect(manifest.ok() && manifest.manifest->laboratory.has_value(),
            "export manifest laboratory extension strictly round-trips");
    }
    if (exported.manifest_path.has_value()) {
        const std::string persisted = read_text(*exported.manifest_path);
        const auto manifest = exporting::parse_export_manifest(persisted, session.registry().schema_registry());
        expect(manifest.ok() && manifest.manifest->laboratory.has_value(),
            "persisted FM-012 sidecar is atomically rewritten with laboratory provenance");
    }
}

}  // namespace

int main() {
    try {
        test_deterministic_encoded_mutation();
        test_raw_binary_interpretation();
        test_ipc_validation_helpers();
        test_worker_containment_and_recovery();
        test_actual_worker_decode_and_freeze();
        test_project_and_export_provenance();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }
    if (g_failures != 0) {
        std::cerr << g_failures << " laboratory assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE isolated laboratory contracts passed.\n";
    return 0;
}
