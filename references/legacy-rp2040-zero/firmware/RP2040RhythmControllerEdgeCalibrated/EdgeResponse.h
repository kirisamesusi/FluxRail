#pragma once
#include <math.h>

// Pure mapping used by the firmware and numerical regression tests.
namespace EdgeResponse {
inline float clamp(float x, float low, float high) {
  return x < low ? low : (x > high ? high : x);
}

inline float map(float position, float endpointRatio, float handoffRatio) {
  if (!isfinite(endpointRatio)) endpointRatio = 0.105f;
  if (!isfinite(handoffRatio)) handoffRatio = 0.30f;
  endpointRatio = clamp(endpointRatio, 0.02f, 0.32f);
  handoffRatio = clamp(handoffRatio, endpointRatio + 0.06f, 0.60f);
  // Convert pair ratios to the existing normalized centroid coordinates.
  // Keep at least the original inner +/-0.88 region unchanged.
  const float start = clamp((1.0f + 0.3333f * handoffRatio) / (1.0f + handoffRatio), 0.88f, 0.95f);
  const float full = clamp((1.0f + 0.3333f * endpointRatio) / (1.0f + endpointRatio), start + 0.03f, 0.99f);
  const float x = clamp(position, 0.0f, 1.0f);
  const float t = clamp((x - start) / (full - start), 0.0f, 1.0f);
  const float blend = t * t * (3.0f - 2.0f * t);
  // At most 45% correction. Derivative >= 0.55 throughout: no foldback,
  // no forced saturation before x=1, no amplitude-dependent hard switch.
  return x + (1.0f - x) * 0.45f * blend;
}
}
