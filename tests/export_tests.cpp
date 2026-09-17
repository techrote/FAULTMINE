#include "faultmine/export.hpp"

#include "faultmine/image.hpp"
#include "faultmine/project.hpp"
#include "faultmine/temporal.hpp"
#include "faultmine/wic_io.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

faultmine::core::ImageBuffer make_source() {
    using namespace faultmine::core;
    auto created = make_rgba8_image(8U, 4U);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    for (std::size_t index = 0U; index < created.image->bytes.size(); ++index) {
        created.image->bytes[index] = static_cast<std::uint8_t>((index * 29U + 17U) & 0xffU);
    }
    return std::move(*created.image);
}

std::filesystem::path fresh_test_directory(const std::string_view name) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "faultmine-fm012-tests";
    root /= std::string{name};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    return root;
}

faultmine::app::SessionModel make_session(const std::filesystem::path& source_path) {
    using namespace faultmine;
    core::ImageBuffer source = make_source();
    const std::string identity = core::source_identity_hex(source);
    app::SessionModel session;
    std::string error;
    if (!session.set_source(std::move(source), identity, source_path, &error)) {
        throw std::runtime_error(error);
    }
    return session;
}

std::string project_snapshot(const faultmine::app::SessionModel& session) {
    std::string error;
    const auto project = session.make_project_document(&error);
    if (!project.has_value()) throw std::runtime_error(error);
    return faultmine::app::serialize_project_canonical(*project);
}

void test_still_is_full_resolution_and_manifest_round_trips() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_test_directory("still");
    app::SessionModel session = make_session(directory / "source.png");
    session.set_proxy_spec(core::ProxySpec{2U, 1U, core::kProxyMethodVersion});
    session.set_proxy_enabled(true);
    std::string error;
    expect(session.ensure_preview(&error), "proxy preview can be prepared before canonical export");
    expect(session.preview_state().is_proxy, "test setup actually uses a proxy preview");

    const std::string before_project = project_snapshot(session);
    const std::string before_genome = session.genome_identity();
    const std::filesystem::path destination = directory / "still.png";
    exporting::StillExportRequest request;
    request.destination = destination;
    request.collision = exporting::CollisionPolicy::fail_if_exists;
    request.write_manifest = true;
    const exporting::ExportResult exported = exporting::export_still(session, request);
    expect(exported.ok(), "canonical still export succeeds");
    expect(exported.manifest.has_value(), "still export returns its provenance manifest");
    expect(exported.manifest_path.has_value(), "still export writes provenance by default when requested");

    const io::WicLoadResult loaded = io::load_wic_image(destination);
    expect(loaded.ok(), "exported still decodes through canonical WIC normalization");
    if (loaded.ok()) {
        expect_equal(loaded.source->image.width, std::uint32_t{8U}, "still export width is full-resolution, not proxy width");
        expect_equal(loaded.source->image.height, std::uint32_t{4U}, "still export height is full-resolution, not proxy height");
        const auto direct = session.render_full_at_frame(session.current_frame(), &error);
        expect(direct.has_value(), "direct canonical render is available for still comparison");
        if (direct.has_value()) expect_equal(loaded.source->image, *direct, "exported still pixels exactly match the canonical full-resolution render");
    }

    if (exported.manifest.has_value()) {
        const exporting::ExportManifest& manifest = *exported.manifest;
        expect(manifest.canonical_full_resolution, "still manifest explicitly identifies canonical full-resolution output");
        expect_equal(manifest.source_identity, session.source_identity(), "manifest source identity matches active normalized source");
        expect_equal(manifest.genome_identity, session.genome_identity(), "manifest genome identity matches active canonical genome");
        expect_equal(manifest.root_seed, session.genome().root_seed.to_string(), "manifest root seed matches canonical genome");
        expect_equal(manifest.output_width, std::uint32_t{8U}, "manifest records exact output width");
        expect_equal(manifest.output_height, std::uint32_t{4U}, "manifest records exact output height");
        expect_equal(manifest.operators.size(), session.genome().operators.size(), "manifest summarizes complete operator stack");
        expect(manifest.output_image_identity.size() == 64U, "manifest records canonical output image identity");

        const std::string text = exporting::serialize_export_manifest(manifest);
        const exporting::ManifestParseResult parsed = exporting::parse_export_manifest(text, session.registry().schema_registry());
        expect(parsed.ok(), "versioned manifest strictly parses after serialization");
        if (parsed.ok()) expect_equal(*parsed.manifest, manifest, "manifest exact round-trip preserves all semantic/provenance fields");

        std::string missing = text;
        const std::string needle =
            "\"application_version\":\"" + std::string{exporting::kApplicationVersion} + "\",";
        const std::size_t found = missing.find(needle);
        expect(found != std::string::npos, "missing-field fixture resolves the current application version field");
        if (found != std::string::npos) missing.erase(found, needle.size());
        expect(!exporting::parse_export_manifest(missing, session.registry().schema_registry()).ok(), "manifest parser rejects a missing required field");

        std::string unknown = text;
        unknown.insert(1U, "\"unknown_field\":1,");
        expect(!exporting::parse_export_manifest(unknown, session.registry().schema_registry()).ok(), "manifest parser rejects unknown schema-owned fields");
    }

    expect_equal(project_snapshot(session), before_project, "still export does not mutate project state");
    expect_equal(session.genome_identity(), before_genome, "still export does not mutate canonical genome");
}

