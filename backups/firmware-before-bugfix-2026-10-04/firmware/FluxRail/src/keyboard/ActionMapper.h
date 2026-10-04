#pragma once
#include "KeyboardTypes.h"

namespace fluxrail {
namespace keyboard {
enum class IngressResult : uint8_t {
  Accepted,
  Backpressure,
  StaleRevision,
  Invalid,
  NeedsResync
};
struct MapperStats {
  uint32_t accepted_edges = 0;
  uint32_t stale_edges = 0;
  uint32_t backpressure = 0;
  uint32_t sequence_faults = 0;
  uint32_t redundant_edges = 0;
  uint32_t keyboard_reports = 0;
  uint32_t mouse_reports = 0;
  uint32_t retriggers = 0;
  uint32_t discarded_on_reset = 0;
  uint32_t queue_high_water = 0;
};
class ActionMapper {
public:
  static constexpr uint8_t kCapacity = 64;
  // 只在核心0调用；不是跨核队列，也不创建USB/Flash/协议。
  // 首次启动、热改键、断连/维护/故障恢复均走reset，先发送全释放再重建snapshot。
  // 公共层先停旧生产者/清旧队列，核心1采样边界应用revision并ACK后才放行新边沿。
  // reset期间允许排入新revision事件，但只有恢复报告全部成功后才处理它们。
  bool reset(const KeyboardConfig &config, uint32_t revision,
             uint16_t physical_mask);
  IngressResult enqueue(uint8_t source_id, bool down, uint32_t sequence,
                        uint32_t timestamp_us, uint32_t revision);
  // 每次至多提交一份报告；没有delay、忙等、动态分配或USB栈调用。
  void service(uint32_t now_us, ActionSink &sink);
  bool recovering() const { return reset_step_ != 0; }
  bool needsResync() const { return needs_resync_; }
  bool idle() const {
    return count_ == 0 && phase_ == Phase::Idle && !recovering();
  }
  uint16_t processedMask() const { return held_mask_; }
  uint32_t revision() const { return revision_; }
  const MapperStats &stats() const { return stats_; }

private:
  struct Event {
    uint8_t source_id;
    bool down;
    uint32_t sequence;
    uint32_t timestamp_us;
  };
  enum class Phase : uint8_t { Idle, KeyboardUp, KeyboardFinal, MouseFinal };
  KeyboardState buildKeyboard(uint16_t mask) const;
  uint8_t buildMouse(uint16_t mask) const;
  void finishEvent();
  KeyboardConfig config_{};
  MapperStats stats_{};
  Event queue_[kCapacity]{};
  uint8_t head_ = 0, tail_ = 0, count_ = 0;
  uint16_t held_mask_ = 0, next_mask_ = 0, restore_mask_ = 0;
  uint32_t revision_ = 0, last_sequence_ = 0, up_accepted_at_ = 0;
  bool have_sequence_ = false, needs_resync_ = false, initialized_ = false;
  bool waiting_gap_ = false;
  Phase phase_ = Phase::Idle;
  uint8_t reset_step_ = 0;
  KeyboardState pending_keyboard_{};
  uint8_t pending_mouse_ = 0;
};
} // namespace keyboard
} // namespace fluxrail
