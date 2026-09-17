#pragma once

#include "faultmine/pipeline.hpp"

namespace faultmine::core {

inline constexpr const char* kColourPaletteNearest = "colour.palette-nearest";
inline constexpr const char* kColourGradientMap = "colour.gradient-map";
inline constexpr const char* kColourLut = "colour.lut";
inline constexpr const char* kColourQuantize = "colour.quantize";
inline constexpr const char* kColourDitherOrdered = "colour.dither-ordered";
inline constexpr const char* kColourDitherNoise = "colour.dither-noise";
inline constexpr const char* kColourGeneratedPaletteMap = "colour.generated-palette-map";

void register_colour_faults(FaultRegistry& registry);

}  // namespace faultmine::core
