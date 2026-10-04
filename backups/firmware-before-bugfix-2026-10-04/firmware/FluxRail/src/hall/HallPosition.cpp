#include "HallPosition.h"

namespace fluxrail {
namespace hall {
bool HallPosition::apply(const Settings &s, uint32_t revision, uint32_t epoch) {
  if (!validateSettings(s))
    return false;
  settings_ = s;
  revision_ = revision;
  epoch_ = epoch;
  resetReference();
  return true;
}
bool HallPosition::load(const Calibration &c) {
  if (!validateCalibration(c))
    return false;
  calibration_ = c;
  resetReference();
  return true;
}
void HallPosition::adoptCompleted(const Calibration &c) {
  calibration_ = c;
  resetReference();
}
void HallPosition::resetReference() {
  referenced_ = false;
  field_valid_ = false;
  activity_seen_ = false;
}
Estimate HallPosition::process(const Sample &s) {
  Estimate e{};
  e.timestamp_us = s.timestamp_us;
  e.sequence = s.sequence;
  e.revision = revision_;
  e.epoch = epoch_;
  // 无效结果携带最后有效位置供诊断，但valid=false，后端不可提交它作新坐标。
  e.mm = held_mm_;
  e.p = held_p_;
  e.output = held_output_;
  auto reject = [&](Quality q) {
    e.quality = q;
    resetReference();
    return e;
  };
  if (!s.ok)
    return reject(Quality::SamplingError);
  for (uint16_t raw : s.raw)
    if (raw <= 8 || raw >= 4087)
      return reject(Quality::Saturated);
  if (!calibration_.valid)
    return reject(Quality::Uncalibrated);
  responses(s, calibration_, e.response);
  float weighted = 0;
  for (uint8_t i = 0; i < kChannels; ++i) {
    const float w = e.response[i] > 0 ? e.response[i] : 0;
    e.signal += w;
    weighted += w * kSensorMm[i];
  }
  if (e.signal > 0)
    e.centroid_mm = weighted / e.signal;
  // Disabled仍提供raw/校准响应诊断；来源不能从禁用的滑轨接管。
  if (!settings_.enabled)
    return reject(Quality::Disabled);
  // 弱信号采用滞回，但任何饱和/偏离标定都立即失效。
  const float gate =
      field_valid_ ? settings_.exit_signal : settings_.enter_signal;
  if (e.signal < gate)
    return reject(Quality::WeakField);
  const Projection p = project(e.response, calibration_);
  if (p.ambiguous)
    return reject(Quality::Ambiguous);
  const float tolerance2 =
      settings_.curve_tolerance * settings_.curve_tolerance;
  if (p.distance2 > tolerance2)
    return reject(Quality::OffCurve);
  e.confidence = clamp(1 - p.distance2 / tolerance2, 0, 1);
  e.raw_mm = p.mm;
  const uint32_t dt = s.timestamp_us - last_us_;
  const bool fresh = !referenced_ || dt > settings_.max_gap_us ||
                     s.sequence - last_sequence_ != 1u;
  if (fresh) {
    // 首次/重新获磁/长缺采/改配置直接恢复当前绝对关系，没有旧运动补发。
    filtered_mm_ = p.mm;
    activity_anchor_ = p.mm;
    activity_seen_ = false;
  } else {
    // 同一时间戳或重复向量不推进滤波。无硬限速/多次移动确认/中心死区。
    const float speed =
        dt ? magnitude(p.mm - last_raw_mm_) * 1000000.0f / dt : 0;
    const float mix = clamp(speed / settings_.fast_speed_mm_s, 0, 1);
    const float tau =
        settings_.quiet_tau_us +
        mix * (float(settings_.moving_tau_us) - settings_.quiet_tau_us);
    if (dt)
      filtered_mm_ += (dt / (dt + tau)) * (p.mm - filtered_mm_);
    // 位移相对累计锚点而非相邻样本；连续微慢移最终仍能标记活动。
    const float threshold = settings_.activity_mm > 3 * calibration_.noise_mm
                                ? settings_.activity_mm
                                : 3 * calibration_.noise_mm;
    if (magnitude(filtered_mm_ - activity_anchor_) >= threshold) {
      activity_anchor_ = filtered_mm_;
      activity_us_ = s.timestamp_us;
      activity_seen_ = true;
    }
  }
  last_us_ = s.timestamp_us;
  last_sequence_ = s.sequence;
  last_raw_mm_ = p.mm;
  referenced_ = true;
  field_valid_ = true;
  e.mm = clamp(filtered_mm_, calibration_.left_mm, calibration_.right_mm);
  e.p = clamp((e.mm - calibration_.left_mm) /
                  (calibration_.right_mm - calibration_.left_mm),
              0, 1);
  const float oriented = settings_.invert ? 1 - e.p : e.p;
  const float scaled =
      clamp(0.5f + settings_.center_gain * (oriented - 0.5f), 0, 1);
  e.output = settings_.range_min +
             scaled * (settings_.range_max - settings_.range_min);
  e.valid = true;
  e.quality = Quality::Valid;
  e.active = activity_seen_ &&
             s.timestamp_us - activity_us_ < settings_.activity_hold_us;
  held_mm_ = e.mm;
  held_p_ = e.p;
  held_output_ = e.output;
  return e;
}
} // namespace hall
} // namespace fluxrail
