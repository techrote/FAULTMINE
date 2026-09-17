#pragma once

#include "faultmine/pipeline.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace faultmine::core {

inline constexpr std::uint32_t kTemporalContractVersion = 1U;

inline constexpr char kFaultFeedbackBlend[] = "temporal.feedback-blend";
inline constexpr char kFaultFeedbackDisplace[] = "temporal.feedback-displace";
inline constexpr char kFaultPartialRefresh[] = "temporal.partial-refresh";
inline constexpr char kFaultTrailAccumulation[] = "temporal.trail";
inline constexpr char kFaultPhaseDrift[] = "temporal.phase-drift";
inline constexpr char kFaultTearingPhase[] = "temporal.tearing-phase";
inline constexpr char kFaultChannelPhase[] = "temporal.channel-phase";

// Exact signed integer modulator. amplitude is non-negative and v1 callers
// constrain it to <= 4096. period_frames must be non-zero.
// Shapes: triangle, saw, square, ramp, sample-hold, keyed-noise.
[[nodiscard]] std::optional<std::string> evaluate_temporal_modulator(
    std::string_view shape,
    std::uint64_t frame_index,
    std::uint64_t period_frames,
    std::uint64_t phase_frames,
    std::int64_t amplitude,
    RootSeed root_seed,
    InstanceId instance_id,
    std::string_view purpose_tag,
    std::int64_t& output);

void register_temporal_faults(FaultRegistry& registry);

// Frame identity is deliberately distinct from genome identity. It binds the
// canonical source identity, canonical genome identity, explicit frame index,
// and canonical output image identity under a versioned domain.
[[nodiscard]] std::string temporal_frame_identity_hex(
    std::string_view source_identity,
    const Genome& genome,
    std::uint64_t frame_index,
    const ImageBuffer& image);

}  // namespace faultmine::core
