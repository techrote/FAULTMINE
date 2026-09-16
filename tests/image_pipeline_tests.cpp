#include "faultmine/determinism.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/starter_operators.hpp"
#include "faultmine/wic_io.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int g_image_failures = 0;

void expect_image(const bool condition, const std::string_view message) {
    if (!condition) {
        ++g_image_failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

template <typename Left, typename Right>
void expect_image_equal(const Left& left, const Right& right, const std::string_view message) {
    expect_image(left == right, message);
}

faultmine::core::ImageBuffer require_image(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::span<const std::uint8_t> bytes) {
    auto created = faultmine::core::make_rgba8_image(width, height, bytes);
    if (!created.ok()) {
        throw std::runtime_error("test image creation failed: " + created.error->message);
    }
    return std::move(*created.image);
}

faultmine::core::ImageBuffer make_main_source() {
    constexpr std::array<std::uint8_t, 48> bytes{
        10,20,30,255, 40,50,60,255, 70,80,90,255, 100,110,120,255,
        130,140,150,255, 160,170,180,255, 190,200,210,255, 220,230,240,255,
        1,2,3,255, 4,5,6,255, 7,8,9,255, 11,12,13,255};
    return require_image(4U, 3U, bytes);
}

faultmine::core::ImageBuffer make_small_source(const std::uint32_t width, const std::uint32_t height) {
    constexpr std::array<std::uint8_t, 16> bytes{
        1,2,3,255, 10,20,30,255, 100,110,120,255, 200,210,220,255};
    return require_image(width, height, bytes);
}

faultmine::core::OperatorInstance make_instance(
    std::string type_id,
    const faultmine::core::InstanceId instance_id,
    std::initializer_list<std::pair<std::string, faultmine::core::ParameterValue>> parameters,
    const bool enabled = true) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = instance_id;
    instance.type_id = std::move(type_id);
    instance.type_version = 1U;
    instance.enabled = enabled;
    for (auto& parameter : parameters) {
        instance.parameters.emplace(parameter.first, parameter.second);
    }
    return instance;
}

faultmine::core::ImageBuffer render_or_throw(
    const faultmine::core::ImageBuffer& source,
    const faultmine::core::Genome& genome,
    const faultmine::core::FaultRegistry& registry) {
    auto rendered = faultmine::core::render_pipeline(source, genome, registry);
    if (!rendered.ok()) {
        std::string message = "render failed";
        if (rendered.error.has_value()) {
            message += ": " + rendered.error->message;
        }
        throw std::runtime_error(message);
    }
    return std::move(*rendered.image);
}

faultmine::core::ImageBuffer render_single(
    const faultmine::core::ImageBuffer& source,
    faultmine::core::OperatorInstance instance,
    const faultmine::core::RootSeed root = faultmine::core::RootSeed{0x0123456789abcdefULL}) {
    const auto registry = faultmine::core::make_starter_fault_registry();
    faultmine::core::Genome genome;
    genome.root_seed = root;
    genome.operators.push_back(std::move(instance));
    return render_or_throw(source, genome, registry);
}

void expect_bytes(
    const faultmine::core::ImageBuffer& image,
    const std::initializer_list<std::uint8_t> expected,
    const std::string_view message) {
    expect_image_equal(image.bytes, std::vector<std::uint8_t>{expected}, message);
}

void test_image_buffer_and_identity() {
    using namespace faultmine::core;
    const ImageBuffer source = make_main_source();
    expect_image(!validate_canonical_image(source).has_value(), "canonical RGBA8 source validates");
    expect_image_equal(source.row_stride, std::uint64_t{16U}, "canonical RGBA8 stride is width * 4");
    expect_image_equal(
        source_identity_hex(source),
        std::string{"c428aed50243cec1d9847a3d677577caba1a4999068cfa6f22e91c178a04a6b8"},
        "normalized source identity known-answer vector");

    expect_image(!make_rgba8_image(0U, 1U).ok(), "zero width is rejected");
    expect_image(!make_rgba8_image(1U, 0U).ok(), "zero height is rejected");
    expect_image(
        !make_rgba8_image(std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()).ok(),
        "impossible canonical byte count is rejected before allocation");

    ImageBuffer bad_stride = source;
    ++bad_stride.row_stride;
    expect_image(validate_canonical_image(bad_stride).has_value(), "noncanonical row stride is rejected");
}

void test_row_offset_boundaries() {
    using namespace faultmine::core;
    const ImageBuffer source = make_small_source(4U, 1U);
    const InstanceId id{1U, 1U};

    const auto wrap = render_single(source, make_instance(
        kFaultRowOffset, id,
        {{"amount", std::int64_t{1}}, {"boundary", std::string{"wrap"}}}));
    expect_bytes(wrap,
        {200,210,220,255, 1,2,3,255, 10,20,30,255, 100,110,120,255},
        "row-offset wrap exact output");

    const auto clamp = render_single(source, make_instance(
        kFaultRowOffset, id,
        {{"amount", std::int64_t{1}}, {"boundary", std::string{"clamp"}}}));
    expect_bytes(clamp,
        {1,2,3,255, 1,2,3,255, 10,20,30,255, 100,110,120,255},
        "row-offset clamp exact output");

    const auto fill = render_single(source, make_instance(
        kFaultRowOffset, id,
        {{"amount", std::int64_t{1}}, {"boundary", std::string{"fill"}}}));
    expect_bytes(fill,
        {0,0,0,0, 1,2,3,255, 10,20,30,255, 100,110,120,255},
        "row-offset fill exact output");

    constexpr std::array<std::uint8_t, 4> one_pixel{9,8,7,255};
    const ImageBuffer one = require_image(1U, 1U, one_pixel);
    const auto extreme_wrap = render_single(one, make_instance(
        kFaultRowOffset, id,
        {{"amount", std::numeric_limits<std::int64_t>::min()}, {"boundary", std::string{"wrap"}}}));
    expect_image_equal(extreme_wrap, one, "1x1 row wrap safely handles INT64_MIN offset");
}

void test_stride_and_address_faults() {
    using namespace faultmine::core;
    const ImageBuffer source2x2 = make_small_source(2U, 2U);
    const auto stride = render_single(source2x2, make_instance(
        kFaultStrideDelta,
        InstanceId{2U, 2U},
        {{"delta_bytes", std::int64_t{4}}, {"boundary", std::string{"wrap"}}}));
    expect_bytes(stride,
        {1,2,3,255, 10,20,30,255, 200,210,220,255, 1,2,3,255},
        "stride-delta wrap exact output");

    const ImageBuffer source4x1 = make_small_source(4U, 1U);
    const auto address = render_single(source4x1, make_instance(
        kFaultAddressXor,
        InstanceId{3U, 3U},
        {{"mask", std::uint64_t{1U}}, {"boundary", std::string{"wrap"}}}));
    expect_bytes(address,
        {10,20,30,255, 1,2,3,255, 200,210,220,255, 100,110,120,255},
        "address-xor exact output");

    const auto registry = make_starter_fault_registry();
    Genome invalid_stride;
    invalid_stride.root_seed = RootSeed{1U};
    invalid_stride.operators.push_back(make_instance(
        kFaultStrideDelta,
        InstanceId{4U, 4U},
        {{"delta_bytes", std::numeric_limits<std::int64_t>::max()}, {"boundary", std::string{"wrap"}}}));
    const auto failed = render_pipeline(source2x2, invalid_stride, registry);
    expect_image(!failed.ok(), "stride arithmetic overflow is reported instead of accessing host memory");
}

void test_representation_and_bit_faults() {
    using namespace faultmine::core;
    const ImageBuffer source = make_small_source(4U, 1U);

    const auto permuted = render_single(source, make_instance(
        kFaultChannelPermute,
        InstanceId{5U, 5U},
        {{"order", std::string{"bgra"}}}));
    expect_bytes(permuted,
        {3,2,1,255, 30,20,10,255, 120,110,100,255, 220,210,200,255},
        "channel permutation exact output");

    const auto xored = render_single(source, make_instance(
        kFaultByteXor,
        InstanceId{6U, 6U},
        {{"mask", std::uint64_t{15U}}, {"channels", std::string{"rb"}}}));
    expect_bytes(xored,
        {14,2,12,255, 5,20,17,255, 107,110,119,255, 199,210,211,255},
        "byte-xor exact output");

    const auto rotated = render_single(source, make_instance(
        kFaultBitRotate,
        InstanceId{7U, 7U},
        {{"amount", std::uint64_t{1U}}, {"channels", std::string{"r"}}}));
    expect_bytes(rotated,
        {2,2,3,255, 20,20,30,255, 200,110,120,255, 145,210,220,255},
        "bit-rotate exact output");

    const auto registry = make_starter_fault_registry();
    Genome invalid;
    invalid.root_seed = RootSeed{1U};
    invalid.operators.push_back(make_instance(
        kFaultByteXor,
        InstanceId{8U, 8U},
        {{"mask", std::uint64_t{256U}}, {"channels", std::string{"r"}}}));
    expect_image(!render_pipeline(source, invalid, registry).ok(), "byte-xor rejects masks outside byte range");
}

void test_named_scanline_jitter() {
    using namespace faultmine::core;
    const ImageBuffer source = make_main_source();
    const RootSeed root{0x0123456789abcdefULL};
    const InstanceId id{0x1111111111111111ULL, 0x2222222222222222ULL};
    const auto jittered = render_single(source, make_instance(
        kFaultScanlineJitter,
        id,
        {{"max_shift", std::uint64_t{2U}}, {"boundary", std::string{"wrap"}}}), root);
    expect_bytes(jittered,
        {
            10,20,30,255, 40,50,60,255, 70,80,90,255, 100,110,120,255,
            190,200,210,255, 220,230,240,255, 130,140,150,255, 160,170,180,255,
            7,8,9,255, 11,12,13,255, 1,2,3,255, 4,5,6,255},
        "scanline-jitter exact named-stream output");
    expect_image_equal(
        source_identity_hex(jittered),
        std::string{"2d17f08071a8a948a55d9a9ba2a7389bae3d93dcfb4943acc3b6d6e38196d3f9"},
        "scanline-jitter output identity known answer");

    auto unrelated = make_named_stream(root, id, "unrelated-test-consumption");
    for (int index = 0; index < 1000; ++index) {
        static_cast<void>(unrelated.next_u64());
    }
    const auto rerendered = render_single(source, make_instance(
        kFaultScanlineJitter,
        id,
        {{"max_shift", std::uint64_t{2U}}, {"boundary", std::string{"wrap"}}}), root);
    expect_image_equal(rerendered, jittered, "unrelated entropy consumption cannot perturb jitter output");
}

faultmine::core::Genome make_multistack_genome() {
    using namespace faultmine::core;
    Genome genome;
    genome.root_seed = RootSeed{0x0123456789abcdefULL};
    genome.operators.push_back(make_instance(
        kFaultRowOffset, InstanceId{10U, 10U},
        {{"amount", std::int64_t{1}}, {"boundary", std::string{"wrap"}}}));
    genome.operators.push_back(make_instance(
        kFaultChannelPermute, InstanceId{11U, 11U},
        {{"order", std::string{"bgra"}}}));
    genome.operators.push_back(make_instance(
        kFaultByteXor, InstanceId{12U, 12U},
        {{"mask", std::uint64_t{15U}}, {"channels", std::string{"rb"}}}));
    genome.operators.push_back(make_instance(
        kFaultAddressXor, InstanceId{13U, 13U},
        {{"mask", std::uint64_t{1U}}, {"boundary", std::string{"wrap"}}}));
    genome.operators.push_back(make_instance(
        kFaultScanlineJitter,
        InstanceId{0x1111111111111111ULL, 0x2222222222222222ULL},
        {{"max_shift", std::uint64_t{2U}}, {"boundary", std::string{"wrap"}}}));
    return genome;
}

void test_pipeline_composition_and_round_trip() {
    using namespace faultmine::core;
    const auto registry = make_starter_fault_registry();
    const ImageBuffer source = make_main_source();
    const Genome genome = make_multistack_genome();

    const ImageBuffer first = render_or_throw(source, genome, registry);
    const ImageBuffer second = render_or_throw(source, genome, registry);
    expect_image_equal(first, second, "repeated canonical render is byte-identical");
    expect_image_equal(
        source_identity_hex(first),
        std::string{"348ac78d545e84a596a7721483dee33eca27be6ca4221170d8741d07424d5dd2"},
        "multi-operator canonical output identity known answer");

    const std::string serialized = serialize_canonical_genome(genome);
    const auto parsed = parse_genome(serialized, registry.schema_registry());
    expect_image(parsed.ok(), "starter genome canonical serialization parses through starter registry");
    if (parsed.genome.has_value()) {
        const ImageBuffer after_reload = render_or_throw(source, *parsed.genome, registry);
        expect_image_equal(after_reload, first, "save/reload reproduces exact canonical output");
    }

    Genome disabled;
    disabled.root_seed = genome.root_seed;
    disabled.operators.push_back(make_instance(
        kFaultRowOffset,
        InstanceId{14U, 14U},
        {{"amount", std::int64_t{1}}, {"boundary", std::string{"wrap"}}},
        false));
    expect_image_equal(render_or_throw(source, disabled, registry), source, "disabled operator is an exact no-op");
}

void test_wic_round_trip_and_path_independence() {
    using namespace faultmine;
    const core::ImageBuffer source = make_main_source();
    const std::filesystem::path base = std::filesystem::current_path();
    const std::filesystem::path first = base / "fm003-wic-a.png";
    const std::filesystem::path second = base / "fm003-wic-b.png";
    const std::filesystem::path changed_path = base / "fm003-wic-changed.png";

    std::filesystem::remove(first);
    std::filesystem::remove(second);
    std::filesystem::remove(changed_path);

    const auto first_error = io::save_wic_png(source, first);
    const auto second_error = io::save_wic_png(source, second);
    expect_image(!first_error.has_value(), "WIC PNG export succeeds for canonical source");
    expect_image(!second_error.has_value(), "same canonical source exports at a second path");

    if (!first_error.has_value() && !second_error.has_value()) {
        const auto loaded_first = io::load_wic_image(first);
        const auto loaded_second = io::load_wic_image(second);
        expect_image(loaded_first.ok(), "WIC exported PNG decodes back to canonical RGBA8");
        expect_image(loaded_second.ok(), "same pixels decode from a different path");
        if (loaded_first.source.has_value() && loaded_second.source.has_value()) {
            expect_image_equal(loaded_first.source->image, source, "WIC PNG round trip preserves canonical pixels");
            expect_image_equal(
                loaded_first.source->source_identity,
                loaded_second.source->source_identity,
                "source path does not participate in normalized source identity");
            expect_image_equal(
                loaded_first.source->source_identity,
                core::source_identity_hex(source),
                "WIC normalized source identity matches pure-core identity");
        }
    }

    core::ImageBuffer changed = source;
    changed.bytes[0] ^= 1U;
    const auto changed_error = io::save_wic_png(changed, changed_path);
    expect_image(!changed_error.has_value(), "changed canonical source exports successfully");
    if (!changed_error.has_value()) {
        const auto loaded_changed = io::load_wic_image(changed_path);
        expect_image(loaded_changed.ok(), "changed PNG decodes successfully");
        if (loaded_changed.source.has_value()) {
            expect_image(
                loaded_changed.source->source_identity != core::source_identity_hex(source),
                "changed normalized content changes source identity even at a stable path model");
        }
    }

    std::filesystem::remove(first);
    std::filesystem::remove(second);
    std::filesystem::remove(changed_path);
}

}  // namespace

int run_image_pipeline_tests() {
    try {
        test_image_buffer_and_identity();
        test_row_offset_boundaries();
        test_stride_and_address_faults();
        test_representation_and_bit_faults();
        test_named_scanline_jitter();
        test_pipeline_composition_and_round_trip();
        test_wic_round_trip_and_path_independence();
    } catch (const std::exception& exception) {
        ++g_image_failures;
        std::cerr << "UNCAUGHT IMAGE/PIPELINE TEST EXCEPTION: " << exception.what() << '\n';
    }
    return g_image_failures;
}
