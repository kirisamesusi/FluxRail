#include "HallSampler.h"

namespace fluxrail {
namespace hall {
bool HallSampler::poll(uint32_t now, AdcReader &reader, Sample &s) {
  const uint32_t interval = now - last_attempt_us_;
  if (started_) {
    const uint32_t elapsed = now - phase_us_;
    if (elapsed < 1000)
      return false;
    const uint32_t periods = elapsed / 1000;
    stats_.skipped_periods += periods - 1;
    const uint32_t lateness = elapsed - 1000;
    if (lateness > stats_.max_lateness_us)
      stats_.max_lateness_us = lateness;
    phase_us_ += periods * 1000;
    if (interval > stats_.max_interval_us)
      stats_.max_interval_us = interval;
    if (interval > 1000)
      ++stats_.deadline_misses;
  } else {
    phase_us_ = now;
  }
  started_ = true;
  last_attempt_us_ = now;
  ++stats_.attempts;
  s = {};
  s.sequence = sequence_++;
  const uint32_t begin = reader.nowUs();
  for (uint8_t i = 0; i < kChannels; ++i) {
    if (!reader.read12(i, s.raw[i]) || s.raw[i] > 4095) {
      s.ok = false;
      break;
    }
    s.channel_offset_us[i] = reader.nowUs() - begin;
  }
  const uint32_t end = reader.nowUs();
  s.span_us = end - begin;
  s.timestamp_us =
      s.ok ? begin + (s.channel_offset_us[0] + s.channel_offset_us[2]) / 2
           : begin + s.span_us / 2;
  // 超过预算的混合时刻向量不用于定位/校准；250us是起始预算，需实板测量。
  if (s.span_us > 250)
    s.ok = false;
  if (!s.ok)
    ++stats_.errors;
  if (s.span_us > stats_.max_span_us)
    stats_.max_span_us = s.span_us;
  const uint32_t service = reader.nowUs() - begin;
  if (service > stats_.max_service_us)
    stats_.max_service_us = service;
  return true;
}
} // namespace hall
} // namespace fluxrail
