#include "faultmine/fault_catalogue.hpp"
#include "faultmine/image.hpp"
#include "faultmine/project.hpp"
#include "faultmine/session.hpp"
#include "faultmine/temporal.hpp"
#include "faultmine/temporal_timeline.hpp"

#include <array>
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

faultmine::core::OperatorInstance make_operator(
    const faultmine::core::InstanceId id,
    const char* type) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type;
    instance.type_version = 1U;
    instance.enabled = true;
    return instance;
}

faultmine::core::ImageBuffer make_three_pixel_source() {
    using namespace faultmine::core;
    const std::array<std::uint8_t, 12> bytes{
        10U, 11U, 12U, 255U,
        20U, 21U, 22U, 255U,
        30U, 31U, 32U, 255U};
    auto created = make_rgba8_image(3U, 1U, bytes);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    return std::move(*created.image);
}

faultmine::core::Genome make_feedback_genome() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x0123456789abcdefULL};
    auto feedback = make_operator(InstanceId{0x0011223344556677ULL, 0x8899aabbccddeeffULL}, kFaultFeedbackDisplace);
    feedback.parameters.emplace("dx", ParameterValue{std::int64_t{1}});
    feedback.parameters.emplace("dy", ParameterValue{std::int64_t{0}});
    feedback.parameters.emplace("amount_256", ParameterValue{std::uint64_t{256U}});
    feedback.parameters.emplace("boundary", ParameterValue{std::string{"wrap"}});
    genome.operators.push_back(std::move(feedback));
    return genome;
}

void test_modulator_known_vectors() {
    using namespace faultmine::core;
    const RootSeed seed{0x0123456789abcdefULL};
    const InstanceId id{0x0011223344556677ULL, 0x8899aabbccddeeffULL};
    std::int64_t value{};

    const std::array<std::int64_t, 5> saw{-4, -2, 0, 2, 4};
    const std::array<std::int64_t, 5> triangle{-4, 0, 4, 0, -4};
    for (std::uint64_t frame = 0U; frame < saw.size(); ++frame) {
        expect(!evaluate_temporal_modulator("saw", frame, 5U, 0U, 4, seed, id, "test-mod", value).has_value(), "saw modulator accepts known vector");
        expect_equal(value, saw[static_cast<std::size_t>(frame)], "saw integer vector is exact");
        expect(!evaluate_temporal_modulator("triangle", frame, 5U, 0U, 4, seed, id, "test-mod", value).has_value(), "triangle modulator accepts known vector");
        expect_equal(value, triangle[static_cast<std::size_t>(frame)], "triangle integer vector is exact");
    }

    const std::array<std::int64_t, 4> square{-3, -3, 3, 3};
    for (std::uint64_t frame = 0U; frame < square.size(); ++frame) {
        expect(!evaluate_temporal_modulator("square", frame, 4U, 0U, 3, seed, id, "test-mod", value).has_value(), "square modulator accepts known vector");
        expect_equal(value, square[static_cast<std::size_t>(frame)], "square integer vector is exact");
    }

    const std::array<std::int64_t, 6> ramp{-4, -2, 0, 2, 4, 4};
    for (std::uint64_t frame = 0U; frame < ramp.size(); ++frame) {
        expect(!evaluate_temporal_modulator("ramp", frame, 5U, 0U, 4, seed, id, "test-mod", value).has_value(), "ramp modulator accepts known vector");
        expect_equal(value, ramp[static_cast<std::size_t>(frame)], "one-shot ramp clamps at its endpoint");
    }

    const std::array<std::int64_t, 8> sample_hold{4, 4, 4, -1, -1, -1, -2, -2};
    const std::array<std::int64_t, 8> keyed{4, -1, -2, -1, -4, 2, -2, -1};
    for (std::uint64_t frame = 0U; frame < sample_hold.size(); ++frame) {
        expect(!evaluate_temporal_modulator("sample-hold", frame, 3U, 0U, 4, seed, id, "test-mod", value).has_value(), "sample-hold accepts named stream vector");
        expect_equal(value, sample_hold[static_cast<std::size_t>(frame)], "sample-hold named entropy vector is exact");
        expect(!evaluate_temporal_modulator("keyed-noise", frame, 3U, 0U, 4, seed, id, "test-mod", value).has_value(), "frame-keyed modulator accepts named stream vector");
        expect_equal(value, keyed[static_cast<std::size_t>(frame)], "frame-keyed named entropy vector is exact");
    }
}

