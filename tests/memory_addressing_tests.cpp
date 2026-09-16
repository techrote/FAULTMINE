#include "faultmine/determinism.hpp"
#include "faultmine/fault_catalogue.hpp"
#include "faultmine/genome.hpp"
#include "faultmine/image.hpp"
#include "faultmine/logical_address.hpp"
#include "faultmine/memory_addressing_operators.hpp"
#include "faultmine/pipeline.hpp"
#include "faultmine/starter_operators.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
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

faultmine::core::ImageBuffer make_indexed_image(
    const std::uint32_t width,
    const std::uint32_t height) {
    auto created = faultmine::core::make_rgba8_image(width, height);
    if (!created.ok()) {
        throw std::runtime_error(created.error->message);
    }
    auto image = std::move(*created.image);
    const std::uint64_t count = static_cast<std::uint64_t>(width) * height;
    if (count > 250U) {
        throw std::runtime_error("indexed test image exceeds byte-label range");
    }
    for (std::uint64_t pixel = 0U; pixel < count; ++pixel) {
        const std::size_t offset = static_cast<std::size_t>(pixel * 4U);
        const std::uint8_t label = static_cast<std::uint8_t>(pixel + 1U);
        image.bytes[offset + 0U] = label;
        image.bytes[offset + 1U] = static_cast<std::uint8_t>(100U + label);
        image.bytes[offset + 2U] = static_cast<std::uint8_t>(200U + label);
        image.bytes[offset + 3U] = 255U;
    }
    return image;
}

std::vector<std::uint8_t> labels(const faultmine::core::ImageBuffer& image) {
    std::vector<std::uint8_t> output;
    output.reserve(static_cast<std::size_t>(image.width) * image.height);
    for (std::size_t offset = 0U; offset < image.bytes.size(); offset += 4U) {
        output.push_back(image.bytes[offset]);
    }
    return output;
}

faultmine::core::OperatorInstance make_instance(
    const char* type,
    const faultmine::core::InstanceId id,
    std::initializer_list<std::pair<const char*, faultmine::core::ParameterValue>> parameters,
    const bool enabled = true) {
    faultmine::core::OperatorInstance instance;
    instance.instance_id = id;
    instance.type_id = type;
    instance.type_version = 1U;
    instance.enabled = enabled;
    for (const auto& [name, value] : parameters) {
        instance.parameters.emplace(name, value);
    }
    return instance;
}

faultmine::core::RenderResult render_one(
    const faultmine::core::ImageBuffer& source,
    faultmine::core::OperatorInstance instance,
    const faultmine::core::RootSeed root = faultmine::core::RootSeed{0x0123456789abcdefULL}) {
    faultmine::core::Genome genome;
    genome.root_seed = root;
    genome.operators.push_back(std::move(instance));
    return faultmine::core::render_pipeline(
        source,
        genome,
        faultmine::core::make_default_fault_registry());
}

faultmine::core::ImageBuffer require_image(
    faultmine::core::RenderResult result,
    const std::string_view context) {
    if (!result.ok()) {
        std::string message{context};
        if (result.error.has_value()) {
            message += ": ";
            message += result.error->message;
        }
        throw std::runtime_error(message);
    }
    return std::move(*result.image);
}

