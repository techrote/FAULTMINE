#include "faultmine/temporal_timeline.hpp"

#include <cstdlib>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace faultmine::core {
namespace {

[[nodiscard]] std::optional<std::string> timeline_rate_executor(
    const ImageBuffer& input,
    ImageBuffer& output,
    const OperatorInstance& instance,
    RootSeed) {
    const auto numerator = instance.parameters.find("rate_num");
    const auto denominator = instance.parameters.find("rate_den");
    const auto* num = numerator == instance.parameters.end() ? nullptr : std::get_if<std::uint64_t>(&numerator->second);
    const auto* den = denominator == instance.parameters.end() ? nullptr : std::get_if<std::uint64_t>(&denominator->second);
    if (num == nullptr || den == nullptr || *num == 0U || *num > 240000U || *den == 0U || *den > 100000U) {
        return std::string{"timeline rate must be an explicit positive rational within v1 bounds"};
    }
    output = input;
    return std::nullopt;
}

[[nodiscard]] MutationMetadata range(const std::uint64_t minimum, const std::uint64_t maximum) {
    MutationMetadata metadata;
    metadata.domain = MutationDomain::unsigned_range;
    metadata.unsigned_min = minimum;
    metadata.unsigned_max = maximum;
    metadata.unsigned_step = 1U;
    return metadata;
}

}  // namespace

void register_temporal_timeline_fault(FaultRegistry& registry) {
    OperatorDescriptor descriptor;
    descriptor.type_id = kFaultTimelineRate;
    descriptor.minimum_supported_version = 1U;
    descriptor.current_version = 1U;
    descriptor.parameters = {
        ParameterDescriptor{"rate_num", ParameterKind::unsigned_integer, true, range(1U, 240000U)},
        ParameterDescriptor{"rate_den", ParameterKind::unsigned_integer, true, range(1U, 100000U)}};
    std::string error;
    if (!registry.register_operator(std::move(descriptor), &timeline_rate_executor, &error)) {
        std::abort();
    }
}

}  // namespace faultmine::core
