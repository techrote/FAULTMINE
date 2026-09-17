#include "faultmine/fault_catalogue.hpp"
#include "faultmine/representation_bit_operators.hpp"
#include "faultmine/starter_operators.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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

faultmine::core::ImageBuffer make_image(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::vector<std::uint8_t>& bytes) {
    const auto created = faultmine::core::make_rgba8_image(width, height, bytes);
    if (!created.ok()) {
        throw std::runtime_error(created.error->message);
    }
    return *created.image;
}

faultmine::core::ImageBuffer make_label_image(const std::uint32_t width, const std::uint32_t height) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) throw std::runtime_error(created.error->message);
    auto image = std::move(*created.image);
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    for (std::size_t pixel = 0U; pixel < pixels; ++pixel) {
        image.bytes[pixel * 4U + 0U] = static_cast<std::uint8_t>(pixel + 1U);
        image.bytes[pixel * 4U + 1U] = static_cast<std::uint8_t>(0x20U + pixel);
        image.bytes[pixel * 4U + 2U] = static_cast<std::uint8_t>(0x40U + pixel);
        image.bytes[pixel * 4U + 3U] = 255U;
    }
    return image;
}

faultmine::core::OperatorInstance make_instance(
    const char* type_id,
    const faultmine::core::InstanceId id,
    std::initializer_list<std::pair<const char*, faultmine::core::ParameterValue>> parameters,
    const bool enabled = true) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type_id;
    instance.type_version = 1U;
    instance.enabled = enabled;
    for (auto parameter : parameters) {
        instance.parameters.emplace(parameter.first, std::move(parameter.second));
    }
    return instance;
}

faultmine::core::ImageBuffer render_single(
    const faultmine::core::ImageBuffer& source,
    faultmine::core::OperatorInstance instance,
    const faultmine::core::RootSeed root = faultmine::core::RootSeed{0x0123456789abcdefULL}) {
    faultmine::core::Genome genome;
    genome.root_seed = root;
    genome.operators.push_back(std::move(instance));
    faultmine::core::FaultRegistry registry;
    faultmine::core::register_representation_bit_faults(registry);
    const auto rendered = faultmine::core::render_pipeline(source, genome, registry);
    if (!rendered.ok()) {
        throw std::runtime_error(rendered.error.has_value() ? rendered.error->message : "render failed");
    }
    return *rendered.image;
}

bool render_single_fails(
    const faultmine::core::ImageBuffer& source,
    faultmine::core::OperatorInstance instance) {
    faultmine::core::Genome genome;
    genome.root_seed = faultmine::core::RootSeed{0x0123456789abcdefULL};
    genome.operators.push_back(std::move(instance));
    faultmine::core::FaultRegistry registry;
    faultmine::core::register_representation_bit_faults(registry);
    return !faultmine::core::render_pipeline(source, genome, registry).ok();
}

std::uint8_t route_value(const std::array<std::uint8_t, 4>& pixel, const char route) {
    switch (route) {
        case 'r': return pixel[0];
        case 'g': return pixel[1];
        case 'b': return pixel[2];
        case 'a': return pixel[3];
        case '0': return 0U;
        case '1': return 255U;
        default: throw std::runtime_error("invalid test route");
    }
}

void test_channel_route() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(1U, 1U, {1U, 2U, 3U, 4U});
    std::array<char, 4> symbols{'a', 'b', 'g', 'r'};
    do {
        const std::string routes(symbols.begin(), symbols.end());
        const ImageBuffer rendered = render_single(
            source,
            make_instance(kFaultChannelRoute, InstanceId{1U, 1U}, {{"routes", routes}}));
        const std::array<std::uint8_t, 4> pixel{1U, 2U, 3U, 4U};
        for (std::size_t channel = 0U; channel < 4U; ++channel) {
            expect_equal(rendered.bytes[channel], route_value(pixel, routes[channel]), "all channel permutations route exactly");
        }
    } while (std::next_permutation(symbols.begin(), symbols.end()));

    expect_equal(
        render_single(source, make_instance(kFaultChannelRoute, InstanceId{1U, 2U}, {{"routes", std::string{"rr01"}}})).bytes,
        std::vector<std::uint8_t>({1U, 1U, 0U, 255U}),
        "channel route supports duplication/drop/fill");
    expect(render_single_fails(source, make_instance(kFaultChannelRoute, InstanceId{1U, 3U}, {{"routes", std::string{"rxba"}}})),
        "invalid channel route is rejected");
}

