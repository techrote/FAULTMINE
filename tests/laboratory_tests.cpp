#include "faultmine/export.hpp"
#include "faultmine/lab_worker.hpp"
#include "faultmine/laboratory.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/sha256.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

std::filesystem::path fresh_directory(const std::string_view name) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "faultmine-fm013-tests" / std::string{name};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    return root;
}

std::filesystem::path worker_path() {
    std::wstring module(32768U, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length == 0U || length >= module.size()) return {};
    module.resize(length);
    return std::filesystem::path{module}.parent_path() / L"FAULTMINE-lab-worker.exe";
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

faultmine::core::ImageBuffer small_image() {
    auto created = faultmine::core::make_rgba8_image(2U, 2U);
    if (!created.ok()) throw std::runtime_error("could not create test image");
    created.image->bytes = {
        1U, 2U, 3U, 255U,
        4U, 5U, 6U, 255U,
        7U, 8U, 9U, 255U,
        10U, 11U, 12U, 255U};
    return std::move(*created.image);
}

void test_deterministic_encoded_mutation_vector() {
    using namespace faultmine::core;
    std::array<std::uint8_t, 16> input{};
    for (std::size_t index = 0U; index < input.size(); ++index) input[index] = static_cast<std::uint8_t>(index);
    EncodedMutationPlan plan;
    plan.protected_prefix_bytes = 4U;
    plan.random_bit_flips = 5U;
    std::string error;
    const auto first = mutate_encoded_bytes(input, plan, RootSeed{0x0123456789abcdefULL}, &error);
    const auto second = mutate_encoded_bytes(input, plan, RootSeed{0x0123456789abcdefULL}, &error);
    expect(first.has_value() && second.has_value(), "known seeded encoded mutation vector is accepted");
    if (!first.has_value() || !second.has_value()) return;
    const std::vector<std::uint8_t> expected{0U, 1U, 2U, 3U, 4U, 5U, 14U, 7U, 8U, 17U, 2U, 11U, 12U, 13U, 30U, 15U};
    expect_equal(first->bytes, expected, "policy-v1 seeded bit mutation matches frozen exact vector");
    expect_equal(second->bytes, expected, "same input/plan/seed repeats byte-for-byte");
    expect_equal(std::vector<std::uint8_t>(first->bytes.begin(), first->bytes.begin() + 4),
        std::vector<std::uint8_t>({0U, 1U, 2U, 3U}), "protected encoded prefix remains untouched");
    expect_equal(first->original_identity, sha256_hex(input), "original encoded identity hashes exact input bytes");
    expect_equal(first->mutated_identity, sha256_hex(first->bytes), "mutated encoded identity hashes exact mutation result");

    EncodedMutationPlan composed;
    composed.xor_offset = 2U;
    composed.xor_length = 3U;
    composed.xor_mask = 0xffU;
    composed.duplicate_offset = 5U;
    composed.duplicate_length = 2U;
    composed.drop_offset = 8U;
    composed.drop_length = 2U;
    const auto composed_result = mutate_encoded_bytes(input, composed, RootSeed{7U}, &error);
    expect(composed_result.has_value(), "bounded xor/duplicate/drop mutation composes deterministically");
    if (composed_result.has_value()) {
        const std::vector<std::uint8_t> composed_expected{0U,1U,253U,252U,251U,5U,6U,5U,6U,7U,10U,11U,12U,13U,14U,15U};
        expect_equal(composed_result->bytes, composed_expected, "xor/duplicate/drop uses documented policy-v1 order and original-stream addressing");
    }

    EncodedMutationPlan invalid = plan;
    invalid.xor_offset = 2U;
    invalid.xor_length = 1U;
    expect(!mutate_encoded_bytes(input, invalid, RootSeed{1U}, &error).has_value(), "mutation ranges may not cross protected prefix");
}

void test_raw_binary_interpretation() {
    using namespace faultmine::core;
    const std::array<std::uint8_t, 6> input{10U,20U,30U,40U,50U,60U};
    RawBinarySpec spec;
    spec.width = 3U;
    spec.height = 2U;
    spec.bytes_per_pixel = 1U;
    spec.boundary = RawBinaryBoundary::wrap;
    std::string error;
    const auto result = interpret_raw_binary(input, spec, &error);
    expect(result.has_value(), "pure core raw interpretation accepts bounded explicit parameters");
    if (result.has_value()) {
        const std::vector<std::uint8_t> expected{
            10U,10U,10U,255U, 20U,20U,20U,255U, 30U,30U,30U,255U,
            40U,40U,40U,255U, 50U,50U,50U,255U, 60U,60U,60U,255U};
        expect_equal(result->image.bytes, expected, "raw grayscale interpretation is exact and deterministic");
        expect_equal(result->source_identity, source_identity_hex(result->image), "raw interpretation returns canonical normalized pixel identity");
    }
    RawBinarySpec overflow = spec;
    overflow.width = 16384U;
    overflow.height = 16384U;
    overflow.bytes_per_pixel = 4U;
    overflow.offset = std::numeric_limits<std::uint64_t>::max() - 2U;
    overflow.stride = std::numeric_limits<std::uint64_t>::max();
    expect(!interpret_raw_binary(input, overflow, &error).has_value(), "raw interpretation rejects address arithmetic overflow before unsafe access");
}

faultmine::laboratory::WorkerResult run_mode(
    const faultmine::laboratory::WorkerMode mode,
    const std::span<const std::uint8_t> bytes,
    const std::uint32_t timeout_ms = 1000U,
    std::function<bool()> cancel = {}) {
    faultmine::laboratory::WorkerRequest request;
    request.worker_executable = worker_path();
    request.encoded_bytes = bytes;
    request.mode = mode;
    request.timeout_ms = timeout_ms;
    request.should_cancel = std::move(cancel);
    return faultmine::laboratory::run_lab_worker(request);
}

void test_worker_containment_and_recovery() {
    using namespace faultmine;
    const std::array<std::uint8_t, 16> bytes{0U,1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,13U,14U,15U};
    expect(std::filesystem::exists(worker_path()), "laboratory tests can locate sibling isolated worker executable");

    const auto success = run_mode(laboratory::WorkerMode::synthetic_success, bytes);
    expect(success.success(), "synthetic worker success traverses versioned bounded IPC and validates pixels");
    if (success.success()) {
        expect_equal(success.image->width, std::uint32_t{2U}, "synthetic worker success width is validated");
        expect_equal(success.image->height, std::uint32_t{2U}, "synthetic worker success height is validated");
        expect_equal(success.image->bytes, std::vector<std::uint8_t>(bytes.begin(), bytes.end()), "synthetic worker pixel payload is exact");
    }

    expect_equal(run_mode(laboratory::WorkerMode::synthetic_decode_failure, bytes).outcome,
        core::LaboratoryOutcome::decode_failure, "decode failure is structured and distinct from host/worker failure");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_crash, bytes).outcome,
        core::LaboratoryOutcome::worker_crash, "worker nonzero/crash is contained and reported structurally");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_hang, bytes, 100U).outcome,
        core::LaboratoryOutcome::timeout, "hung worker is killed at explicit deadline");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_oversized_metadata, bytes).outcome,
        core::LaboratoryOutcome::rejected_response, "oversized response metadata is rejected before allocation/use");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_malformed_header, bytes).outcome,
        core::LaboratoryOutcome::rejected_response, "malformed IPC header is rejected");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_invalid_dimensions, bytes).outcome,
        core::LaboratoryOutcome::rejected_response, "invalid worker dimensions are rejected");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_invalid_stride, bytes).outcome,
        core::LaboratoryOutcome::rejected_response, "invalid worker stride is rejected");
    expect_equal(run_mode(laboratory::WorkerMode::synthetic_invalid_byte_count, bytes).outcome,
        core::LaboratoryOutcome::rejected_response, "invalid worker byte count is rejected");

    int cancellation_checks = 0;
    const auto cancelled = run_mode(laboratory::WorkerMode::synthetic_hang, bytes, 2000U, [&] {
        ++cancellation_checks;
        return cancellation_checks >= 2;
    });
    expect_equal(cancelled.outcome, core::LaboratoryOutcome::cancelled, "host cancellation kills contained worker without hanging editor-side caller");

    const auto after_crash = run_mode(laboratory::WorkerMode::synthetic_success, bytes);
    expect(after_crash.success(), "fresh worker launch succeeds after prior crash/timeout/cancellation");
}