void test_collision_overwrite_and_invalid_destination() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_test_directory("collision");
    app::SessionModel session = make_session(directory / "source.png");
    const std::filesystem::path destination = directory / "collision.png";

    exporting::StillExportRequest first;
    first.destination = destination;
    first.collision = exporting::CollisionPolicy::fail_if_exists;
    const exporting::ExportResult initial = exporting::export_still(session, first);
    expect(initial.ok(), "initial no-overwrite export succeeds on unused path");
    const io::WicLoadResult initial_pixels = io::load_wic_image(destination);
    expect(initial_pixels.ok(), "initial exported pixels can be reloaded");

    const exporting::ExportResult collision = exporting::export_still(session, first);
    expect(!collision.ok(), "no-overwrite policy deliberately rejects an existing destination");
    const io::WicLoadResult after_collision = io::load_wic_image(destination);
    expect(after_collision.ok(), "collision rejection leaves existing user output readable");
    if (initial_pixels.ok() && after_collision.ok()) {
        expect_equal(after_collision.source->image, initial_pixels.source->image, "collision rejection does not delete or replace existing output");
    }

    exporting::StillExportRequest overwrite = first;
    overwrite.collision = exporting::CollisionPolicy::overwrite;
    const exporting::ExportResult replaced = exporting::export_still(session, overwrite);
    expect(replaced.ok(), "explicit overwrite policy atomically replaces existing export and manifest");

    exporting::StillExportRequest invalid = first;
    invalid.destination = directory / "missing-parent" / "bad.png";
    const exporting::ExportResult bad = exporting::export_still(session, invalid);
    expect(!bad.ok(), "invalid destination directory returns structured export failure");
    expect(!std::filesystem::exists(invalid.destination), "failed destination validation never leaves a final-looking PNG");
}