void test_feedback_replay_and_frame_identity() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const ImageBuffer source = make_three_pixel_source();
    const Genome genome = make_feedback_genome();

    const RenderResult frame0 = render_pipeline_at_frame(source, genome, registry, 0U);
    const RenderResult frame1 = render_pipeline_at_frame(source, genome, registry, 1U);
    const RenderResult frame2 = render_pipeline_at_frame(source, genome, registry, 2U);
    const RenderResult frame2_repeat = render_pipeline_at_frame(source, genome, registry, 2U);
    expect(frame0.ok() && frame1.ok() && frame2.ok() && frame2_repeat.ok(), "feedback frames render through deterministic replay");
    if (!frame0.ok() || !frame1.ok() || !frame2.ok() || !frame2_repeat.ok()) return;

    expect_equal(*frame0.image, source, "frame zero feedback state is the defined source passthrough");
    const std::array<std::uint8_t, 12> expected1{
        30U, 31U, 32U, 255U,
        10U, 11U, 12U, 255U,
        20U, 21U, 22U, 255U};
    const std::array<std::uint8_t, 12> expected2{
        20U, 21U, 22U, 255U,
        30U, 31U, 32U, 255U,
        10U, 11U, 12U, 255U};
    expect_equal(frame1.image->bytes, std::vector<std::uint8_t>(expected1.begin(), expected1.end()), "frame one consumes only the model-owned previous operator output");
    expect_equal(frame2.image->bytes, std::vector<std::uint8_t>(expected2.begin(), expected2.end()), "frame two advances feedback exactly one explicit tick at a time");
    expect_equal(*frame2.image, *frame2_repeat.image, "requesting the same frame twice is byte-identical and independent of scheduling history");

    const std::string source_id = source_identity_hex(source);
    const std::string frame1_id = temporal_frame_identity_hex(source_id, genome, 1U, *frame1.image);
    const std::string frame2_id = temporal_frame_identity_hex(source_id, genome, 2U, *frame2.image);
    expect(frame1_id != frame2_id, "temporal frame identity binds explicit frame index and output");
    expect_equal(frame2_id, temporal_frame_identity_hex(source_id, genome, 2U, *frame2_repeat.image), "temporal frame identity is stable for replayed frame");
}

void test_registry_and_temporal_genome_round_trip() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    const std::array<const char*, 8> types{
        kFaultTimelineRate,
        kFaultFeedbackBlend,
        kFaultFeedbackDisplace,
        kFaultPartialRefresh,
        kFaultTrailAccumulation,
        kFaultPhaseDrift,
        kFaultTearingPhase,
        kFaultChannelPhase};
    for (const char* type : types) {
        const OperatorDescriptor* descriptor = registry.schema_registry().find(type);
        expect(descriptor != nullptr, std::string{"default registry exposes "} + type);
        if (descriptor != nullptr) {
            for (const ParameterDescriptor& parameter : descriptor->parameters) {
                expect(parameter.mutation.domain != MutationDomain::opaque, "temporal parameters expose typed mutation metadata");
                expect(parameter.mutation.policy_version != 0U, "temporal parameters expose explicit mutation policy version");
            }
        }
    }

    const Genome genome = make_feedback_genome();
    const std::string canonical = serialize_canonical_genome(genome);
    const GenomeParseResult parsed = parse_genome(canonical, registry.schema_registry());
    expect(parsed.ok(), "temporal genome parses through the ordinary canonical genome schema");
    if (parsed.ok()) expect_equal(*parsed.genome, genome, "temporal genome round-trip preserves exact operator parameters and IDs");
}

