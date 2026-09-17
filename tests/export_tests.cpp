#include "faultmine/export.hpp"

#include "faultmine/fault_catalogue.hpp"
#include "faultmine/image.hpp"
#include "faultmine/wic_io.hpp"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

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
    auto created = make_rgba8_image(9U, 7U);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    ImageBuffer image = std::move(*created.image);
    for (std::size_t index = 0U; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::uint8_t>((index * 29U + 17U) & 0xffU);
    }
    return image;
}

std::filesystem::path test_root() {
    return std::filesystem::temp_directory_path() /
        (L"faultmine-fm012-" + std::to_wstring(static_cast<unsigned long>(GetCurrentProcessId())));
}

void reset_directory(const std::filesystem::path& root) {
    std::error_code error;
    std::filesystem::remove_all(root, error);
    error.clear();
    std::filesystem::create_directories(root, error);
    if (error) throw std::runtime_error("could not create FM-012 test directory");
}

faultmine::app::SessionModel make_session(const std::filesystem::path& source_path) {
    using namespace faultmine;
    app::SessionModel session;
    core::ImageBuffer source = make_source();
    const std::string identity = core::source_identity_hex(source);
    std::string error;
    if (!session.set_source(std::move(source), identity, source_path, &error)) {
        throw std::runtime_error(error);
    }
    session.set_proxy_spec(core::ProxySpec{3U, 3U, core::kProxyMethodVersion});
    session.set_proxy_enabled(true);
    if (!session.ensure_preview(&error)) throw std::runtime_error(error);
    return session;
}

void test_still_is_full_canonical_and_manifest_round_trips(const std::filesystem::path& root) {
    using namespace faultmine;
    app::SessionModel session = make_session(root / L"source.png");
    const app::PreviewState preview = session.preview_state();
    expect(preview.is_proxy, "test session actually uses a proxy preview");
    expect(preview.width < session.full_source()->width, "proxy preview is smaller than the canonical source");

    const std::filesystem::path output = root / L"still.png";
    const app::ExportResult exported = app::export_canonical_still(session, output);
    expect(exported.success, "canonical still export succeeds");
    expect(exported.manifest.has_value(), "still export returns provenance manifest");
    expect(std::filesystem::exists(output), "still PNG exists after successful atomic commit");
    expect(std::filesystem::exists(app::companion_manifest_path(output)), "companion manifest exists by default");
    if (!exported.success || !exported.manifest.has_value()) return;

    const io::WicLoadResult loaded = io::load_wic_image(output);
    expect(loaded.ok(), "exported canonical PNG decodes through the normal WIC boundary");
    std::string render_error;
    const auto direct = session.render_full(&render_error);
    expect(direct.has_value(), "direct canonical full render succeeds for still comparison");
    if (loaded.ok() && direct.has_value()) {
        expect_equal(loaded.source->image, *direct, "still export pixels equal the full canonical render, never the proxy preview");
        expect_equal(loaded.source->image.width, session.full_source()->width, "still export records full source width");
        expect_equal(loaded.source->image.height, session.full_source()->height, "still export records full source height");
    }

    const app::ExportManifest& manifest = *exported.manifest;
    expect(manifest.canonical_pixels && !manifest.proxy_pixels, "still manifest declares canonical non-proxy pixels");
    expect_equal(manifest.source_identity, session.source_identity(), "manifest binds normalized source identity");
    expect_equal(manifest.genome_identity, session.genome_identity(), "manifest binds exact canonical genome identity");
    expect_equal(manifest.root_seed, session.genome().root_seed.to_string(), "manifest records exact root seed");
    expect_equal(manifest.engine_contract_version, session.genome().engine_contract_version, "manifest records engine contract version");
    expect_equal(manifest.genome_schema_version, session.genome().schema_version, "manifest records genome schema version");
    expect_equal(manifest.application_version, std::string{app::kApplicationVersion}, "manifest records application version");
    expect_equal(manifest.operators.size(), session.genome().operators.size(), "manifest records ordered operator/version summary");
    expect_equal(manifest.frames.size(), std::size_t{1U}, "still manifest carries one frame audit record");

    const std::string text = app::serialize_export_manifest(manifest);
    const app::ExportManifestParseResult parsed = app::parse_export_manifest(text, session.registry().schema_registry());
    expect(parsed.ok(), "export manifest parses under its strict versioned schema");
    if (parsed.ok()) expect_equal(*parsed.manifest, manifest, "export manifest round-trip is exact");

    app::ExportManifest future = manifest;
    future.manifest_schema_version = app::kExportManifestSchemaVersion + 1U;
    expect(!app::parse_export_manifest(app::serialize_export_manifest(future), session.registry().schema_registry()).ok(),
        "future manifest schema is rejected rather than partially interpreted");
}

