#include "faultmine/batch.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

faultmine::core::ImageBuffer make_pattern(const std::uint32_t width = 19U, const std::uint32_t height = 13U) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    auto image = std::move(*created.image);
    for (std::uint32_t y = 0U; y < height; ++y) {
        for (std::uint32_t x = 0U; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4U;
            image.bytes[offset + 0U] = static_cast<std::uint8_t>((x * 17U + y * 29U + 3U) & 0xffU);
            image.bytes[offset + 1U] = static_cast<std::uint8_t>((x * 47U + y * 7U + 19U) & 0xffU);
            image.bytes[offset + 2U] = static_cast<std::uint8_t>((x * 5U + y * 61U + 101U) & 0xffU);
            image.bytes[offset + 3U] = 255U;
        }
    }
    return image;
}

faultmine::core::Genome make_parent(const bool enabled = true) {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x123456789abcdef0ULL};
    OperatorInstance row;
    row.instance_id = InstanceId{0x1111111111111111ULL, 0x2222222222222222ULL};
    row.type_id = kFaultRowOffset;
    row.type_version = 1U;
    row.enabled = enabled;
    row.parameters.emplace("amount", ParameterValue{std::int64_t{3}});
    row.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});
    genome.operators.push_back(std::move(row));
    return genome;
}

faultmine::core::BatchRequest make_request(const std::uint64_t begin = 0U, const std::uint64_t count = 16U) {
    faultmine::core::BatchRequest request;
    request.mutation_seed = faultmine::core::RootSeed{0x0badf00dcafefeedULL};
    request.radius = faultmine::core::MutationRadius::medium;
    request.index_begin = begin;
    request.index_end_exclusive = begin + count;
    request.near_duplicate_threshold = 0U;
    request.select_count = 6U;
    request.worker_count = 1U;
    return request;
}

void test_descriptor_known_answer() {
    using namespace faultmine::core;
    auto created = make_rgba8_image(2U, 2U);
    expect(created.ok(), "2x2 descriptor fixture allocates");
    if (!created.ok()) return;
    ImageBuffer image = std::move(*created.image);
    const std::uint8_t levels[4]{0U, 64U, 128U, 255U};
    for (std::size_t pixel = 0U; pixel < 4U; ++pixel) {
        for (std::size_t channel = 0U; channel < 3U; ++channel) image.bytes[pixel * 4U + channel] = levels[pixel];
        image.bytes[pixel * 4U + 3U] = 255U;
    }
    const DescriptorResult descriptor = compute_visual_descriptor(image);
    expect(descriptor.ok(), "descriptor fixture computes");
    if (!descriptor.ok()) return;
    const std::vector<std::uint32_t> expected{
        16384U, 16384U, 16384U, 16384U,
        0U, 0U, 16448U, 0U,
        0U, 0U, 0U, 0U,
        32896U, 0U, 65535U, 0U,
        0U, 0U, 0U, 0U,
        24672U, 41120U, 32768U, 0U};
    expect_equal(descriptor.descriptor->version, kVisualDescriptorVersion, "descriptor version is explicit");
    expect_equal(descriptor.descriptor->values, expected, "descriptor v1 exact known-answer vector is frozen");
    expect_equal(
        visual_descriptor_distance(*descriptor.descriptor, *descriptor.descriptor),
        std::optional<std::uint64_t>{0U},
        "descriptor distance is exact and zero for identity");
}

void test_serial_parallel_and_manifest_reproducibility() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_pattern();
    const std::string source_identity = source_identity_hex(source);
    const Genome parent = make_parent();

    BatchRequest serial_request = make_request();
    serial_request.worker_count = 1U;
    const BatchRunResult serial = run_batch(source, source_identity, parent, registry, serial_request);
    expect(serial.ok() && serial.manifest.complete, "serial batch completes");
    expect_equal(serial.stats.completed, std::size_t{16U}, "serial batch completes requested indexed range");

    BatchRequest parallel_request = serial_request;
    parallel_request.worker_count = 4U;
    const BatchRunResult parallel = run_batch(source, source_identity, parent, registry, parallel_request);
    expect(parallel.ok() && parallel.manifest.complete, "parallel batch completes");
    expect_equal(
        serialize_batch_manifest(parallel.manifest),
        serialize_batch_manifest(serial.manifest),
        "serial and parallel execution produce byte-identical semantic batch manifests");

    const std::string serialized = serialize_batch_manifest(serial.manifest);
    const auto parsed = parse_batch_manifest(serialized, registry.schema_registry());
    expect(parsed.ok(), "batch manifest strict parser accepts canonical output");
    if (parsed.ok()) {
        expect_equal(serialize_batch_manifest(*parsed.manifest), serialized, "batch manifest canonical round-trip is exact");
    }
    std::string with_unknown = serialized;
    const std::size_t final_object = with_unknown.rfind('}');
    if (final_object != std::string::npos) with_unknown.insert(final_object, ",\"unexpected\":1");
    expect(!parse_batch_manifest(with_unknown, registry.schema_registry()).ok(), "unknown manifest fields are rejected");
}

