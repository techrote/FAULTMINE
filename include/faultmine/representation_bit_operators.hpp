#pragma once

#include "faultmine/pipeline.hpp"

namespace faultmine::core {

inline constexpr const char* kFaultChannelRoute = "fault.channel-route";
inline constexpr const char* kFaultChannelOffset = "fault.channel-offset";
inline constexpr const char* kFaultWordLanes = "fault.word-lanes";
inline constexpr const char* kFaultPackedReinterpret = "fault.packed-reinterpret";
inline constexpr const char* kFaultPlanarLayout = "fault.planar-layout";
inline constexpr const char* kFaultSignedByte = "fault.signed-byte";
inline constexpr const char* kFaultBitShift = "fault.bit-shift";
inline constexpr const char* kFaultNibbleSwap = "fault.nibble-swap";
inline constexpr const char* kFaultBitplaneSwap = "fault.bitplane-swap";
inline constexpr const char* kFaultStuckBits = "fault.stuck-bits";
inline constexpr const char* kFaultBitBurst = "fault.bit-burst";

void register_representation_bit_faults(FaultRegistry& registry);

}  // namespace faultmine::core
