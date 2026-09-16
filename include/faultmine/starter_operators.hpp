#pragma once

#include "faultmine/pipeline.hpp"

namespace faultmine::core {

inline constexpr const char* kFaultRowOffset = "fault.row-offset";
inline constexpr const char* kFaultStrideDelta = "fault.stride-delta";
inline constexpr const char* kFaultAddressXor = "fault.address-xor";
inline constexpr const char* kFaultChannelPermute = "fault.channel-permute";
inline constexpr const char* kFaultByteXor = "fault.byte-xor";
inline constexpr const char* kFaultBitRotate = "fault.bit-rotate";
inline constexpr const char* kFaultScanlineJitter = "fault.scanline-jitter";

[[nodiscard]] FaultRegistry make_starter_fault_registry();

}  // namespace faultmine::core
