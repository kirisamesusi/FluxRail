#pragma once
#include "HallTypes.h"

namespace fluxrail {
namespace hall {
// 小型硬件边界便于用原始C++模拟时序和失败；没有第二份ADC算法。
class AdcReader {
public:
  virtual ~AdcReader() = default;
  virtual uint32_t nowUs() const = 0;
  virtual bool read12(uint8_t channel, uint16_t &raw) = 0;
};
struct SamplerStatistics {
  uint32_t attempts = 0, errors = 0, deadline_misses = 0;
  uint32_t max_interval_us = 0, max_span_us = 0, max_service_us = 0;
};
class HallSampler {
public:
  // 一次poll最多一向量；过期不追赶补采，真实超期如实统计。
  bool poll(uint32_t now_us, AdcReader &reader, Sample &sample);
  const SamplerStatistics &statistics() const { return stats_; }

private:
  uint32_t last_attempt_us_ = 0, sequence_ = 0;
  bool started_ = false;
  SamplerStatistics stats_{};
};
} // namespace hall
} // namespace fluxrail