void test_contact_sheet_order_and_mapping() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_test_directory("contact");
    app::SessionModel session = make_session(directory / "source.png");

    core::Genome first = session.genome();
    core::Genome second = session.genome();
    second.root_seed.value ^= 0x8877665544332211ULL;
    const std::string first_identity = core::genome_identity_hex(first);
    const std::string second_identity = core::genome_identity_hex(second);

    exporting::ContactSheetRequest request;
    request.destination = directory / "contact.png";
    request.columns = 2U;
    request.cell_width = 4U;
    request.cell_height = 4U;
    request.specimens.push_back(exporting::ContactSheetSpecimen{17U, second, second_identity});
    request.specimens.push_back(exporting::ContactSheetSpecimen{3U, first, first_identity});

    std::vector<std::size_t> progress_values;
    const exporting::ExportResult exported = exporting::export_contact_sheet(
        session,
        request,
        {},
        [&](const std::size_t completed, const std::size_t, const std::filesystem::path&) {
            progress_values.push_back(completed);
        });
    expect(exported.ok(), "deterministic contact-sheet export succeeds");
    expect_equal(progress_values, std::vector<std::size_t>({1U, 2U}), "contact-sheet progress follows explicit specimen order");
    expect(exported.manifest.has_value(), "contact sheet returns companion mapping manifest");
    if (exported.manifest.has_value()) {
        const exporting::ExportManifest& manifest = *exported.manifest;
        expect(!manifest.canonical_full_resolution, "contact sheet is explicitly marked as presentation output rather than canonical specimen pixels");
        expect_equal(manifest.contact_cells.size(), std::size_t{2U}, "contact manifest maps every ordered cell");
        if (manifest.contact_cells.size() == 2U) {
            expect_equal(manifest.contact_cells[0].cell_index, std::uint64_t{0U}, "first contact cell keeps explicit ordinal zero");
            expect_equal(manifest.contact_cells[0].order_key, std::uint64_t{17U}, "first contact cell preserves caller tray/order key");
            expect_equal(manifest.contact_cells[0].genome_identity, second_identity, "contact ordering is caller order, not genome sort/render completion order");
            expect_equal(manifest.contact_cells[1].order_key, std::uint64_t{3U}, "second contact cell preserves independent caller order key");
            expect_equal(manifest.contact_cells[1].genome_identity, first_identity, "second contact mapping preserves exact specimen identity");
        }
        const std::string text = exporting::serialize_export_manifest(manifest);
        const auto parsed = exporting::parse_export_manifest(text, session.registry().schema_registry());
        expect(parsed.ok(), "contact manifest strict round-trip parses");
        if (parsed.ok()) expect_equal(*parsed.manifest, manifest, "contact manifest round-trip preserves cell ordering and embedded canonical genomes");
    }

    const io::WicLoadResult loaded = io::load_wic_image(request.destination);
    expect(loaded.ok(), "contact sheet is a valid PNG presentation artefact");
    if (loaded.ok()) {
        expect_equal(loaded.source->image.width, std::uint32_t{8U}, "contact sheet width is deterministic from columns and cell width");
        expect_equal(loaded.source->image.height, std::uint32_t{4U}, "contact sheet height is deterministic from row count and cell height");
    }
}

void add_temporal_feedback(faultmine::app::SessionModel& session) {
    using namespace faultmine;
    const std::size_t index = session.genome().operators.size();
    if (!session.add_operator(core::kFaultFeedbackDisplace, index).ok() ||
        !session.set_parameter_from_text(index, "dx", "1").ok() ||
        !session.set_parameter_from_text(index, "dy", "0").ok() ||
        !session.set_parameter_from_text(index, "amount_256", "256").ok() ||
        !session.set_parameter_from_text(index, "boundary", "wrap").ok()) {
        throw std::runtime_error("could not construct temporal export test genome");
    }
}

