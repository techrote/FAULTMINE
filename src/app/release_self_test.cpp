#include "app/release_self_test.hpp"

#include "faultmine/export.hpp"
#include "faultmine/image.hpp"
#include "faultmine/mutation.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/wic_io.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace faultmine::app {
namespace {

[[nodiscard]] bool write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(stream);
}

[[nodiscard]] core::ImageBuffer make_fixture() {
    auto created = core::make_rgba8_image(96U, 64U);
    if (!created.ok()) {
        return {};
    }
    core::ImageBuffer image = std::move(*created.image);
    for (std::uint32_t y = 0U; y < image.height; ++y) {
        for (std::uint32_t x = 0U; x < image.width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
            image.bytes[offset + 0U] = static_cast<std::uint8_t>((x * 7U + y * 3U) & 0xffU);
            image.bytes[offset + 1U] = static_cast<std::uint8_t>((x * 5U + y * 11U) & 0xffU);
            image.bytes[offset + 2U] = static_cast<std::uint8_t>((x * 13U + y * 2U) & 0xffU);
            image.bytes[offset + 3U] = 255U;
        }
    }
    return image;
}

}  // namespace

int run_release_self_test() noexcept {
    std::filesystem::path root;
    try {
        root = std::filesystem::temp_directory_path() / "FAULTMINE-v1-release-self-test";
        std::error_code cleanup_error;
        std::filesystem::remove_all(root, cleanup_error);
        std::filesystem::create_directories(root);

        const std::filesystem::path source_path = root / "source.png";
        const std::filesystem::path project_path = root / "roundtrip.fmproj";
        const std::filesystem::path still_path = root / "canonical.png";
        const std::filesystem::path sequence_path = root / "sequence";

        const core::ImageBuffer fixture = make_fixture();
        if (fixture.bytes.empty()) {
            return 10;
        }
        if (io::save_wic_png(fixture, source_path).has_value()) {
            return 11;
        }
        const auto loaded = io::load_wic_image(source_path);
        if (!loaded.ok()) {
            return 12;
        }

        SessionModel session;
        std::string error;
        if (!session.set_source(
                loaded.source->image,
                loaded.source->source_identity,
                source_path,
                &error)) {
            return 13;
        }

        session.adjust_row_offset(3);
        session.reroll_seed();
        core::MutationRequest request;
        request.mutation_seed = core::RootSeed{0x4d494e455631ULL};
        request.descendant_index = 7U;
        request.radius = core::MutationRadius::medium;
        const core::MutationResult descendant = core::generate_descendant(
            session.genome(),
            session.registry().schema_registry(),
            request);
        if (!descendant.ok()) {
            return 14;
        }
        if (!session.promote_mutation_specimen(
                *descendant.genome,
                descendant.provenance,
                true,
                &error)) {
            return 15;
        }
        if (session.lineage().active() == nullptr || !session.lineage().active()->favourite) {
            return 16;
        }

        const auto project = session.make_project_document(&error);
        if (!project.has_value()) {
            return 17;
        }
        const std::string project_text = serialize_project_canonical(*project);
        if (!write_text(project_path, project_text)) {
            return 18;
        }
        const auto parsed = parse_project(project_text, session.registry().schema_registry());
        if (!parsed.ok()) {
            return 19;
        }

        SessionModel reopened;
        if (!reopened.load_project_state(
                *parsed.project,
                loaded.source->image,
                source_path,
                &error)) {
            return 20;
        }
        if (reopened.genome_identity() != session.genome_identity() ||
            reopened.lineage().active() == nullptr ||
            !reopened.lineage().active()->favourite) {
            return 21;
        }

        exporting::StillExportRequest still;
        still.destination = still_path;
        still.write_manifest = true;
        const exporting::ExportResult still_result = exporting::export_still(reopened, still);
        if (!still_result.ok() || !std::filesystem::exists(still_path)) {
            return 22;
        }

        const EditResult temporal_added = reopened.add_operator(
            "temporal.feedback-blend",
            reopened.genome().operators.size());
        if (!temporal_added.ok()) {
            return 23;
        }
        if (!reopened.step_frame_forward() || reopened.current_frame() != 1U) {
            return 24;
        }

        exporting::FrameSequenceRequest sequence;
        sequence.directory = sequence_path;
        sequence.stem = "frame";
        sequence.frame_begin = 0U;
        sequence.frame_end_exclusive = 3U;
        sequence.write_manifest = true;
        const exporting::ExportResult sequence_result = exporting::export_frame_sequence(reopened, sequence);
        if (!sequence_result.ok() || sequence_result.cancelled || sequence_result.completed_files.size() < 3U) {
            return 25;
        }

        std::filesystem::remove_all(root, cleanup_error);
        return 0;
    } catch (...) {
        std::error_code cleanup_error;
        if (!root.empty()) {
            std::filesystem::remove_all(root, cleanup_error);
        }
        return 99;
    }
}

}  // namespace faultmine::app
