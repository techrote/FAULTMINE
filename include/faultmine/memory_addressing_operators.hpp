#pragma once

#include "faultmine/pipeline.hpp"

namespace faultmine::core {

inline constexpr const char* kFaultAddressOffset = "fault.address-offset";
inline constexpr const char* kFaultAddressMask = "fault.address-mask";
inline constexpr const char* kFaultCoordinateRemap = "fault.coordinate-remap";
inline constexpr const char* kFaultTilePermute = "fault.tile-permute";
inline constexpr const char* kFaultBandRepeat = "fault.band-repeat";
inline constexpr const char* kFaultAddressBurst = "fault.address-burst";

void register_memory_addressing_faults(FaultRegistry& registry);

}  // namespace faultmine::core