void test_resume_and_stale_rejection() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_pattern();
    const std::string source_identity = source_identity_hex(source);
    const Genome parent = make_parent();
    BatchRequest request = make_request(40U, 12U);
    const BatchRunResult baseline = run_batch(source, source_identity, parent, registry, request);
    expect(baseline.manifest.complete, "resume baseline completes");

    BatchRequest resumed_request = request;
    resumed_request.worker_count = 3U;
    const BatchRunResult resumed = run_batch(
        source, source_identity, parent, registry, resumed_request, baseline.manifest.candidates);
    expect(resumed.manifest.complete, "fully resumed batch completes");
    expect_equal(resumed.stats.reused, std::size_t{12U}, "all verified cached candidates are reused");
    expect_equal(
        serialize_batch_manifest(resumed.manifest),
        serialize_batch_manifest(baseline.manifest),
        "resumed final manifest equals uninterrupted final manifest");

    std::vector<BatchCandidate> stale = baseline.manifest.candidates;
    expect(!stale.empty() && stale.front().rendered_image.has_value(), "stale fixture has rendered cache");
    if (!stale.empty() && stale.front().rendered_image.has_value()) {
        stale.front().rendered_image->bytes[0] ^= 1U;
        const BatchRunResult repaired = run_batch(source, source_identity, parent, registry, resumed_request, stale);
        expect(repaired.manifest.complete, "batch regenerates stale cached output");
        expect_equal(repaired.stats.reused, std::size_t{11U}, "stale pixel identity is rejected rather than reused");
        expect_equal(
            serialize_batch_manifest(repaired.manifest),
            serialize_batch_manifest(baseline.manifest),
            "regeneration after stale rejection restores exact result set");
    }
}

void test_exact_dedupe_distinguishes_genome_and_pixels() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_pattern();
    const std::string source_identity = source_identity_hex(source);
    const Genome disabled_parent = make_parent(false);
    BatchRequest request = make_request(0U, 32U);
    request.radius = MutationRadius::low;
    request.near_duplicate_threshold = 0U;
    request.select_count = 32U;
    const BatchRunResult run = run_batch(source, source_identity, disabled_parent, registry, request);
    expect(run.manifest.complete, "disabled-operator dedupe fixture completes");
    expect(run.stats.unique_genomes > 1U, "mutation can produce multiple distinct disabled genomes");
    expect_equal(run.stats.unique_pixels, std::size_t{1U}, "different disabled genomes with identical pixels collapse only at pixel dedupe layer");
    if (run.stats.unique_genomes > 1U) {
        bool found_distinct_same_pixels = false;
        for (std::size_t left = 0U; left < run.manifest.candidates.size(); ++left) {
            for (std::size_t right = left + 1U; right < run.manifest.candidates.size(); ++right) {
                const auto& a = run.manifest.candidates[left];
                const auto& b = run.manifest.candidates[right];
                if (a.genome_identity != b.genome_identity && a.pixel_identity == b.pixel_identity) {
                    found_distinct_same_pixels = true;
                }
            }
        }
        expect(found_distinct_same_pixels, "dedupe records distinguish different-genome/same-pixel convergence");
    }
}

void test_near_duplicate_threshold_boundary() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_pattern();
    const std::string source_identity = source_identity_hex(source);
    const Genome parent = make_parent();

    std::optional<std::uint64_t> start;
    std::uint64_t distance{};
    for (std::uint64_t index = 0U; index < 64U && !start.has_value(); ++index) {
        BatchRequest pair = make_request(index, 2U);
        pair.near_duplicate_threshold = 0U;
        const BatchRunResult run = run_batch(source, source_identity, parent, registry, pair);
        if (!run.manifest.complete || run.manifest.candidates.size() != 2U ||
            run.manifest.candidates[0].pixel_identity == run.manifest.candidates[1].pixel_identity) continue;
        const auto measured = visual_descriptor_distance(
            run.manifest.candidates[0].descriptor, run.manifest.candidates[1].descriptor);
        if (measured.has_value() && *measured > 0U) { start = index; distance = *measured; }
    }
    expect(start.has_value(), "found deterministic two-candidate threshold fixture");
    if (!start.has_value()) return;

    BatchRequest below = make_request(*start, 2U);
    below.near_duplicate_threshold = distance - 1U;
    below.select_count = 2U;
    const BatchRunResult below_run = run_batch(source, source_identity, parent, registry, below);
    BatchRequest inclusive = below;
    inclusive.near_duplicate_threshold = distance;
    const BatchRunResult inclusive_run = run_batch(source, source_identity, parent, registry, inclusive);
    expect_equal(below_run.stats.selected, std::size_t{2U}, "distance threshold minus one keeps both visual representatives");
    expect_equal(inclusive_run.stats.selected, std::size_t{1U}, "inclusive exact distance threshold groups the near duplicate");
}