void test_session_step_seek_and_project_reload() {
    using namespace faultmine;
    core::ImageBuffer source = make_three_pixel_source();
    const std::string source_id = core::source_identity_hex(source);

    app::SessionModel session;
    std::string error;
    expect(session.set_source(source, source_id, L"C:\\fixtures\\temporal.png", &error), "session accepts temporal test source");
    const std::size_t feedback_index = session.genome().operators.size();
    expect(session.add_operator(core::kFaultFeedbackDisplace, feedback_index).ok(), "generic editor can add temporal feedback operator");
    expect(session.set_parameter_from_text(feedback_index, "dx", "1").ok(), "generic editor sets temporal dx");
    expect(session.set_parameter_from_text(feedback_index, "dy", "0").ok(), "generic editor sets temporal dy");
    expect(session.set_parameter_from_text(feedback_index, "amount_256", "256").ok(), "generic editor sets temporal feedback amount");
    expect(session.set_parameter_from_text(feedback_index, "boundary", "wrap").ok(), "generic editor sets temporal boundary");

    const std::size_t rate_index = session.genome().operators.size();
    expect(session.add_operator(core::kFaultTimelineRate, rate_index).ok(), "generic editor can add explicit semantic timeline rate");
    expect(session.set_parameter_from_text(rate_index, "rate_num", "24").ok(), "timeline numerator is generic typed project state");
    expect(session.set_parameter_from_text(rate_index, "rate_den", "1").ok(), "timeline denominator is generic typed project state");
    expect_equal(session.semantic_timeline_rate(), app::TimelineRate{24U, 1U}, "semantic frame rate is explicit and independent of preview playback rate");

    session.seek_frame(2U);
    expect_equal(session.current_frame(), std::uint64_t{2U}, "session seek sets exact semantic frame");
    expect(session.step_frame_backward(), "session can step backward exactly one frame");
    expect_equal(session.current_frame(), std::uint64_t{1U}, "backward step changes only semantic frame index");
    expect(session.step_frame_forward(), "session can step forward exactly one frame");
    expect_equal(session.current_frame(), std::uint64_t{2U}, "forward step restores exact semantic frame index");
    session.set_preview_rate_milli(2000U);
    expect_equal(session.preview_rate_milli(), std::uint32_t{2000U}, "preview playback rate is separate from semantic timeline rate");

    const auto before_save = session.render_full_at_frame(2U, &error);
    expect(before_save.has_value(), "session renders explicit full-resolution temporal frame before save");
    const auto document = session.make_project_document(&error);
    expect(document.has_value(), "project document can materialize temporal genome and lineage");
    if (!document.has_value() || !before_save.has_value()) return;

    const std::string serialized = app::serialize_project_canonical(*document);
    const app::ProjectParseResult parsed = app::parse_project(serialized, session.registry().schema_registry());
    expect(parsed.ok(), "project parser accepts temporal operators without a parallel persistence format");
    if (!parsed.ok()) return;

    app::SessionModel reloaded;
    expect(reloaded.load_project_state(*parsed.project, source, L"C:\\fixtures\\temporal.png", &error), "project reload restores temporal canonical genome and lineage");
    const auto after_reload = reloaded.render_full_at_frame(2U, &error);
    expect(after_reload.has_value(), "reloaded project renders requested temporal frame");
    if (after_reload.has_value()) expect_equal(*after_reload, *before_save, "project save/reload reproduces exact per-frame output");
    expect_equal(reloaded.semantic_timeline_rate(), app::TimelineRate{24U, 1U}, "semantic rate survives project round-trip through canonical genome state");
}

}  // namespace

int main() {
    test_modulator_known_vectors();
    test_feedback_replay_and_frame_identity();
    test_registry_and_temporal_genome_round_trip();
    test_session_step_seek_and_project_reload();

    if (g_failures != 0) {
        std::cerr << g_failures << " temporal contract test(s) failed\n";
        return 1;
    }
    std::cout << "FAULTMINE FM-011 temporal contracts passed\n";
    return 0;
}
