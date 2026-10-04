#include "KeyScanner.h"

namespace fluxrail {
namespace keyboard {
bool KeyScanner::reset(const ScanConfig &c, uint16_t raw, uint32_t now,
                       uint32_t revision) {
  if (c.press_debounce_us > 20000 || c.release_debounce_us > 20000)
    return false;
  config_ = c;
  stable_mask_ = candidate_mask_ = raw & 0x03ff;
  for (auto &timestamp : candidate_since_)
    timestamp = now;
  last_sample_us_ = now;
  revision_ = revision;
  initialized_ = true;
  needs_resync_ = false;
  // 诊断累计值在reset时保留，便于维护/溢出后追踪原故障。
  return true;
}
void KeyScanner::sample(uint16_t raw, uint32_t now, EdgeSink &sink) {
  if (!initialized_)
    return;
  raw &= 0x03ff;
  const uint32_t gap =
      now - last_sample_us_; // 无符号差支持micros()约71分钟回绕。
  if (gap > stats_.max_scan_gap_us)
    stats_.max_scan_gap_us = gap;
  last_sample_us_ = now;
  ++stats_.samples;
  for (uint8_t id = 0; id < kKeyCount; ++id) {
    const uint16_t bit = static_cast<uint16_t>(1u << id);
    const bool down = (raw & bit) != 0;
    if (down != ((candidate_mask_ & bit) != 0)) {
      candidate_mask_ ^= bit;
      candidate_since_[id] = now;
    }
    if (down == ((stable_mask_ & bit) != 0))
      continue;
    const uint32_t elapsed = now - candidate_since_[id];
    const uint32_t threshold =
        down ? config_.press_debounce_us : config_.release_debounce_us;
    if (elapsed < threshold)
      continue;
    stable_mask_ ^= bit;
    if (elapsed > stats_.max_edge_delay_us)
      stats_.max_edge_delay_us = elapsed;
    // 满队列是显式故障：仍扫描以保持最新真实状态，但停止整个流的后续边沿。
    // 公共层应停生产者、丢旧队列、全释放并拿12路快照恢复；不得伪称没丢边沿。
    if (!needs_resync_) {
      if (sink.tryEdge(id, down, candidate_since_[id], revision_))
        ++stats_.emitted_edges;
      else {
        needs_resync_ = true;
        ++stats_.overflow_faults;
      }
    }
  }
}
} // namespace keyboard
} // namespace fluxrail
