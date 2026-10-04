#include "KnobSampler.h"

namespace fluxrail {
namespace knobs {
void KnobSampler::reset() {
  // 序号持续递增，维护恢复由新epoch隔离；不要把漏采伪装成连续旧序号。
  for (uint8_t i = 0; i < kKnobCount; ++i) started_[i] = false;
}
bool KnobSampler::poll(uint8_t id, uint32_t now, Reader &reader, Sample &out) {
  if (id >= kKnobCount) return false;
  if (started_[id]) {
    const uint32_t elapsed = now - phase_us_[id];
    if (elapsed < kSamplePeriodUs) return false;
    const uint32_t periods = elapsed / kSamplePeriodUs;
    stats_.skipped_periods[id] += periods - 1;
    phase_us_[id] += periods * kSamplePeriodUs;
    const uint32_t dt = now - last_us_[id];
    if (dt > stats_.max_poll_gap_us[id]) stats_.max_poll_gap_us[id] = dt;
  } else {
    phase_us_[id] = now;
  }
  started_[id] = true; last_us_[id] = now;
  out = Sample{}; out.timestamp_us = now;
  out.transport_ok = reader.read(id, out);
  out.sequence = next_sequence_[id]++;
  ++stats_.attempts[id];
  return true;
}
} // namespace knobs
} // namespace fluxrail
