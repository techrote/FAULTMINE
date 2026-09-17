#pragma once

#include "faultmine/pipeline.hpp"

namespace faultmine::core {

inline constexpr char kFaultTimelineRate[] = "temporal.timeline-rate";

// Registers a pixel-preserving metadata operator whose explicit rational rate
// is persisted in ordinary canonical genome/project state. The frame index is
// still supplied separately to rendering and never inferred from wall clock.
void register_temporal_timeline_fault(FaultRegistry& registry);

}  // namespace faultmine::core