void test_collision_and_error_semantics(const std::filesystem::path& root) {
    using namespace faultmine;
    const app::SessionModel session = make_session(root / L"source-collision.png");
    const std::string before_identity = session.genome_identity();
    const std::uint64_t before_frame = session.current_frame();
    const std::filesystem::path output = root / L"collision.png";
    expect(app::export_canonical_still(session, output).success, "first collision fixture export succeeds");
    const app::ExportResult refused = app::export_canonical_still(session, output);
    expect(!refused.success && refused.error.find("collision") != std::string::npos,
        "default overwrite policy refuses an existing final path deliberately");

    app::ExportOptions replace;
    replace.overwrite_policy = app::ExportOverwritePolicy::replace_existing;
    expect(app::export_canonical_still(session, output, replace).success,
        "explicit replace policy safely replaces existing export and manifest");

    const std::filesystem::path impossible = root / L"missing-directory" / L"failed.png";
    const app::ExportResult failed = app::export_canonical_still(session, impossible);
    expect(!failed.success && !failed.error.empty(), "invalid destination reports actionable export failure");
    expect(!std::filesystem::exists(impossible), "failed export leaves no final-looking PNG");
    expect_equal(session.genome_identity(), before_identity, "export failures do not mutate canonical genome state");
    expect_equal(session.current_frame(), before_frame, "export failures do not mutate semantic timeline position");
}

void test_contact_sheet_order_and_mapping(const std::filesystem::path& root) {
    using namespace faultmine;
    app::SessionModel session = make_session(root / L"source-contact.png");
    app::SpecimenTrayModel tray;
    app::SpecimenTrayConfig config;
    config.population_size = 4U;
    config.radius = core::MutationRadius::medium;
    std::string error;
    expect(tray.generate(session.genome(), session.locks(), session.registry(), config, &error),
        "contact-sheet fixture generates deterministic specimen tray");
    if (tray.items().empty()) return;

    const std::filesystem::path output = root / L"contact.png";
    const app::ExportResult exported = app::export_contact_sheet(session, tray.items(), output);
    expect(exported.success && exported.manifest.has_value(), "contact-sheet export succeeds with companion provenance");
    if (!exported.manifest.has_value()) return;
    const app::ExportManifest& manifest = *exported.manifest;
    expect(manifest.kind == app::ExportKind::contact_sheet, "manifest identifies contact sheet as presentation artifact");
    expect(!manifest.canonical_pixels && !manifest.proxy_pixels, "contact sheet is not mislabelled as canonical specimen or proxy output");
    expect_equal(manifest.contact_cells.size(), tray.items().size(), "manifest maps every contact-sheet cell");
    for (std::size_t index = 0U; index < tray.items().size(); ++index) {
        expect_equal(manifest.contact_cells[index].cell_index, index, "contact sheet preserves explicit cell order");
        expect_equal(manifest.contact_cells[index].tray_index, index, "contact sheet preserves deterministic tray index order");
        expect_equal(manifest.contact_cells[index].descendant_index, tray.items()[index].provenance.descendant_index,
            "contact sheet maps descendant provenance to its deterministic cell");
        expect_equal(manifest.contact_cells[index].genome_identity, core::genome_identity_hex(tray.items()[index].genome),
            "contact-sheet cell maps exact canonical specimen genome identity");
    }
    const io::WicLoadResult decoded = io::load_wic_image(output);
    expect(decoded.ok(), "contact sheet PNG is valid lossless WIC output");
    if (decoded.ok()) {
        expect_equal(decoded.source->image.width, manifest.width, "contact sheet manifest width matches encoded artifact");
        expect_equal(decoded.source->image.height, manifest.height, "contact sheet manifest height matches encoded artifact");
    }
}

