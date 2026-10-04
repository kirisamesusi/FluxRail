#pragma once
#include "KnobTypes.h"

namespace fluxrail {
namespace knobs {
// 纯核心1状态机。短CRC间隙只在转速*有效dt<半圈的可观察边界内补差。
// 任何场异常/序号缺口/长间隔立即丢参考，恢复第一帧只定位，不补历史。
class KnobMotion {
public:
  bool apply(const Settings &settings, uint32_t revision);
  void resetReference(); // 维护/断连/来源恢复，累计诊断圈数保留。
  Motion sample(const Sample &sample, uint8_t knob_id, uint32_t epoch);
  const MotionStats &stats() const { return stats_; }
private:
  Settings settings_{};
  MotionStats stats_{};
  int64_t total_ticks_ = 0, activity_anchor_ = 0;
  double scaled_turns_ = 0, filter_tail_ = 0;
  uint32_t revision_ = 0, last_sample_us_ = 0, last_sequence_ = 0;
  uint32_t last_valid_us_ = 0, activity_us_ = 0;
  uint16_t last_angle_ = 0;
  bool sampled_ = false, reference_ = false, active_ = false, valid_seen_ = false;
};
} // namespace knobs
} // namespace fluxrail
