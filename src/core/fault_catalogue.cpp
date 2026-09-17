#include "faultmine/fault_catalogue.hpp"

#include "faultmine/colour_operators.hpp"
#include "faultmine/memory_addressing_operators.hpp"
#include "faultmine/representation_bit_operators.hpp"
#include "faultmine/starter_operators.hpp"
#include "faultmine/temporal.hpp"
#include "faultmine/temporal_timeline.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace faultmine::core {
namespace {

[[nodiscard]] MutationMetadata signed_range(
    const std::int64_t minimum,
    const std::int64_t maximum,
    const std::int64_t step = 1) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::signed_range;
    metadata.signed_min = minimum;
    metadata.signed_max = maximum;
    metadata.signed_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata unsigned_range(
    const std::uint64_t minimum,
    const std::uint64_t maximum,
    const std::uint64_t step = 1U) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::unsigned_range;
    metadata.unsigned_min = minimum;
    metadata.unsigned_max = maximum;
    metadata.unsigned_step = step;
    return metadata;
}

[[nodiscard]] MutationMetadata choice(std::vector<std::string> choices) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::choice;
    metadata.choices = std::move(choices);
    return metadata;
}

[[nodiscard]] MutationMetadata bitmask() {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::bitmask;
    return metadata;
}

void update_required(
    FaultRegistry& registry,
    const std::string_view type_id,
    const std::string_view parameter,
    MutationMetadata metadata) {
    std::string error;
    if (!registry.schema_registry().update_mutation_metadata(
            type_id, parameter, std::move(metadata), &error)) {
        throw std::runtime_error("failed to refine starter mutation metadata: " + error);
    }
}

void make_starter_faults_mutation_ready(FaultRegistry& registry) {
    const auto boundary = [] {
        return choice({"wrap", "clamp", "fill"});
    };
    const auto channels = [] {
        return choice({"r", "g", "b", "a", "rg", "rb", "gb", "rgb", "rgba"});
    };

    update_required(registry, kFaultRowOffset, "amount", signed_range(-4096, 4096));
    update_required(registry, kFaultRowOffset, "boundary", boundary());

    update_required(registry, kFaultStrideDelta, "delta_bytes", signed_range(-65536, 65536));
    update_required(registry, kFaultStrideDelta, "boundary", boundary());

    update_required(registry, kFaultAddressXor, "mask", bitmask());
    update_required(registry, kFaultAddressXor, "boundary", boundary());

    update_required(
        registry,
        kFaultChannelPermute,
        "order",
        choice({
            "rgba", "rgab", "rbga", "rbag", "ragb", "rabg",
            "grba", "grab", "gbra", "gbar", "garb", "gabr",
            "brga", "brag", "bgra", "bgar", "barg", "bagr",
            "argb", "arbg", "agrb", "agbr", "abrg", "abgr"}));

    update_required(registry, kFaultByteXor, "mask", bitmask());
    update_required(registry, kFaultByteXor, "channels", channels());

    update_required(registry, kFaultBitRotate, "amount", unsigned_range(0U, 7U));
    update_required(registry, kFaultBitRotate, "channels", channels());

    update_required(registry, kFaultScanlineJitter, "max_shift", unsigned_range(0U, 4096U));
    update_required(registry, kFaultScanlineJitter, "boundary", boundary());
}

}  // namespace

FaultRegistry make_default_fault_registry() {
    FaultRegistry registry;
    register_starter_faults(registry);
    make_starter_faults_mutation_ready(registry);
    register_memory_addressing_faults(registry);
    register_representation_bit_faults(registry);
    register_colour_faults(registry);
    register_temporal_timeline_fault(registry);
    register_temporal_faults(registry);
    return registry;
}

}  // namespace faultmine::core