void test_real_worker_valid_decode_without_malformed_pixel_assumptions() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_directory("real-worker");
    const std::filesystem::path png = directory / "valid.png";
    const core::ImageBuffer source = small_image();
    expect(!io::save_wic_png(source, png).has_value(), "test creates a normal known-valid PNG fixture");
    const std::vector<std::uint8_t> encoded = read_file(png);
    const auto result = run_mode(laboratory::WorkerMode::decode_wic, encoded);
    expect(result.success(), "real WIC decode executes only inside worker for valid control fixture");
    if (result.success()) expect_equal(*result.image, source, "worker normalizes known-valid PNG exactly");
}

faultmine::core::LaboratoryProvenance make_provenance(
    const faultmine::core::ImageBuffer& image,
    const std::span<const std::uint8_t> encoded) {
    using namespace faultmine;
    core::LaboratoryProvenance provenance;
    provenance.original_encoded_identity = core::sha256_hex(encoded);
    std::vector<std::uint8_t> mutated(encoded.begin(), encoded.end());
    mutated[0] ^= 1U;
    provenance.mutated_encoded_identity = core::sha256_hex(mutated);
    provenance.mutation_seed = core::RootSeed{0x1020304050607080ULL}.to_string();
    core::EncodedMutationPlan plan;
    plan.random_bit_flips = 1U;
    provenance.mutation_parameters = core::encoded_mutation_plan_text(plan);
    provenance.worker_protocol_version = laboratory::kLabWorkerProtocolVersion;
    provenance.worker_application_version = std::string{exporting::kApplicationVersion};
    provenance.decoder_identifier = "synthetic-decoder-for-freeze-test";
    provenance.os_build = "Windows test build metadata";
    provenance.outcome = core::LaboratoryOutcome::success;
    provenance.diagnostic = "validated test materialization";
    provenance.materialized_source_identity = core::source_identity_hex(image);
    return provenance;
}