void test_shared_boundary_contract() {
    using namespace faultmine::core;
    expect_equal(parse_boundary_policy("wrap"), std::optional<BoundaryPolicy>{BoundaryPolicy::wrap}, "parse wrap boundary");
    expect_equal(parse_boundary_policy("clamp"), std::optional<BoundaryPolicy>{BoundaryPolicy::clamp}, "parse clamp boundary");
    expect_equal(parse_boundary_policy("fill"), std::optional<BoundaryPolicy>{BoundaryPolicy::fill}, "parse fill boundary");
    expect(!parse_boundary_policy("mirror").has_value(), "unsupported boundary is rejected");

    expect_equal(resolve_logical_index(std::int64_t{-1}, 4U, BoundaryPolicy::wrap), std::optional<std::uint64_t>{3U}, "signed wrap lower edge");
    expect_equal(resolve_logical_index(std::int64_t{-1}, 4U, BoundaryPolicy::clamp), std::optional<std::uint64_t>{0U}, "signed clamp lower edge");
    expect(!resolve_logical_index(std::int64_t{-1}, 4U, BoundaryPolicy::fill).has_value(), "signed fill lower edge");
    expect_equal(resolve_logical_index(std::uint64_t{4U}, 4U, BoundaryPolicy::wrap), std::optional<std::uint64_t>{0U}, "unsigned wrap upper edge");
    expect_equal(resolve_logical_index(std::uint64_t{4U}, 4U, BoundaryPolicy::clamp), std::optional<std::uint64_t>{3U}, "unsigned clamp upper edge");
    expect(!resolve_logical_index(std::uint64_t{4U}, 4U, BoundaryPolicy::fill).has_value(), "unsigned fill upper edge");

    expect_equal(
        resolve_displaced_index(3U, std::numeric_limits<std::int64_t>::min(), 8U, BoundaryPolicy::wrap),
        std::optional<std::uint64_t>{3U},
        "INT64_MIN displacement wraps without negation overflow");
    expect_equal(
        resolve_displaced_index(3U, std::numeric_limits<std::int64_t>::min(), 8U, BoundaryPolicy::clamp),
        std::optional<std::uint64_t>{7U},
        "INT64_MIN displacement clamps high without overflow");
    expect(
        !resolve_displaced_index(3U, std::numeric_limits<std::int64_t>::min(), 8U, BoundaryPolicy::fill).has_value(),
        "INT64_MIN displacement fill is safely absent");

    for (std::uint64_t extent = 1U; extent <= 7U; ++extent) {
        for (std::uint64_t base = 0U; base < extent; ++base) {
            for (std::int64_t displacement = -20; displacement <= 20; ++displacement) {
                const std::int64_t logical = static_cast<std::int64_t>(base) - displacement;
                for (const BoundaryPolicy policy : {BoundaryPolicy::wrap, BoundaryPolicy::clamp, BoundaryPolicy::fill}) {
                    std::optional<std::uint64_t> reference;
                    if (logical >= 0 && logical < static_cast<std::int64_t>(extent)) {
                        reference = static_cast<std::uint64_t>(logical);
                    } else if (policy == BoundaryPolicy::wrap) {
                        std::int64_t wrapped = logical % static_cast<std::int64_t>(extent);
                        if (wrapped < 0) {
                            wrapped += static_cast<std::int64_t>(extent);
                        }
                        reference = static_cast<std::uint64_t>(wrapped);
                    } else if (policy == BoundaryPolicy::clamp) {
                        reference = logical < 0 ? 0U : extent - 1U;
                    }
                    expect_equal(
                        resolve_displaced_index(base, displacement, extent, policy),
                        reference,
                        "shared displacement mapper matches slow safe reference");
                }
            }
        }
    }
}

void test_stride_family_contract() {
    using namespace faultmine::core;
    const ImageBuffer source = make_indexed_image(3U, 2U);
    const InstanceId id{1U, 1U};

    const auto equal = require_image(render_one(source, make_instance(
        kFaultStrideDelta, id,
        {{"delta_bytes", std::int64_t{0}}, {"boundary", std::string{"wrap"}}})), "equal stride");
    expect_equal(labels(equal), std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6}), "equal logical stride is identity");

    const auto smaller = require_image(render_one(source, make_instance(
        kFaultStrideDelta, id,
        {{"delta_bytes", std::int64_t{-4}}, {"boundary", std::string{"wrap"}}})), "smaller stride");
    expect_equal(labels(smaller), std::vector<std::uint8_t>({1, 2, 3, 3, 4, 5}), "smaller pitch overlaps previous row");

    const auto larger = require_image(render_one(source, make_instance(
        kFaultStrideDelta, id,
        {{"delta_bytes", std::int64_t{4}}, {"boundary", std::string{"wrap"}}})), "larger stride");
    expect_equal(labels(larger), std::vector<std::uint8_t>({1, 2, 3, 5, 6, 1}), "larger pitch skips logical bytes with wrap");

    const auto overflow = render_one(source, make_instance(
        kFaultStrideDelta, id,
        {{"delta_bytes", std::numeric_limits<std::int64_t>::max()}, {"boundary", std::string{"fill"}}}));
    expect(!overflow.ok(), "stride arithmetic overflow is rejected");
}