void test_channel_offset() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(
        3U, 1U,
        {10U,1U,2U,255U, 20U,3U,4U,255U, 30U,5U,6U,255U});
    const ImageBuffer shifted = render_single(
        source,
        make_instance(
            kFaultChannelOffset,
            InstanceId{2U, 1U},
            {{"channels", std::string{"r"}}, {"dx", std::int64_t{1}}, {"dy", std::int64_t{0}}, {"boundary", std::string{"fill"}}}));
    expect_equal(
        shifted.bytes,
        std::vector<std::uint8_t>({0U,1U,2U,255U, 10U,3U,4U,255U, 20U,5U,6U,255U}),
        "channel offset delays selected channel without moving others");

    const ImageBuffer extreme = render_single(
        source,
        make_instance(
            kFaultChannelOffset,
            InstanceId{2U, 2U},
            {{"channels", std::string{"r"}}, {"dx", std::numeric_limits<std::int64_t>::min()}, {"dy", std::int64_t{0}}, {"boundary", std::string{"wrap"}}}));
    expect(!validate_canonical_image(extreme).has_value(), "INT64_MIN channel displacement remains bounded and canonical");
}

void test_word_lanes() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(2U, 1U, {1U,2U,3U,4U,5U,6U,7U,8U});
    expect_equal(
        render_single(source, make_instance(
            kFaultWordLanes, InstanceId{3U,1U},
            {{"word_bytes", std::uint64_t{2U}}, {"mode", std::string{"reverse"}}})).bytes,
        std::vector<std::uint8_t>({2U,1U,4U,3U,6U,5U,8U,7U}),
        "two-byte lane reverse is explicit endian swap");
    expect_equal(
        render_single(source, make_instance(
            kFaultWordLanes, InstanceId{3U,2U},
            {{"word_bytes", std::uint64_t{4U}}, {"mode", std::string{"rotate-left"}}})).bytes,
        std::vector<std::uint8_t>({2U,3U,4U,1U,6U,7U,8U,5U}),
        "four-byte lane rotation is exact");
    expect(render_single_fails(source, make_instance(
        kFaultWordLanes, InstanceId{3U,3U},
        {{"word_bytes", std::uint64_t{3U}}, {"mode", std::string{"reverse"}}})),
        "unsupported word width is rejected");
}

void test_packed_reinterpret() {
    using namespace faultmine::core;
    const ImageBuffer magenta = make_image(1U, 1U, {255U,0U,255U,255U});
    expect_equal(
        render_single(magenta, make_instance(
            kFaultPackedReinterpret, InstanceId{4U,1U},
            {{"source_format", std::string{"rgb565"}}, {"source_endian", std::string{"little"}},
             {"interpret_format", std::string{"rgb565"}}, {"interpret_endian", std::string{"little"}}})).bytes,
        std::vector<std::uint8_t>({255U,0U,255U,255U}),
        "RGB565 round trip reproduces boundary colour exactly");
    expect_equal(
        render_single(magenta, make_instance(
            kFaultPackedReinterpret, InstanceId{4U,2U},
            {{"source_format", std::string{"rgb565"}}, {"source_endian", std::string{"little"}},
             {"interpret_format", std::string{"rgb565"}}, {"interpret_endian", std::string{"big"}}})).bytes,
        std::vector<std::uint8_t>({24U,255U,198U,255U}),
        "packed endian disagreement has exact host-independent result");

    const ImageBuffer rgba = make_image(1U, 1U, {255U,128U,0U,64U});
    expect_equal(
        render_single(rgba, make_instance(
            kFaultPackedReinterpret, InstanceId{4U,3U},
            {{"source_format", std::string{"rgba4444"}}, {"source_endian", std::string{"little"}},
             {"interpret_format", std::string{"argb1555"}}, {"interpret_endian", std::string{"little"}}})).bytes,
        std::vector<std::uint8_t>({247U,0U,33U,255U}),
        "packed layout disagreement uses documented truncate/expand rules");
    expect(render_single_fails(rgba, make_instance(
        kFaultPackedReinterpret, InstanceId{4U,4U},
        {{"source_format", std::string{"rgb999"}}, {"source_endian", std::string{"little"}},
         {"interpret_format", std::string{"rgb565"}}, {"interpret_endian", std::string{"little"}}})),
        "unknown packed layout is rejected");
}