void test_freeze_project_reload_and_export_provenance() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_directory("freeze");
    const core::ImageBuffer image = small_image();
    const std::array<std::uint8_t, 8> encoded{1U,3U,5U,7U,9U,11U,13U,15U};
    core::MaterializedLaboratorySource materialized{image, make_provenance(image, encoded)};
    std::string error;
    expect(core::validate_materialized_laboratory_source(materialized, &error), "successful external result validates before becoming a frozen source");

    app::SessionModel session;
    const std::filesystem::path frozen_png = directory / "frozen.png";
    expect(!io::save_wic_png(image, frozen_png).has_value(), "frozen normalized source asset is materialized as ordinary PNG");
    expect(session.set_materialized_laboratory_source(materialized, frozen_png, &error), "session adopts materialized source with explicit laboratory provenance");
    expect(session.laboratory_provenance().has_value(), "session exposes frozen provenance distinctly from ordinary source state");

    const auto project = session.make_project_document(&error);
    expect(project.has_value(), "materialized laboratory session creates saveable project v3");
    if (!project.has_value()) return;
    expect_equal(project->project_version, app::kProjectSchemaVersion, "laboratory project uses current versioned project schema");
    expect(project->laboratory_source.has_value(), "project embeds frozen normalized pixels and decoder provenance");
    const std::string text = app::serialize_project_canonical(*project);
    const auto parsed = app::parse_project(text, session.registry().schema_registry());
    expect(parsed.ok(), "project v3 strict parser round-trips frozen laboratory source");
    if (!parsed.ok()) return;
    expect(parsed.project->laboratory_source.has_value(), "reloaded project still carries embedded materialized source");
    if (!parsed.project->laboratory_source.has_value()) return;
    expect_equal(parsed.project->laboratory_source->image, image, "project reopen recovers exact frozen pixels without invoking worker/decoder");
    expect_equal(parsed.project->laboratory_source->provenance, materialized.provenance, "project reopen preserves decoder-dependent provenance exactly");

    app::SessionModel reloaded;
    expect(reloaded.load_project_state(*parsed.project, parsed.project->laboratory_source->image, frozen_png, &error),
        "session reload consumes embedded frozen pixels directly with no external decoder rerun");
    expect_equal(reloaded.source_identity(), session.source_identity(), "frozen reload keeps canonical materialized source identity");
    expect_equal(reloaded.laboratory_provenance(), session.laboratory_provenance(), "frozen reload keeps laboratory provenance");

    exporting::StillExportRequest request;
    request.destination = directory / "export.png";
    request.write_manifest = true;
    const auto exported = exporting::export_still(reloaded, request);
    expect(exported.ok(), "canonical downstream export works from frozen source");
    expect(exported.manifest.has_value() && exported.manifest->laboratory_provenance.has_value(),
        "export manifest v2 preserves laboratory source provenance");
    if (exported.manifest.has_value() && exported.manifest->laboratory_provenance.has_value()) {
        expect_equal(exported.manifest->laboratory_provenance, reloaded.laboratory_provenance(), "export provenance is exact session frozen provenance");
        const std::string manifest_text = exporting::serialize_export_manifest(*exported.manifest);
        const auto manifest = exporting::parse_export_manifest(manifest_text, reloaded.registry().schema_registry());
        expect(manifest.ok(), "export manifest v2 strict parser accepts laboratory provenance");
        if (manifest.ok()) expect_equal(*manifest.manifest, *exported.manifest, "laboratory export manifest round-trips exactly");
    }
}

}  // namespace

int main() {
    test_deterministic_encoded_mutation_vector();
    test_raw_binary_interpretation();
    test_worker_containment_and_recovery();
    test_real_worker_valid_decode_without_malformed_pixel_assumptions();
    test_freeze_project_reload_and_export_provenance();
    if (g_failures != 0) {
        std::cerr << g_failures << " laboratory contract test(s) failed\n";
        return 1;
    }
    std::cout << "FM-013 laboratory contracts passed\n";
    return 0;
}