void test_address_offset_and_mask() {
    using namespace faultmine::core;
    const ImageBuffer source = make_indexed_image(4U, 2U);
    const InstanceId id{2U, 2U};

    const auto wrapped = require_image(render_one(source, make_instance(
        kFaultAddressOffset, id,
        {{"offset_pixels", std::int64_t{1}}, {"boundary", std::string{"wrap"}}})), "address offset wrap");
    expect_equal(labels(wrapped), std::vector<std::uint8_t>({8, 1, 2, 3, 4, 5, 6, 7}), "linear positive offset moves content forward across row boundaries");

    const auto filled = require_image(render_one(source, make_instance(
        kFaultAddressOffset, id,
        {{"offset_pixels", std::int64_t{1}}, {"boundary", std::string{"fill"}}})), "address offset fill");
    expect_equal(labels(filled), std::vector<std::uint8_t>({0, 1, 2, 3, 4, 5, 6, 7}), "linear fill produces zero pixel when logical address is absent");

    const auto extreme = require_image(render_one(source, make_instance(
        kFaultAddressOffset, id,
        {{"offset_pixels", std::numeric_limits<std::int64_t>::min()}, {"boundary", std::string{"wrap"}}})), "extreme address offset");
    expect_equal(labels(extreme), labels(source), "INT64_MIN offset is safe and mathematically wrapped");

    const auto masked = require_image(render_one(source, make_instance(
        kFaultAddressMask, InstanceId{3U, 3U},
        {
            {"xor_mask", std::uint64_t{1U}},
            {"and_mask", std::numeric_limits<std::uint64_t>::max()},
            {"or_mask", std::uint64_t{0U}},
            {"boundary", std::string{"wrap"}},
        })), "address mask");
    expect_equal(labels(masked), std::vector<std::uint8_t>({2, 1, 4, 3, 6, 5, 8, 7}), "XOR address bit swaps adjacent logical pixels");

    const auto huge_fill = require_image(render_one(source, make_instance(
        kFaultAddressMask, InstanceId{3U, 4U},
        {
            {"xor_mask", std::uint64_t{0U}},
            {"and_mask", std::numeric_limits<std::uint64_t>::max()},
            {"or_mask", std::numeric_limits<std::uint64_t>::max()},
            {"boundary", std::string{"fill"}},
        })), "huge address fill");
    expect_equal(labels(huge_fill), std::vector<std::uint8_t>(8U, 0U), "hostile address mask cannot escape bounded logical buffer");
}

void test_coordinate_remap() {
    using namespace faultmine::core;
    const ImageBuffer source = make_indexed_image(3U, 2U);
    const auto swapped = require_image(render_one(source, make_instance(
        kFaultCoordinateRemap, InstanceId{4U, 4U},
        {
            {"x_offset", std::int64_t{0}},
            {"y_offset", std::int64_t{0}},
            {"x_xor_mask", std::uint64_t{0U}},
            {"y_xor_mask", std::uint64_t{0U}},
            {"swap_xy", true},
            {"boundary", std::string{"wrap"}},
        })), "coordinate swap");
    expect_equal(labels(swapped), std::vector<std::uint8_t>({1, 4, 1, 2, 5, 2}), "coordinate swap has explicit non-square wrap semantics");

    const auto invalid_mask = render_one(source, make_instance(
        kFaultCoordinateRemap, InstanceId{4U, 5U},
        {
            {"x_offset", std::int64_t{0}},
            {"y_offset", std::int64_t{0}},
            {"x_xor_mask", std::uint64_t{1ULL << 40U}},
            {"y_xor_mask", std::uint64_t{0U}},
            {"swap_xy", false},
            {"boundary", std::string{"fill"}},
        }));
    expect(!invalid_mask.ok(), "coordinate masks outside uint32 contract are rejected");
}

void test_tile_permutation_partial_edges_and_entropy() {
    using namespace faultmine::core;
    const RootSeed root{0x0123456789abcdefULL};
    const InstanceId id{0x1111111111111111ULL, 0x2222222222222222ULL};
    const ImageBuffer source = make_indexed_image(4U, 2U);
    const OperatorInstance instance = make_instance(
        kFaultTilePermute,
        id,
        {
            {"tile_width", std::uint64_t{2U}},
            {"tile_height", std::uint64_t{1U}},
            {"boundary", std::string{"wrap"}},
        });
    const auto permuted = require_image(render_one(source, instance, root), "tile permutation");
    expect_equal(labels(permuted), std::vector<std::uint8_t>({1, 2, 7, 8, 5, 6, 3, 4}), "named affine tile permutation exact output");

    auto unrelated = make_named_stream(root, id, "unrelated-memory-test");
    for (int i = 0; i < 1000; ++i) {
        static_cast<void>(unrelated.next_u64());
    }
    const auto rerendered = require_image(render_one(source, instance, root), "tile permutation rerender");
    expect_equal(rerendered.bytes, permuted.bytes, "unrelated entropy consumption cannot perturb tile permutation");

    const ImageBuffer partial_source = make_indexed_image(5U, 3U);
    const auto partial = require_image(render_one(partial_source, make_instance(
        kFaultTilePermute,
        InstanceId{5U, 5U},
        {
            {"tile_width", std::uint64_t{2U}},
            {"tile_height", std::uint64_t{2U}},
            {"boundary", std::string{"fill"}},
        }), root), "partial tile permutation");
    expect_equal(partial.width, std::uint32_t{5U}, "partial-edge tile output preserves width");
    expect_equal(partial.height, std::uint32_t{3U}, "partial-edge tile output preserves height");
    expect(!validate_canonical_image(partial).has_value(), "partial-edge tile result remains canonical and bounded");
}

