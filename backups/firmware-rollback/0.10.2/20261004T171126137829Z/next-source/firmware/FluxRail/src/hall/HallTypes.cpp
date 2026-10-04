#include "HallTypes.h"
#include <math.h>

namespace fluxrail {
namespace hall {
float clamp(float x, float lo, float hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}
float magnitude(float x) { return x < 0 ? -x : x; }
float squareRoot(float x) {
  if (x <= 0)
    return 0;
  float y = x > 1 ? x : 1;
  for (uint8_t i = 0; i < 24; ++i)
    y = 0.5f * (y + x / y);
  return y;
}
static bool between(float v, float lo, float hi) {
  return isfinite(v) && v >= lo && v <= hi;
}
bool validateSettings(const Settings &s) {
  return between(s.center_gain, 0.1f, 8) && between(s.range_min, 0, 1) &&
         between(s.range_max, 0, 1) && s.range_min < s.range_max &&
         s.quiet_tau_us <= 20000 && s.moving_tau_us <= s.quiet_tau_us &&
         between(s.fast_speed_mm_s, 1, 5000) &&
         between(s.activity_mm, 0.01f, 5) && s.activity_hold_us <= 100000 &&
         between(s.enter_signal, 1, 12000) &&
         between(s.exit_signal, 1, s.enter_signal) &&
         between(s.curve_tolerance, 1, 1000) && s.max_gap_us >= 1000 &&
         s.max_gap_us <= 100000;
}
void responses(const Sample &s, const Calibration &c, float out[kChannels]) {
  for (uint8_t i = 0; i < kChannels; ++i)
    out[i] = (float(s.raw[i]) - c.baseline[i]) * c.polarity[i] * c.gain[i];
}
Projection project(const float r[kChannels], const Calibration &c) {
  Projection best{};
  best.distance2 = 1e30f;
  if (c.node_count < 2 || c.node_count > kMaxNodes) {
    best.ambiguous = true;
    return best;
  }
  float distances[kMaxNodes]{};
  float positions[kMaxNodes]{};
  // 最多16段，每段常量工作量。不对绝对坐标求差值或积分。
  for (uint8_t j = 0; j + 1 < c.node_count; ++j) {
    const Node &a = c.nodes[j], &b = c.nodes[j + 1];
    float norm = 0, dot = 0;
    for (uint8_t i = 0; i < kChannels; ++i) {
      const float d = b.response[i] - a.response[i];
      norm += d * d;
      dot += (r[i] - a.response[i]) * d;
    }
    const float t = norm > 0 ? clamp(dot / norm, 0, 1) : 0;
    float d2 = 0;
    for (uint8_t i = 0; i < kChannels; ++i) {
      const float error =
          r[i] - (a.response[i] + t * (b.response[i] - a.response[i]));
      d2 += error * error;
    }
    distances[j] = d2;
    positions[j] = a.mm + t * (b.mm - a.mm);
    if (d2 < best.distance2)
      best = {positions[j], d2, t, j, false};
  }
  // 非邻段出现近似同一响应且对应mm明显不同，禁止用上次位置猜一条分支。
  // 邻段在共用节点附近的同一位置不属于歧义。
  for (uint8_t j = 0; j + 1 < c.node_count; ++j) {
    if (magnitude(positions[j] - best.mm) > 2.0f &&
        distances[j] <= best.distance2 + 25.0f)
      best.ambiguous = true;
  }
  return best;
}
bool validateCalibrationStructure(const Calibration &c) {
  if (!c.valid || c.version != kCalibrationVersion ||
      c.node_count < kMinNodes || c.node_count > kMaxNodes ||
      !between(c.left_mm, -200, 200) || !between(c.right_mm, -200, 200) ||
      c.right_mm - c.left_mm < 1 || !between(c.noise_mm, 0, 5) ||
      !c.sweep_left || !c.sweep_right ||
      c.sweep_segments != ((1u << (c.node_count - 1)) - 1u))
    return false;
  for (uint8_t i = 0; i < kChannels; ++i)
    if (!between(c.baseline[i], 16, 4079) ||
        !between(c.baseline_noise[i], 0, 8) || !between(c.gain[i], 0.1f, 10) ||
        (c.polarity[i] != 1 && c.polarity[i] != -1))
      return false;
  if (magnitude(c.nodes[0].mm - c.left_mm) > 0.01f ||
      magnitude(c.nodes[c.node_count - 1].mm - c.right_mm) > 0.01f)
    return false;
  // 至少覆盖传感器上方、两交接区和真实端点，节点最大间距20mm。
  const float landmarks[5] = {kSensorMm[0], kSensorMm[0] * 0.5f, 0,
                              kSensorMm[2] * 0.5f, kSensorMm[2]};
  for (float mm : landmarks) {
    if (mm < c.left_mm || mm > c.right_mm)
      continue; // 实测行程可短于传感器跨度。
    bool found = false;
    for (uint8_t j = 0; j < c.node_count; ++j)
      if (magnitude(c.nodes[j].mm - mm) <= 2)
        found = true;
    if (!found)
      return false;
  }
  for (uint8_t j = 0; j < c.node_count; ++j) {
    const Node &n = c.nodes[j];
    float sum = 0;
    if (!between(n.mm, c.left_mm, c.right_mm))
      return false;
    for (uint8_t i = 0; i < kChannels; ++i) {
      if (!between(n.response[i], -4095, 4095) || !between(n.noise[i], 0, 80))
        return false;
      // 保存数据也检查对应原始ADC不饱和，不信任外部valid字段。
      const float raw =
          c.baseline[i] + n.response[i] / (c.polarity[i] * c.gain[i]);
      if (!between(raw, 9, 4086))
        return false;
      if (n.response[i] > 0)
        sum += n.response[i];
    }
    if (sum < 80)
      return false;
    if (j) {
      const Node &prev = c.nodes[j - 1];
      const float gap = n.mm - prev.mm;
      float norm = 0, noise2 = 0;
      for (uint8_t i = 0; i < kChannels; ++i) {
        const float d = n.response[i] - prev.response[i];
        norm += d * d;
        noise2 += n.noise[i] * n.noise[i] + prev.noise[i] * prev.noise[i];
      }
      if (gap < 0.5f || gap > 20 || norm < 100 || norm <= 36 * noise2)
        return false;
    }
  }
  return true;
}
bool validateCalibration(const Calibration &c) {
  if (!validateCalibrationStructure(c))
    return false;
  // 采样每段0/25/50/75/100%，拒绝明显自交/不可辨识表。
  // 这只是有限检查，不是对连续真实磁场的可辨识性证明；实板仍需完整扫描。
  for (uint8_t j = 0; j + 1 < c.node_count; ++j)
    for (uint8_t t = 0; t <= 4; ++t) {
      float r[kChannels]{};
      for (uint8_t i = 0; i < kChannels; ++i)
        r[i] =
            c.nodes[j].response[i] +
            (t * 0.25f) * (c.nodes[j + 1].response[i] - c.nodes[j].response[i]);
      if (project(r, c).ambiguous)
        return false;
    }
  return true;
}
} // namespace hall
} // namespace fluxrail