void test_sequence_naming_hashes_and_cancellation(const std::filesystem::path& root) {
    using namespace faultmine;
    app::SessionModel session = make_session(root / L"source-sequence.png");
    session.seek_frame(7U);
    const std::string genome_before = session.genome_identity();
    const std::uint64_t frame_before = session.current_frame();

    app::SequenceOptions range;
    range.frame_start_inclusive = 2U;
    range.frame_end_exclusive = 6U;
    const std::filesystem::path base = root / L"motion.png";
    const app::ExportResult complete = app::export_frame_sequence(session, base, range);
    expect(complete.success && complete.manifest.has_value(), "canonical frame sequence export succeeds");
    if (complete.manifest.has_value()) {
        const app::ExportManifest& manifest = *complete.manifest;
        expect(manifest.complete && !manifest.cancelled, "completed sequence manifest is explicitly complete");
        expect_equal(manifest.frames.size(), std::size_t{4U}, "sequence manifest contains one audit record per exported frame");
        expect_equal(manifest.frames.front().filename_utf8, std::string{"motion-f000002.png"}, "sequence naming is deterministic and zero padded");
        expect_equal(manifest.frames.back().filename_utf8, std::string{"motion-f000005.png"}, "sequence naming preserves explicit end-exclusive range");
        for (const app::ExportFrameRecord& record : manifest.frames) {
            std::string render_error;
            const auto direct = session.render_full_at_frame(record.frame_index, &render_error);
            expect(direct.has_value(), "direct frame render succeeds for sequence audit comparison");
            if (!direct.has_value()) continue;
            expect_equal(record.image_identity, core::source_identity_hex(*direct), "sequence frame audit hash matches direct canonical replay");
            expect_equal(record.temporal_identity,
                core::temporal_frame_identity_hex(session.source_identity(), session.genome(), record.frame_index, *direct),
                "sequence frame temporal identity matches direct canonical replay");
            const io::WicLoadResult decoded = io::load_wic_image(root / std::filesystem::path(record.filename_utf8));
            expect(decoded.ok(), "each sequence frame is a valid PNG");
            if (decoded.ok()) expect_equal(decoded.source->image, *direct, "encoded sequence frame pixels equal direct canonical replay");
        }
    }
    expect_equal(session.genome_identity(), genome_before, "sequence export does not mutate genome identity");
    expect_equal(session.current_frame(), frame_before, "sequence export does not move the interactive semantic frame");

    app::SequenceOptions cancel_range;
    cancel_range.frame_start_inclusive = 10U;
    cancel_range.frame_end_exclusive = 15U;
    const app::ExportResult cancelled = app::export_frame_sequence(
        session,
        root / L"cancel.png",
        cancel_range,
        {},
        [](const app::ExportProgress& progress) { return progress.completed < 2U; });
    expect(cancelled.cancelled && !cancelled.success && cancelled.manifest.has_value(), "sequence cancellation is explicit and not reported as success");
    expect_equal(cancelled.completed_files.size(), std::size_t{3U}, "cancelled sequence preserves two completed frames plus partial audit manifest");
    if (cancelled.manifest.has_value()) {
        expect(!cancelled.manifest->complete && cancelled.manifest->cancelled, "partial sequence manifest truthfully records cancellation");
        expect_equal(cancelled.manifest->frames.size(), std::size_t{2U}, "partial manifest audits exactly the frames committed before cancellation");
    }
    expect(std::filesystem::exists(root / L"cancel-f000010.png"), "first completed frame remains after cancellation");
    expect(std::filesystem::exists(root / L"cancel-f000011.png"), "second completed frame remains after cancellation");
    expect(!std::filesystem::exists(root / L"cancel-f000012.png"), "cancelled frame is not left as a final-looking output");
    expect_equal(session.genome_identity(), genome_before, "cancellation leaves project/genome state unchanged");
    expect_equal(session.current_frame(), frame_before, "cancellation leaves interactive timeline state unchanged");
}

}  // namespace

int main() {
    try {
        const std::filesystem::path root = test_root();
        reset_directory(root);
        test_still_is_full_canonical_and_manifest_round_trips(root);
        test_collision_and_error_semantics(root);
        test_contact_sheet_order_and_mapping(root);
        test_sequence_naming_hashes_and_cancellation(root);
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    } catch (const std::exception& exception) {
        std::cerr << "UNEXPECTED EXCEPTION: " << exception.what() << '\n';
        return 1;
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " FM-012 export contract assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FM-012 export contracts passed.\n";
    return 0;
}