void test_planar_layout() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(2U,1U,{1U,2U,3U,4U,5U,6U,7U,8U});
    expect_equal(
        render_single(source, make_instance(
            kFaultPlanarLayout, InstanceId{5U,1U}, {{"mode", std::string{"interleaved-as-planar"}}})).bytes,
        std::vector<std::uint8_t>({1U,3U,5U,7U,2U,4U,6U,8U}),
        "interleaved bytes misread as planes are exact");
    expect_equal(
        render_single(source, make_instance(
            kFaultPlanarLayout, InstanceId{5U,2U}, {{"mode", std::string{"planar-as-interleaved"}}})).bytes,
        std::vector<std::uint8_t>({1U,5U,2U,6U,3U,7U,4U,8U}),
        "conceptual planes misread as interleaved bytes are exact");
}

void test_signed_byte() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(1U,1U,{0U,127U,128U,255U});
    expect_equal(
        render_single(source, make_instance(
            kFaultSignedByte, InstanceId{6U,1U}, {{"channels", std::string{"rgba"}}, {"mode", std::string{"bias-flip"}}})).bytes,
        std::vector<std::uint8_t>({128U,255U,0U,127U}),
        "signed bias reinterpretation flips the explicit sign bit");
    expect_equal(
        render_single(source, make_instance(
            kFaultSignedByte, InstanceId{6U,2U}, {{"channels", std::string{"rgba"}}, {"mode", std::string{"absolute-signed"}}})).bytes,
        std::vector<std::uint8_t>({0U,127U,128U,1U}),
        "signed magnitude endpoints are explicit");
    expect_equal(
        render_single(source, make_instance(
            kFaultSignedByte, InstanceId{6U,3U}, {{"channels", std::string{"rgba"}}, {"mode", std::string{"clamp-negative"}}})).bytes,
        std::vector<std::uint8_t>({0U,127U,0U,0U}),
        "negative signed bytes clamp deterministically without implementation-defined casts");
}