void test_band_repeat() {
    using namespace faultmine::core;
    const ImageBuffer source = make_indexed_image(4U, 4U);
    const auto repeated = require_image(render_one(source, make_instance(
        kFaultBandRepeat,
        InstanceId{6U, 6U},
        {
            {"band_height", std::uint64_t{1U}},
            {"every", std::uint64_t{2U}},
            {"source_delta_bands", std::int64_t{1}},
            {"boundary", std::string{"fill"}},
        })), "band repeat");
    expect_equal(
        labels(repeated),
        std::vector<std::uint8_t>({1,2,3,4, 1,2,3,4, 9,10,11,12, 9,10,11,12}),
        "every second line repeats the previous logical band");

    const auto invalid = render_one(source, make_instance(
        kFaultBandRepeat,
        InstanceId{6U, 7U},
        {
            {"band_height", std::uint64_t{0U}},
            {"every", std::uint64_t{2U}},
            {"source_delta_bands", std::int64_t{1}},
            {"boundary", std::string{"fill"}},
        }));
    expect(!invalid.ok(), "zero band height is rejected before indexing");
}

void test_address_burst() {
    using namespace faultmine::core;
    const RootSeed root{0x0123456789abcdefULL};
    const InstanceId id{0x3333333333333333ULL, 0x4444444444444444ULL};
    const ImageBuffer source = make_indexed_image(4U, 2U);
    const OperatorInstance instance = make_instance(
        kFaultAddressBurst,
        id,
        {
            {"burst_count", std::uint64_t{1U}},
            {"burst_length_pixels", std::uint64_t{2U}},
            {"max_offset_pixels", std::uint64_t{2U}},
            {"boundary", std::string{"wrap"}},
        });
    const auto burst = require_image(render_one(source, instance, root), "address burst");
    expect_equal(labels(burst), std::vector<std::uint8_t>({1,2,3,3,4,6,7,8}), "named burst exact start/offset output");

    const auto rerender = require_image(render_one(source, instance, root), "address burst rerender");
    expect_equal(rerender.bytes, burst.bytes, "address bursts repeat exactly");

    const auto too_many = render_one(source, make_instance(
        kFaultAddressBurst,
        InstanceId{7U, 8U},
        {
            {"burst_count", std::uint64_t{65537U}},
            {"burst_length_pixels", std::uint64_t{1U}},
            {"max_offset_pixels", std::uint64_t{1U}},
            {"boundary", std::string{"fill"}},
        }));
    expect(!too_many.ok(), "hostile burst count is rejected by bounded work contract");
}

void test_narrow_images_and_disabled_semantics() {
    using namespace faultmine::core;
    for (const auto dimensions : {std::pair{1U,1U}, std::pair{1U,7U}, std::pair{7U,1U}, std::pair{5U,3U}}) {
        const ImageBuffer source = make_indexed_image(dimensions.first, dimensions.second);
        const auto rendered = require_image(render_one(source, make_instance(
            kFaultAddressOffset,
            InstanceId{8U, dimensions.first * 100U + dimensions.second},
            {{"offset_pixels", std::int64_t{5}}, {"boundary", std::string{"wrap"}}})), "narrow/odd address offset");
        expect(!validate_canonical_image(rendered).has_value(), "narrow/odd result stays canonical");
    }

    const ImageBuffer source = make_indexed_image(4U, 2U);
    const auto disabled = require_image(render_one(source, make_instance(
        kFaultAddressMask,
        InstanceId{9U, 9U},
        {
            {"xor_mask", std::uint64_t{std::numeric_limits<std::uint64_t>::max()}},
            {"and_mask", std::numeric_limits<std::uint64_t>::max()},
            {"or_mask", std::uint64_t{0U}},
            {"boundary", std::string{"fill"}},
        },
        false)), "disabled address mask");
    expect_equal(disabled.bytes, source.bytes, "disabled memory/address operator is exact no-op");
}

