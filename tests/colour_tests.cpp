#include "faultmine/colour.hpp"
#include "faultmine/colour_operators.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/memory_addressing_operators.hpp"
#include "faultmine/pipeline.hpp"

#include <array>
#include <cstdint>
#include <iostream>
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
    const std::vector<std::array<std::uint8_t, 4U>>& pixels) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(pixels.size() * 4U);
    for (const auto& pixel : pixels) {
        bytes.insert(bytes.end(), pixel.begin(), pixel.end());
    }
    auto created = faultmine::core::make_rgba8_image(width, height, bytes);
    if (!created.ok()) {
        throw std::runtime_error(created.error->message);
    }
    return std::move(*created.image);
}

faultmine::core::OperatorInstance make_instance(
    const char* type_id,
    const faultmine::core::InstanceId instance_id,
    std::initializer_list<std::pair<const std::string, faultmine::core::ParameterValue>> parameters,
    const bool enabled = true) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = instance_id;
    instance.type_id = type_id;
    instance.type_version = 1U;
    instance.enabled = enabled;
    for (const auto& parameter : parameters) {
        instance.parameters.emplace(parameter.first, parameter.second);
    }
    return instance;
}

faultmine::core::ImageBuffer render_single(
    const faultmine::core::ImageBuffer& source,
    faultmine::core::OperatorInstance instance,
    const faultmine::core::RootSeed root_seed = faultmine::core::RootSeed{0x0123456789abcdefULL}) {
    faultmine::core::Genome genome;
    genome.root_seed = root_seed;
    genome.operators.push_back(std::move(instance));
    const auto result = faultmine::core::render_pipeline(
        source,
        genome,
        faultmine::core::make_default_fault_registry());
    if (!result.ok()) {
        throw std::runtime_error(result.error.has_value() ? result.error->message : "render failed");
    }
    return std::move(*result.image);
}

std::vector<std::uint8_t> red_values(const faultmine::core::ImageBuffer& image) {
    std::vector<std::uint8_t> values;
    for (std::size_t offset = 0U; offset < image.bytes.size(); offset += 4U) {
        values.push_back(image.bytes[offset]);
    }
    return values;
}

void test_palette_assets() {
    using namespace faultmine::core;
    const std::string noncanonical =
        "{\"entries\":[\"000000ff\",\"ff0000ff\",\"ffffffff\"],\"schema_version\":1}";
    const auto parsed = parse_palette(noncanonical);
    expect(parsed.ok(), "palette parser accepts valid inspectable JSON independent of field order");
    if (!parsed.ok()) {
        return;
    }
    const std::string canonical =
        "{\"schema_version\":1,\"entries\":[\"000000ff\",\"ff0000ff\",\"ffffffff\"]}\n";
    expect_equal(serialize_palette_canonical(*parsed.palette), canonical, "palette canonical serialization is exact");
    expect_equal(
        palette_identity_hex(*parsed.palette),
        std::string{"ea95d4adf9d527aa50a7119b25a4373977e6b0ab5bc42cc064ee80f3a4321c30"},
        "palette content identity known answer");

    Palette changed = *parsed.palette;
    changed.entries[1].g = 1U;
    expect(palette_identity_hex(changed) != palette_identity_hex(*parsed.palette), "changed palette content changes identity");

    expect(!parse_palette("{\"schema_version\":1,\"schema_version\":1,\"entries\":[\"000000ff\"]}").ok(),
        "duplicate palette fields are rejected");
    expect(!parse_palette("{\"schema_version\":1,\"entries\":[\"xyz\"]}").ok(),
        "malformed palette entry is rejected");
    expect_equal(parse_rgba8_hex("10203040"), std::optional<Rgba8>{Rgba8{0x10U, 0x20U, 0x30U, 0x40U}},
        "RGBA hex parsing is explicit");
}

void test_lut_assets() {
    using namespace faultmine::core;
    const Lut256 identity = make_identity_lut();
    const std::string text = serialize_lut_canonical(identity);
    expect_equal(text.size(), std::size_t{2110U}, "canonical LUT text length is stable");
    expect_equal(
        lut_identity_hex(identity),
        std::string{"0ee1c8c0f071571789b442fb3773ec153e226e2133c96eb1785f68bcfcc60f24"},
        "identity LUT content identity known answer");
    const auto parsed = parse_lut(text);
    expect(parsed.ok(), "canonical LUT parses");
    if (parsed.ok()) {
        expect_equal(*parsed.lut, identity, "LUT parse/serialize round trip preserves all 1024 bytes");
    }
    expect(!parse_lut("{\"schema_version\":1,\"channels\":{\"r\":\"00\",\"g\":\"00\",\"b\":\"00\",\"a\":\"00\"}}").ok(),
        "short LUT channels are rejected");
}