void test_bit_shift_nibble_bitplanes_and_stuck_bits() {
    using namespace faultmine::core;
    const ImageBuffer shifts = make_image(1U,1U,{0x81U,0x7fU,0x55U,0xffU});
    expect_equal(
        render_single(shifts, make_instance(
            kFaultBitShift, InstanceId{7U,1U},
            {{"channels", std::string{"rgba"}}, {"direction", std::string{"left"}}, {"amount", std::uint64_t{1U}}})).bytes,
        std::vector<std::uint8_t>({0x02U,0xfeU,0xaaU,0xfeU}),
        "left shifts use explicit zero fill and byte truncation");
    expect_equal(
        render_single(shifts, make_instance(
            kFaultBitShift, InstanceId{7U,2U},
            {{"channels", std::string{"rgba"}}, {"direction", std::string{"right"}}, {"amount", std::uint64_t{8U}}})).bytes,
        std::vector<std::uint8_t>({0U,0U,0U,0U}),
        "shift by byte width is explicit zero");
    expect(render_single_fails(shifts, make_instance(
        kFaultBitShift, InstanceId{7U,3U},
        {{"channels", std::string{"r"}}, {"direction", std::string{"left"}}, {"amount", std::uint64_t{9U}}})),
        "out-of-range shift is rejected");

    const ImageBuffer nibbles = make_image(1U,1U,{0x12U,0xabU,0xf0U,0x05U});
    expect_equal(
        render_single(nibbles, make_instance(
            kFaultNibbleSwap, InstanceId{8U,1U}, {{"channels", std::string{"rgba"}}})).bytes,
        std::vector<std::uint8_t>({0x21U,0xbaU,0x0fU,0x50U}),
        "nibble swap exact vector");

    const ImageBuffer planes = make_image(1U,1U,{0x01U,0x80U,0x81U,0x00U});
    expect_equal(
        render_single(planes, make_instance(
            kFaultBitplaneSwap, InstanceId{8U,2U},
            {{"channels", std::string{"rgba"}}, {"plane_a", std::uint64_t{0U}}, {"plane_b", std::uint64_t{7U}}})).bytes,
        std::vector<std::uint8_t>({0x80U,0x01U,0x81U,0x00U}),
        "bitplane exchange exact vector");

    const ImageBuffer stuck = make_image(1U,1U,{0xffU,0x00U,0xaaU,0x55U});
    expect_equal(
        render_single(stuck, make_instance(
            kFaultStuckBits, InstanceId{8U,3U},
            {{"channels", std::string{"rgba"}}, {"zero_mask", std::uint64_t{0x0fU}}, {"one_mask", std::uint64_t{0x80U}}})).bytes,
        std::vector<std::uint8_t>({0xf0U,0x80U,0xa0U,0xd0U}),
        "stuck-at-zero and stuck-at-one masks are exact");
    expect(render_single_fails(stuck, make_instance(
        kFaultStuckBits, InstanceId{8U,4U},
        {{"channels", std::string{"r"}}, {"zero_mask", std::uint64_t{0x80U}}, {"one_mask", std::uint64_t{0x80U}}})),
        "overlapping stuck-bit masks are rejected");
}

void test_structured_bit_burst() {
    using namespace faultmine::core;
    const ImageBuffer source = make_label_image(4U,2U);
    const InstanceId id{0x1111111111111111ULL, 0x2222222222222222ULL};
    const OperatorInstance burst = make_instance(
        kFaultBitBurst,
        id,
        {{"block_width", std::uint64_t{2U}}, {"block_height", std::uint64_t{1U}},
         {"burst_count", std::uint64_t{2U}}, {"xor_mask", std::uint64_t{0x80U}},
         {"channels", std::string{"r"}}});
    const ImageBuffer first = render_single(source, burst);
    std::vector<std::uint8_t> expected = source.bytes;
    for (const std::size_t pixel : {2U, 3U, 4U, 5U}) {
        expected[pixel * 4U] ^= 0x80U;
    }
    expect_equal(first.bytes, expected, "structured named bit bursts select exact known blocks");

    auto unrelated = make_named_stream(RootSeed{0x0123456789abcdefULL}, id, "unrelated-bit-test");
    for (int index = 0; index < 1000; ++index) static_cast<void>(unrelated.next_u64());
    const ImageBuffer second = render_single(source, burst);
    expect_equal(second.bytes, first.bytes, "unrelated stream consumption cannot perturb structured bit bursts");

    const ImageBuffer large = make_label_image(100U,100U);
    expect(render_single_fails(large, make_instance(
        kFaultBitBurst, InstanceId{9U,2U},
        {{"block_width", std::uint64_t{100U}}, {"block_height", std::uint64_t{100U}},
         {"burst_count", std::uint64_t{65536U}}, {"xor_mask", std::uint64_t{1U}},
         {"channels", std::string{"r"}}})),
        "hostile bit-burst work is bounded and rejected");
}