void test_composition_and_serialization() {
    using namespace faultmine::core;
    const ImageBuffer source = make_indexed_image(4U, 2U);
    Genome genome;
    genome.root_seed = RootSeed{0x0123456789abcdefULL};
    genome.operators.push_back(make_instance(
        kFaultAddressOffset, InstanceId{10U, 1U},
        {{"offset_pixels", std::int64_t{1}}, {"boundary", std::string{"wrap"}}}));
    genome.operators.push_back(make_instance(
        kFaultAddressMask, InstanceId{10U, 2U},
        {
            {"xor_mask", std::uint64_t{1U}},
            {"and_mask", std::numeric_limits<std::uint64_t>::max()},
            {"or_mask", std::uint64_t{0U}},
            {"boundary", std::string{"wrap"}},
        }));
    genome.operators.push_back(make_instance(
        kFaultBandRepeat, InstanceId{10U, 3U},
        {
            {"band_height", std::uint64_t{1U}},
            {"every", std::uint64_t{2U}},
            {"source_delta_bands", std::int64_t{1}},
            {"boundary", std::string{"wrap"}},
        }));

    const FaultRegistry registry = make_default_fault_registry();
    const RenderResult rendered = render_pipeline(source, genome, registry);
    expect(rendered.ok(), "multi-addressing composition renders");
    if (rendered.ok()) {
        expect_equal(labels(*rendered.image), std::vector<std::uint8_t>({1,8,3,2, 1,8,3,2}), "multi-addressing composition exact output");
    }

    const std::string serialized = serialize_canonical_genome(genome);
    const GenomeParseResult parsed = parse_genome(serialized, registry.schema_registry());
    expect(parsed.ok(), "memory/addressing genome round-trips through canonical registry");
    if (parsed.genome.has_value()) {
        const RenderResult after = render_pipeline(source, *parsed.genome, registry);
        expect(after.ok(), "round-tripped memory/addressing genome renders");
        if (rendered.ok() && after.ok()) {
            expect_equal(after.image->bytes, rendered.image->bytes, "serialization round trip preserves addressing output");
        }
    }
}

void test_mutation_metadata() {
    using namespace faultmine::core;
    const FaultRegistry registry = make_default_fault_registry();
    for (const char* type : {
            kFaultAddressOffset,
            kFaultAddressMask,
            kFaultCoordinateRemap,
            kFaultTilePermute,
            kFaultBandRepeat,
            kFaultAddressBurst}) {
        const OperatorDescriptor* descriptor = registry.schema_registry().find(type);
        expect(descriptor != nullptr, "memory/addressing descriptor is registered");
        if (descriptor == nullptr) {
            continue;
        }
        expect_equal(descriptor->current_version, std::uint32_t{1U}, "memory/addressing operator version is stable v1");
        for (const ParameterDescriptor& parameter : descriptor->parameters) {
            expect(parameter.mutation.mutable_gene, "memory/addressing parameter is mutation-enabled");
            expect_equal(parameter.mutation.policy_version, std::uint32_t{1U}, "mutation hint policy version is explicit");
            expect(parameter.mutation.domain != MutationDomain::opaque, "memory/addressing parameter has typed mutation domain");
            if (parameter.mutation.domain == MutationDomain::choice) {
                expect(!parameter.mutation.choices.empty(), "choice mutation metadata lists canonical choices");
            }
        }
    }
}

}  // namespace

int main() {
    try {
        test_shared_boundary_contract();
        test_stride_family_contract();
        test_address_offset_and_mask();
        test_coordinate_remap();
        test_tile_permutation_partial_edges_and_entropy();
        test_band_repeat();
        test_address_burst();
        test_narrow_images_and_disabled_semantics();
        test_composition_and_serialization();
        test_mutation_metadata();
    } catch (const std::exception& exception) {
        ++g_failures;
        std::cerr << "UNCAUGHT TEST EXCEPTION: " << exception.what() << '\n';
    }

    if (g_failures != 0) {
        std::cerr << g_failures << " memory/addressing assertion(s) failed.\n";
        return 1;
    }
    std::cout << "FAULTMINE memory/addressing contracts passed.\n";
    return 0;
}