void test_palette_generator() {
    using namespace faultmine::core;
    PaletteGenerationSpec spec;
    spec.start = Rgba8{0U, 16U, 32U, 255U};
    spec.end = Rgba8{240U, 224U, 208U, 128U};
    spec.count = 5U;
    spec.jitter = 7U;
    std::string error;
    const auto palette = generate_palette_ramp(
        spec,
        RootSeed{0x0123456789abcdefULL},
        InstanceId{0x1111111111111111ULL, 0x2222222222222222ULL},
        &error);
    expect(palette.has_value(), "seeded palette generation succeeds");
    if (palette.has_value()) {
        const std::vector<Rgba8> expected{
            {0U, 16U, 32U, 255U},
            {66U, 68U, 81U, 223U},
            {127U, 121U, 119U, 192U},
            {175U, 179U, 160U, 160U},
            {240U, 224U, 208U, 128U}};
        expect_equal(palette->entries, expected, "palette generator named-stream known answer");
        expect_equal(
            palette_identity_hex(*palette),
            std::string{"cdbfdb301894f0e2cb7380a2b1bd74b2577f85f66b44a4d1493b02e80438c434"},
            "generated palette identity known answer");
    }
    spec.count = 0U;
    expect(!generate_palette_ramp(spec, RootSeed{1U}, InstanceId{1U, 2U}, &error).has_value(),
        "palette generator rejects zero entries");
}

void test_palette_mapping() {
    using namespace faultmine::core;
    const Palette tie_palette{1U, {{255U, 0U, 0U, 255U}, {0U, 0U, 255U, 255U}}};
    const std::string tie_text = serialize_palette_canonical(tie_palette);
    const ImageBuffer source = make_image(1U, 1U, {{{128U, 0U, 128U, 77U}}});

    const auto preserve = render_single(source, make_instance(
        kColourPaletteNearest,
        InstanceId{1U, 1U},
        {{"palette", tie_text}, {"alpha_mode", std::string{"preserve"}}}));
    expect_equal(preserve.bytes, std::vector<std::uint8_t>({255U, 0U, 0U, 77U}),
        "nearest-palette exact tie chooses lowest palette index and preserves alpha");

    const auto mapped = render_single(source, make_instance(
        kColourPaletteNearest,
        InstanceId{1U, 2U},
        {{"palette", tie_text}, {"alpha_mode", std::string{"map"}}}));
    expect_equal(mapped.bytes, std::vector<std::uint8_t>({255U, 0U, 0U, 255U}),
        "nearest-palette map mode maps alpha explicitly");
}

void test_gradient_and_lut() {
    using namespace faultmine::core;
    const Palette gradient{1U, {{0U, 0U, 0U, 0U}, {255U, 255U, 255U, 255U}}};
    const std::string gradient_text = serialize_palette_canonical(gradient);
    const ImageBuffer source = make_image(3U, 1U, {
        {{0U, 0U, 0U, 40U}},
        {{128U, 128U, 128U, 80U}},
        {{255U, 255U, 255U, 120U}}});
    const auto preserved = render_single(source, make_instance(
        kColourGradientMap,
        InstanceId{2U, 1U},
        {{"palette", gradient_text}, {"alpha_mode", std::string{"preserve"}}}));
    expect_equal(preserved.bytes, std::vector<std::uint8_t>({
        0U, 0U, 0U, 40U,
        128U, 128U, 128U, 80U,
        255U, 255U, 255U, 120U}),
        "gradient luminance endpoints/midpoint and preserved alpha are exact");

    const auto alpha_mapped = render_single(source, make_instance(
        kColourGradientMap,
        InstanceId{2U, 2U},
        {{"palette", gradient_text}, {"alpha_mode", std::string{"map"}}}));
    expect_equal(alpha_mapped.bytes[3], std::uint8_t{0U}, "gradient map alpha endpoint zero");
    expect_equal(alpha_mapped.bytes[7], std::uint8_t{128U}, "gradient map alpha midpoint");
    expect_equal(alpha_mapped.bytes[11], std::uint8_t{255U}, "gradient map alpha endpoint 255");

    Lut256 lut = make_identity_lut();
    for (std::size_t value = 0U; value < 256U; ++value) {
        lut.channels[0][value] = static_cast<std::uint8_t>(255U - value);
        lut.channels[3][value] = 255U;
    }
    const auto lut_result = render_single(source, make_instance(
        kColourLut,
        InstanceId{2U, 3U},
        {{"lut", serialize_lut_canonical(lut)}}));
    expect_equal(lut_result.bytes, std::vector<std::uint8_t>({
        255U, 0U, 0U, 255U,
        127U, 128U, 128U, 255U,
        0U, 255U, 255U, 255U}),
        "per-channel LUT mapping is exact for all channels including alpha");
}