void test_composition_serialization_and_metadata() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(
        2U,1U,{0x12U,0x34U,0x56U,0x78U, 0x9aU,0xbcU,0xdeU,0xf0U});
    Genome genome;
    genome.root_seed = RootSeed{0xabcdef0123456789ULL};
    genome.operators = {
        make_instance(kFaultChannelRoute, InstanceId{10U,1U}, {{"routes", std::string{"bgra"}}}),
        make_instance(kFaultNibbleSwap, InstanceId{10U,2U}, {{"channels", std::string{"rgba"}}}),
        make_instance(kFaultBitShift, InstanceId{10U,3U},
            {{"channels", std::string{"rgba"}}, {"direction", std::string{"right"}}, {"amount", std::uint64_t{1U}}}),
    };
    const FaultRegistry registry = make_default_fault_registry();
    const RenderResult rendered = render_pipeline(source, genome, registry);
    expect(rendered.ok(), "representation/bit operators compose in the default catalogue");
    if (rendered.ok()) {
        expect_equal(
            rendered.image->bytes,
            std::vector<std::uint8_t>({0x32U,0x21U,0x10U,0x43U, 0x76U,0x65U,0x54U,0x07U}),
            "multi-operator representation composition exact output");
    }

    const std::string serialized = serialize_canonical_genome(genome);
    const GenomeParseResult parsed = parse_genome(serialized, registry.schema_registry());
    expect(parsed.ok(), "representation genome serializes and reconstructs through default registry");
    if (parsed.genome.has_value()) {
        const RenderResult after_reload = render_pipeline(source, *parsed.genome, registry);
        expect(after_reload.ok(), "reloaded representation genome renders");
        if (rendered.ok() && after_reload.ok()) expect_equal(after_reload.image->bytes, rendered.image->bytes, "save/reload preserves representation output");
    }

    const std::array<const char*, 11> type_ids{
        kFaultChannelRoute, kFaultChannelOffset, kFaultWordLanes, kFaultPackedReinterpret,
        kFaultPlanarLayout, kFaultSignedByte, kFaultBitShift, kFaultNibbleSwap,
        kFaultBitplaneSwap, kFaultStuckBits, kFaultBitBurst};
    for (const char* type_id : type_ids) {
        const OperatorDescriptor* descriptor = registry.schema_registry().find(type_id);
        expect(descriptor != nullptr, "default registry exposes every FM-006 operator");
        if (descriptor == nullptr) continue;
        for (const ParameterDescriptor& parameter : descriptor->parameters) {
            expect(parameter.mutation.mutable_gene, "FM-006 parameter is mutation-enabled");
            expect(parameter.mutation.policy_version != 0U, "FM-006 mutation metadata is versioned");
            expect(parameter.mutation.domain != MutationDomain::opaque, "FM-006 mutation metadata has typed domain");
        }
    }

    Genome disabled;
    disabled.root_seed = RootSeed{1U};
    disabled.operators.push_back(make_instance(
        kFaultNibbleSwap, InstanceId{10U,4U}, {{"channels", std::string{"rgba"}}}, false));
    const RenderResult disabled_result = render_pipeline(source, disabled, registry);
    expect(disabled_result.ok(), "disabled representation operator pipeline succeeds");
    if (disabled_result.ok()) expect_equal(disabled_result.image->bytes, source.bytes, "disabled representation operator is exact no-op");
}

}  // namespace

int main() {
    try {
        test_channel_route();
        test_channel_offset();
        test_word_lanes();
        test_packed_reinterpret();
        test_planar_layout();
        test_signed_byte();
        test_bit_shift_nibble_bitplanes_and_stuck_bits();
        test_structured_bit_burst();
        test_composition_serialization_and_metadata();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " representation/bit assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE representation/bit contracts passed.\n";
    return 0;
}
