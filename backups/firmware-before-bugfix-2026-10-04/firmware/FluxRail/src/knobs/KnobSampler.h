#pragma once
#include "KnobTypes.h"

namespace fluxrail {
namespace knobs {
// 注入真实驱动或模拟记录。每次read必须有界，失败仍返回尝试结果。
class Reader {
public:
  virtual ~Reader() = default;
  virtual bool read(uint8_t knob_id, Sample &sample) = 0;
};
struct SamplerStats {
  uint32_t attempts[kKnobCount]{}, skipped_periods[kKnobCount]{};
  uint32_t max_poll_gap_us[kKnobCount]{};
};
class KnobSampler {
public:
  // 核心1每轮分别poll(0)和poll(1)，禁止早退导致另一轴饿死。
  // 不补采过期时刻；采样时间来自read的真实帧中点。
  bool poll(uint8_t knob_id, uint32_t now_us, Reader &reader, Sample &out);
  void reset();
  const SamplerStats &stats() const { return stats_; }
private:
  uint32_t last_us_[kKnobCount]{}, next_sequence_[kKnobCount]{};
  bool started_[kKnobCount]{};
  SamplerStats stats_{};
};
} // namespace knobs
} // namespace fluxrail