void test_quantization() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(6U, 1U, {
        {{42U, 9U, 9U, 255U}},
        {{43U, 9U, 9U, 255U}},
        {{127U, 9U, 9U, 255U}},
        {{128U, 9U, 9U, 255U}},
        {{212U, 9U, 9U, 255U}},
        {{213U, 9U, 9U, 255U}}});
    const auto result = render_single(source, make_instance(
        kColourQuantize,
        InstanceId{3U, 1U},
        {{"levels", std::uint64_t{4U}}, {"channels", std::string{"r"}}}));
    expect_equal(red_values(result), std::vector<std::uint8_t>({0U, 85U, 85U, 170U, 170U, 255U}),
        "4-level round-half-up quantization boundaries are exact");
    for (std::size_t offset = 0U; offset < result.bytes.size(); offset += 4U) {
        expect_equal(result.bytes[offset + 1U], std::uint8_t{9U}, "unselected channel survives quantization");
    }
}

void test_ordered_dither() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(2U, 2U, {
        {{128U, 10U, 20U, 255U}}, {{128U, 10U, 20U, 255U}},
        {{128U, 10U, 20U, 255U}}, {{128U, 10U, 20U, 255U}}});
    const auto result = render_single(source, make_instance(
        kColourDitherOrdered,
        InstanceId{4U, 1U},
        {{"levels", std::uint64_t{2U}}, {"channels", std::string{"r"}}, {"matrix", std::string{"bayer2"}}}));
    expect_equal(red_values(result), std::vector<std::uint8_t>({255U, 0U, 0U, 255U}),
        "Bayer2 rank positions produce exact ordered-dither pattern");
    expect_equal(result.bytes[1], std::uint8_t{10U}, "ordered dither preserves unselected channels");
}

void test_noise_dither() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(4U, 1U, {
        {{64U, 0U, 0U, 255U}},
        {{128U, 0U, 0U, 255U}},
        {{192U, 0U, 0U, 255U}},
        {{100U, 0U, 0U, 255U}}});
    const RootSeed seed{0x0123456789abcdefULL};
    const InstanceId id{0x3333333333333333ULL, 0x4444444444444444ULL};
    const auto instance = make_instance(
        kColourDitherNoise,
        id,
        {{"levels", std::uint64_t{2U}}, {"channels", std::string{"r"}}});
    const auto first = render_single(source, instance, seed);
    expect_equal(red_values(first), std::vector<std::uint8_t>({0U, 255U, 255U, 0U}),
        "named-noise dither known-answer vector");

    auto unrelated = make_named_stream(seed, id, "unrelated-colour-test");
    for (int index = 0; index < 1000; ++index) {
        static_cast<void>(unrelated.next_u64());
    }
    const auto second = render_single(source, instance, seed);
    expect_equal(second.bytes, first.bytes, "unrelated entropy consumption cannot perturb noise dither");
}

void test_generated_palette_operator() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(3U, 1U, {
        {{10U, 10U, 10U, 21U}},
        {{120U, 120U, 120U, 22U}},
        {{240U, 240U, 240U, 23U}}});
    const auto result = render_single(source, make_instance(
        kColourGeneratedPaletteMap,
        InstanceId{5U, 1U},
        {
            {"start", std::string{"000000ff"}},
            {"end", std::string{"ffffffff"}},
            {"count", std::uint64_t{2U}},
            {"jitter", std::uint64_t{0U}},
            {"alpha_mode", std::string{"preserve"}},
        }));
    expect_equal(result.bytes, std::vector<std::uint8_t>({
        0U, 0U, 0U, 21U,
        0U, 0U, 0U, 22U,
        255U, 255U, 255U, 23U}),
        "generated two-endpoint palette maps deterministically with preserved alpha");
}

