#pragma once
#include "KeyboardTypes.h"

namespace fluxrail {
namespace keyboard {
using common::ScanConfig;
struct ScanStats {
  uint32_t samples = 0;
  uint32_t emitted_edges = 0;
  uint32_t overflow_faults = 0;
  uint32_t max_scan_gap_us = 0;
  uint32_t max_edge_delay_us = 0;
};
class EdgeSink {
public:
  virtual ~EdgeSink() = default;
  // 公共输入生产者负责给BT与PUSH共同分配全局sequence，并发布可靠SPSC队列。
  virtual bool tryEdge(uint8_t source_id, bool down, uint32_t timestamp_us,
                       uint32_t revision) = 0;
};
class KeyScanner {
public:
  // raw_down_mask中1代表按下；板适配器负责把GPIO低电平转成1。
  // 启动/配置切换时先以真实状态建立参考，由公共恢复快照重建，不伪造DOWN。
  bool reset(const ScanConfig &config, uint16_t raw_down_mask,
             uint32_t timestamp_us, uint32_t revision);
  void sample(uint16_t raw_down_mask, uint32_t timestamp_us, EdgeSink &sink);
  uint16_t physicalMask() const { return stable_mask_; }
  bool needsResync() const { return needs_resync_; }
  const ScanStats &stats() const { return stats_; }

private:
  ScanConfig config_{};
  ScanStats stats_{};
  uint32_t candidate_since_[kKeyCount]{};
  uint32_t revision_ = 0;
  uint32_t last_sample_us_ = 0;
  uint16_t stable_mask_ = 0;
  uint16_t candidate_mask_ = 0;
  bool initialized_ = false;
  bool needs_resync_ = false;
};
} // namespace keyboard
} // namespace fluxrail