void test_sequence_naming_hashes_and_cancellation() {
    using namespace faultmine;
    const std::filesystem::path directory = fresh_test_directory("sequence");
    app::SessionModel session = make_session(directory / "source.png");
    add_temporal_feedback(session);
    const std::string before_project = project_snapshot(session);
    const std::string before_genome = session.genome_identity();
    session.seek_frame(11U);
    const std::uint64_t before_frame = session.current_frame();

    exporting::FrameSequenceRequest request;
    request.directory = directory / "frames";
    request.stem = "seq";
    request.frame_begin = 0U;
    request.frame_end_exclusive = 3U;
    request.minimum_padding = 4U;
    std::vector<std::filesystem::path> progress_paths;
    const exporting::ExportResult exported = exporting::export_frame_sequence(
        session,
        request,
        {},
        [&](const std::size_t, const std::size_t, const std::filesystem::path& path) {
            progress_paths.push_back(path.filename());
        });
    expect(exported.ok() && !exported.cancelled, "canonical temporal sequence export completes explicit [begin,end) range");
    expect_equal(
        progress_paths,
        std::vector<std::filesystem::path>({"seq_0000.png", "seq_0001.png", "seq_0002.png"}),
        "sequence naming is stable, zero-padded and frame ordered");
    expect(exported.manifest.has_value(), "sequence export returns aggregate provenance manifest");
    if (exported.manifest.has_value()) {
        const exporting::ExportManifest& manifest = *exported.manifest;
        expect(manifest.sequence_complete && !manifest.sequence_cancelled, "completed sequence manifest truthfully marks completion");
        expect_equal(manifest.frame_begin, std::optional<std::uint64_t>{0U}, "sequence manifest records inclusive begin frame");
        expect_equal(manifest.frame_end_exclusive, std::optional<std::uint64_t>{3U}, "sequence manifest records exclusive end frame");
        expect_equal(manifest.frames.size(), std::size_t{3U}, "sequence manifest includes every completed frame identity");
        for (std::uint64_t frame = 0U; frame < 3U; ++frame) {
            const std::filesystem::path path = request.directory / exporting::sequence_frame_filename("seq", frame, 3U, 4U);
            const io::WicLoadResult loaded = io::load_wic_image(path);
            std::string error;
            const auto direct = session.render_full_at_frame(frame, &error);
            expect(loaded.ok() && direct.has_value(), "exported and direct temporal frame are both available");
            if (loaded.ok() && direct.has_value()) {
                expect_equal(loaded.source->image, *direct, "sequence frame PNG decodes to exact canonical direct-render pixels");
                const std::size_t index = static_cast<std::size_t>(frame);
                expect_equal(manifest.frames[index].image_identity, core::source_identity_hex(*direct), "per-frame manifest image hash matches direct canonical frame");
                expect_equal(
                    manifest.frames[index].frame_identity,
                    core::temporal_frame_identity_hex(session.source_identity(), session.genome(), frame, *direct),
                    "per-frame manifest temporal identity matches canonical direct-render identity");
            }
        }
        const std::string text = exporting::serialize_export_manifest(manifest);
        const auto parsed = exporting::parse_export_manifest(text, session.registry().schema_registry());
        expect(parsed.ok(), "sequence manifest strict round-trip parses");
        if (parsed.ok()) expect_equal(*parsed.manifest, manifest, "sequence manifest round-trip preserves explicit range/rate/frame identities");
    }

    expect_equal(exporting::sequence_frame_filename("f", 998U, 1002U, 2U), std::string{"f_0998.png"}, "sequence padding expands deterministically for the largest frame address");
    expect_equal(project_snapshot(session), before_project, "completed sequence export does not mutate project state");
    expect_equal(session.genome_identity(), before_genome, "completed sequence export does not mutate genome");
    expect_equal(session.current_frame(), before_frame, "completed sequence export does not move interactive semantic frame");

    exporting::FrameSequenceRequest cancelled_request = request;
    cancelled_request.directory = directory / "cancelled";
    cancelled_request.frame_end_exclusive = 5U;
    std::size_t cancel_checks = 0U;
    const exporting::ExportResult cancelled = exporting::export_frame_sequence(
        session,
        cancelled_request,
        [&]() {
            const bool stop = cancel_checks >= 2U;
            ++cancel_checks;
            return stop;
        });
    expect(cancelled.ok() && cancelled.cancelled, "sequence cancellation is a structured non-error outcome");
    expect(cancelled.manifest.has_value(), "cancelled sequence retains an audit manifest in memory");
    if (cancelled.manifest.has_value()) {
        expect(!cancelled.manifest->sequence_complete && cancelled.manifest->sequence_cancelled, "partial sequence manifest distinguishes cancellation from completion");
        expect_equal(cancelled.manifest->frames.size(), std::size_t{2U}, "partial manifest lists exactly the atomically completed frames");
    }
    expect(std::filesystem::exists(cancelled_request.directory / "seq_0000.png"), "cancellation preserves already completed frame zero");
    expect(std::filesystem::exists(cancelled_request.directory / "seq_0001.png"), "cancellation preserves already completed frame one");
    expect(!std::filesystem::exists(cancelled_request.directory / "seq_0002.png"), "cancellation does not leave a final-looking unfinished next frame");
    expect_equal(project_snapshot(session), before_project, "cancelled sequence export does not mutate project state");
    expect_equal(session.genome_identity(), before_genome, "cancelled sequence export does not mutate genome");
    expect_equal(session.current_frame(), before_frame, "cancelled sequence export does not move interactive semantic frame");
}

}  // namespace

int main() {
    try {
        test_still_is_full_resolution_and_manifest_round_trips();
        test_collision_overwrite_and_invalid_destination();
        test_contact_sheet_order_and_mapping();
        test_sequence_naming_hashes_and_cancellation();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNEXPECTED EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " export contract test(s) failed\n";
        return 1;
    }
    std::cout << "FM-012 canonical export contracts passed\n";
    return 0;
}
