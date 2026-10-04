#include "KnobMotion.h"
#include <limits.h>

namespace fluxrail {
namespace knobs {
namespace {
double absolute(double v) { return v < 0 ? -v : v; }
void maximum(uint32_t &a, uint32_t b) { if (b > a) a = b; }
}
bool KnobMotion::apply(const Settings &s, uint32_t revision) {
  if (!validate(s) || !revision) return false;
  resetReference();
  settings_ = s;
  revision_ = revision;
  sampled_ = valid_seen_ = false;
  return true;
}
void KnobMotion::resetReference() {
  stats_.filter_discarded_ticks += filter_tail_;
  filter_tail_ = 0;
  reference_ = active_ = false;
  activity_anchor_ = total_ticks_;
}
Motion KnobMotion::sample(const Sample &s, uint8_t id, uint32_t epoch) {
  Motion out{};
  out.knob_id = id; out.axis = settings_.axis;
  out.timestamp_us = s.timestamp_us; out.sequence = s.sequence;
  out.revision = revision_; out.epoch = epoch; out.enabled = settings_.enabled;
  ++stats_.samples;
  bool sequence_gap = false;
  if (sampled_) {
    const uint32_t dt = s.timestamp_us - last_sample_us_;
    maximum(stats_.max_interval_us, dt);
    if (dt > kSamplePeriodUs) ++stats_.deadline_misses;
    sequence_gap = s.sequence != last_sequence_ + 1;
    if (sequence_gap) { ++stats_.sequence_gaps; resetReference(); }
  }
  sampled_ = true; last_sample_us_ = s.timestamp_us; last_sequence_ = s.sequence;
  const Frame f = decode(s);
  out.raw_angle = f.angle; out.field_status = f.status; out.quality = f.quality;
  if (!f.crc_ok) {
    if (f.quality == Quality::CrcError) ++stats_.crc_errors;
    else { ++stats_.transport_errors; resetReference(); }
    // CRC错但短间隔可保留参考；一旦超长仍立即重置，滤波尾量也不延后回放。
    if (reference_ && s.timestamp_us - last_valid_us_ > settings_.max_gap_us)
      resetReference();
  } else if (!f.field_ok) {
    if ((f.status & 3) == 1) ++stats_.strong_field;
    if ((f.status & 3) == 2) ++stats_.weak_field;
    if ((f.status & 3) == 3) ++stats_.reserved_field;
    if (f.status & 8) ++stats_.loss_of_track;
    resetReference();
  } else {
    const uint32_t dt = s.timestamp_us - last_valid_us_;
    if (valid_seen_) maximum(stats_.max_valid_gap_us, dt);
    valid_seen_ = true;
    if (reference_) {
      const double limit = settings_.max_turns_per_second * kTicksPerTurn * dt / 1000000.0;
      int32_t delta = static_cast<int32_t>(f.angle) - last_angle_;
      if (delta > 8191) delta -= kTicksPerTurn;
      else if (delta < -8192) delta += kTicksPerTurn;
      // dt==0、超过配置时间边界或可允许转速走完半圈时均无法可信展开。
      // 此处拒绝整段运动并记原因，不裁剪成一个较小的HID增量。
      if (!dt || dt > settings_.max_gap_us) {
        ++stats_.gaps; resetReference(); out.quality = Quality::Gap;
      } else if (limit >= 8192 || delta == -8192 || absolute(delta) > limit) {
        ++stats_.ambiguous; resetReference(); out.quality = Quality::Ambiguous;
      } else if ((delta > 0 && total_ticks_ > INT64_MAX - delta) ||
                 (delta < 0 && total_ticks_ < INT64_MIN - delta)) {
        ++stats_.ambiguous; resetReference(); out.quality = Quality::Ambiguous;
      } else {
        total_ticks_ += delta;
        out.delta_ticks = delta;
        const double signed_gain = settings_.invert ? -settings_.gain : settings_.gain;
        scaled_turns_ += delta * signed_gain / kTicksPerTurn;
        const double filtered = settings_.filter_tau_us ?
            (filter_tail_ + delta) * dt / (settings_.filter_tau_us + static_cast<double>(dt)) :
            static_cast<double>(delta);
        filter_tail_ += delta - filtered;
        out.delta_counts = settings_.enabled ?
            filtered * signed_gain * settings_.counts_per_turn_at_gain1 / kTicksPerTurn : 0;
        // 物理域累积净位移识别慢移，增益变化不改变活动门槛。
        // 用double相减避免int64跨符号极值的有符号溢出。
        if (absolute(static_cast<double>(total_ticks_) - activity_anchor_) >= settings_.activity_ticks) {
          activity_anchor_ = total_ticks_; activity_us_ = s.timestamp_us; active_ = true;
        }
        if (active_ && s.timestamp_us - activity_us_ > settings_.activity_hold_us) active_ = false;
        out.valid = true; out.active = active_;
      }
    } else out.quality = sequence_gap ? Quality::Gap : Quality::Reference;
    if (!reference_) { ++stats_.reacquire; reference_ = true; }
    last_angle_ = f.angle; last_valid_us_ = s.timestamp_us;
  }
  out.raw_ticks = total_ticks_;
  out.raw_turns = static_cast<double>(total_ticks_) / kTicksPerTurn;
  out.scaled_turns = scaled_turns_;
  return out;
}
} // namespace knobs
} // namespace fluxrail
