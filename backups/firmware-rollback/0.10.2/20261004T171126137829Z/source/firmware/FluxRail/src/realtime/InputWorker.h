#pragma once
#include "../keyboard/KeyScanner.h"
#include "ConfigMailbox.h"

namespace fluxrail {
namespace realtime {
// 纯状态机由App核心1提供raw与时间；不建立第二个setup/loop或操作USB。
class InputWorker : private keyboard::EdgeSink {
public:
  explicit InputWorker(RealtimeBridge &bridge) : bridge_(bridge) {}
  void begin(uint16_t key_down_mask, uint32_t now_us);
  void service(uint16_t key_down_mask, uint32_t now_us);
  // 模块3以后在同一核心1调度中提交过滤后的PUSH；本层不初始化PUSH GPIO。
  bool submitPush(uint8_t source_id, bool down, uint32_t now_us);
  void setAcquisitionHook(AcquisitionHook *hook) { acquisition_ = hook; }
  bool paused() const { return paused_; }
  const keyboard::ScanStats &scanStats() const { return scanner_.stats(); }

private:
  bool tryEdge(uint8_t source_id, bool down, uint32_t timestamp_us,
               uint32_t revision) override;
  void updatePhysical();
  void handle(const ConfigCommand &command, uint16_t raw, uint32_t now);
  RealtimeBridge &bridge_;
  AcquisitionHook *acquisition_ = nullptr;
  keyboard::KeyScanner scanner_;
  common::RuntimeConfig config_{};
  common::InputSnapshot pending_reply_{};
  uint32_t epoch_ = 0, next_sequence_ = 0, overflows_ = 0;
  uint16_t physical_mask_ = 0, pause_snapshot_mask_ = 0;
  bool paused_ = true, have_reply_ = false, initialized_ = false;
};
} // namespace realtime
} // namespace fluxrail
