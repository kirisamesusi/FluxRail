#include "PushFilter.h"

namespace fluxrail {
namespace knobs {
void PushFilter::apply(const PushSettings &s) { settings_ = s; reset(); }
void PushFilter::reset() {
  if (down_) ++stats_.releases;
  down_ = armed_ = low_seen_ = candidate_ = releasing_ = mismatch_ = sampled_ = false;
}
bool PushFilter::sample(const Sample &s, bool rotating) {
  const bool gap = sampled_ && (s.timestamp_us - last_sample_us_ > settings_.max_sample_gap_us ||
                               s.sequence != last_sequence_ + 1);
  if (gap) { if (down_) ++stats_.quality_releases; reset(); }
  sampled_ = true; last_sample_us_ = s.timestamp_us; last_sequence_ = s.sequence;
  const Frame f = decode(s);
  const bool healthy = f.crc_ok && f.field_ok;
  const bool mismatch = healthy && s.push_gpio != f.push;
  if (mismatch && !mismatch_) ++stats_.mismatches;
  mismatch_ = mismatch;
  if (!settings_.enabled || !healthy) {
    if (down_) { ++stats_.releases; if (!healthy) ++stats_.quality_releases; }
    if (candidate_) ++stats_.rejected_candidates;
    down_ = armed_ = low_seen_ = candidate_ = releasing_ = false;
    return false;
  }
  const bool high = s.push_gpio && f.push;
  const bool low = !s.push_gpio && !f.push;
  const uint32_t now = s.timestamp_us;
  if (down_ && now - held_since_ >= settings_.max_hold_us) {
    ++stats_.hold_timeouts; ++stats_.releases;
    down_ = armed_ = low_seen_ = candidate_ = releasing_ = false;
    return false; // 仍高时不重复按下；必须先回到稳定低。
  }
  if (!armed_) {
    if (low) {
      if (!low_seen_) { low_seen_ = true; low_since_ = now; }
      if (now - low_since_ >= settings_.rearm_us) armed_ = true;
    } else low_seen_ = false;
    return false;
  }
  if (down_) {
    // 任一路先释放即开始UP确认，短GPIO/SSI时间偏差不会制造第二次DOWN。
    if (!high) {
      if (!releasing_) { releasing_ = true; candidate_since_ = now; }
      if (now - candidate_since_ >= settings_.release_us) {
        ++stats_.releases; down_ = armed_ = low_seen_ = releasing_ = false;
      }
    } else releasing_ = false;
  } else if (high) {
    if (!candidate_) {
      candidate_ = true; candidate_since_ = now; candidate_rotating_ = rotating;
    }
    // 期间曾旋转便保留更长确认门槛，避免短暂静止提前接受旋转诱发的脉冲。
    candidate_rotating_ = candidate_rotating_ || rotating;
    const uint32_t threshold = candidate_rotating_ ? settings_.rotating_press_us : settings_.press_us;
    if (now - candidate_since_ >= threshold) {
      ++stats_.presses; down_ = true; candidate_ = false; held_since_ = now;
    }
  } else {
    if (candidate_) ++stats_.rejected_candidates;
    candidate_ = false;
  }
  return down_;
}
} // namespace knobs
} // namespace fluxrail