void test_composition_serialization_and_disabled() {
    using namespace faultmine::core;
    const ImageBuffer source = make_image(4U, 1U, {
        {{10U, 10U, 10U, 255U}},
        {{100U, 100U, 100U, 255U}},
        {{180U, 180U, 180U, 255U}},
        {{250U, 250U, 250U, 255U}}});
    Genome genome;
    genome.root_seed = RootSeed{0x0102030405060708ULL};
    genome.operators.push_back(make_instance(
        kFaultAddressOffset,
        InstanceId{6U, 1U},
        {{"offset_pixels", std::int64_t{1}}, {"boundary", std::string{"wrap"}}}));
    genome.operators.push_back(make_instance(
        kColourQuantize,
        InstanceId{6U, 2U},
        {{"levels", std::uint64_t{2U}}, {"channels", std::string{"rgb"}}}));
    const FaultRegistry registry = make_default_fault_registry();
    const auto rendered = render_pipeline(source, genome, registry);
    expect(rendered.ok(), "FM-005 addressing composes with FM-007 quantization");
    if (!rendered.ok()) {
        return;
    }
    expect_equal(red_values(*rendered.image), std::vector<std::uint8_t>({255U, 0U, 0U, 255U}),
        "structural then colour composition exact output");

    const std::string serialized = serialize_canonical_genome(genome);
    const auto parsed = parse_genome(serialized, registry.schema_registry());
    expect(parsed.ok(), "colour genome canonical serialization parses through default registry");
    if (parsed.ok()) {
        const auto rerendered = render_pipeline(source, *parsed.genome, registry);
        expect(rerendered.ok(), "reloaded colour genome rerenders");
        if (rerendered.ok()) {
            expect_equal(rerendered.image->bytes, rendered.image->bytes, "colour genome save/reload reproduces exact output");
        }
    }

    Genome disabled;
    disabled.root_seed = genome.root_seed;
    disabled.operators.push_back(make_instance(
        kColourQuantize,
        InstanceId{6U, 3U},
        {{"levels", std::uint64_t{2U}}, {"channels", std::string{"rgb"}}},
        false));
    const auto no_op = render_pipeline(source, disabled, registry);
    expect(no_op.ok() && no_op.image->bytes == source.bytes, "disabled colour operator is exact no-op");
}

void test_metadata_and_validation() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    constexpr std::array<const char*, 7U> types{
        kColourPaletteNearest,
        kColourGradientMap,
        kColourLut,
        kColourQuantize,
        kColourDitherOrdered,
        kColourDitherNoise,
        kColourGeneratedPaletteMap};
    for (const char* type : types) {
        const OperatorDescriptor* descriptor = registry.schema_registry().find(type);
        expect(descriptor != nullptr, "default registry contains every FM-007 operator");
        if (descriptor != nullptr) {
            for (const ParameterDescriptor& parameter : descriptor->parameters) {
                expect(parameter.mutation.domain != MutationDomain::opaque, "FM-007 parameter mutation metadata is typed");
            }
        }
    }
    const auto* palette_descriptor = registry.schema_registry().find(kColourPaletteNearest);
    expect(palette_descriptor != nullptr && palette_descriptor->parameters[0].mutation.domain == MutationDomain::palette,
        "embedded palette uses structured palette mutation domain");
    const auto* lut_descriptor = registry.schema_registry().find(kColourLut);
    expect(lut_descriptor != nullptr && lut_descriptor->parameters[0].mutation.domain == MutationDomain::lut,
        "embedded LUT uses structured LUT mutation domain");
    const auto* generated_descriptor = registry.schema_registry().find(kColourGeneratedPaletteMap);
    expect(generated_descriptor != nullptr && generated_descriptor->parameters[0].mutation.domain == MutationDomain::colour_rgba,
        "generator endpoints use RGBA colour mutation domain");

    const ImageBuffer one = make_image(1U, 1U, {{{1U, 2U, 3U, 4U}}});
    bool rejected = false;
    try {
        static_cast<void>(render_single(one, make_instance(
            kColourQuantize,
            InstanceId{7U, 1U},
            {{"levels", std::uint64_t{1U}}, {"channels", std::string{"rgb"}}})));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    expect(rejected, "out-of-range quantization levels are rejected");
}

}  // namespace

int main() {
    try {
        test_palette_assets();
        test_lut_assets();
        test_palette_generator();
        test_palette_mapping();
        test_gradient_and_lut();
        test_quantization();
        test_ordered_dither();
        test_noise_dither();
        test_generated_palette_operator();
        test_composition_serialization_and_disabled();
        test_metadata_and_validation();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " colour/LUT assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE colour/LUT contracts passed.\n";
    return 0;
}