void test_gui_canonical_agreement_and_project_import() {
    using namespace faultmine;
    const core::FaultRegistry registry = core::make_default_fault_registry();
    const core::ImageBuffer source = make_pattern();
    const std::string source_identity = core::source_identity_hex(source);
    const core::Genome parent = make_parent();
    core::BatchRequest request = make_request(100U, 8U);
    request.select_count = 3U;
    const core::BatchRunResult run = core::run_batch(source, source_identity, parent, registry, request);
    expect(run.manifest.complete, "GUI agreement batch completes");
    if (!run.manifest.complete || run.manifest.candidates.empty()) return;

    app::SessionModel session;
    std::string error;
    expect(session.set_source(source, source_identity, L"batch-fixture.png", &error), "session accepts batch source");
    expect(session.promote_exploration_genome(parent, &error), "session adopts exact batch parent genome");
    const core::BatchCandidate& candidate = run.manifest.candidates.front();
    expect(session.promote_mutation_specimen(candidate.genome, candidate.provenance, true, &error), "GUI session promotes exact mined specimen/provenance");
    const auto gui_render = session.render_full_at_frame(request.frame_index, &error);
    expect(gui_render.has_value() && candidate.rendered_image.has_value(), "GUI and batch renders exist");
    if (gui_render.has_value() && candidate.rendered_image.has_value()) {
        expect_equal(core::source_identity_hex(*gui_render), candidate.pixel_identity, "headless canonical pixel hash agrees with GUI/session canonical hash");
        expect_equal(*gui_render, *candidate.rendered_image, "headless and GUI canonical bytes agree exactly");
    }

    for (const auto& mined : run.manifest.candidates) {
        if (mined.selected) expect(session.retain_mutation_specimen(mined.genome, mined.provenance, true, &error), "selected mined specimen is retainable in GUI lineage");
    }
    const auto project = session.make_project_document(&error);
    expect(project.has_value(), "mined GUI lineage serializes as ordinary project v2");
    if (project.has_value()) {
        const std::string project_text = app::serialize_project_canonical(*project);
        const auto parsed = app::parse_project(project_text, registry.schema_registry());
        expect(parsed.ok(), "batch-derived project reopens through normal GUI project parser");
        if (parsed.ok()) {
            bool found_mutation = false;
            for (const auto& record : parsed.project->lineage.specimens) {
                if (record.derivation.kind == app::DerivationKind::mutation) found_mutation = true;
            }
            expect(found_mutation, "reopened batch project preserves mutation provenance");
            expect_equal(parsed.project->source.source_identity, source_identity, "reopened batch project preserves source provenance");
        }
    }
}

void test_cancellation_is_coherent() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_pattern();
    BatchRequest request = make_request(0U, 10U);
    request.worker_count = 1U;
    std::size_t checks{};
    const BatchRunResult run = run_batch(
        source, source_identity_hex(source), make_parent(), registry, request, {},
        [&checks] { return ++checks > 2U; });
    expect(run.ok(), "cancelled batch is a structured non-engineering-error result");
    expect(run.manifest.cancelled && !run.manifest.complete, "cancelled batch is explicitly partial");
    expect(run.manifest.candidates.size() <= 2U, "cancellation stops before claiming more work");
    const std::string text = serialize_batch_manifest(run.manifest);
    expect(parse_batch_manifest(text, registry.schema_registry()).ok(), "partial cancelled batch manifest remains strict and parseable");
}

}  // namespace

int main() {
    try {
        test_descriptor_known_answer();
        test_serial_parallel_and_manifest_reproducibility();
        test_resume_and_stale_rejection();
        test_exact_dedupe_distinguishes_genome_and_pixels();
        test_near_duplicate_threshold_boundary();
        test_gui_canonical_agreement_and_project_import();
        test_cancellation_is_coherent();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "FAIL: unexpected exception: " << exception.what() << '\n';
    }
    if (g_failures != 0) {
        std::cerr << g_failures << " batch contract assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FM-014 deterministic batch mining contracts passed.\n";
    return 0;
}
