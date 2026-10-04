#pragma once
#include "../keyboard/ActionMapper.h"
#include "../realtime/ConfigMailbox.h"

namespace fluxrail {
namespace app {
enum class ApplyPhase : uint8_t {
  Idle,
  QueuePause,
  WaitPause,
  Restore,
  QueueResume,
  WaitResume,
  MaintenanceReady,
  Failed
};
struct ApplyStats {
  uint32_t applied = 0, timeouts = 0, rejected = 0, stale_replies = 0,
           discarded_edges = 0;
};
// 唯一配置握手由核心0推进；不读Flash，不定义CDC/JSON，也不忙等核心1。
class ConfigCoordinator {
public:
  ConfigCoordinator(realtime::RealtimeBridge &bridge,
                    keyboard::ActionMapper &mapper)
      : bridge_(bridge), mapper_(mapper) {}
  // USB断连/传输失败可以替换正在进行的申请；新epoch拒绝所有旧ACK。
  // 普通配置请求保持默认false，忙时由上层等待/重试。
  bool start(const common::RuntimeConfig &config, uint32_t now_us,
             bool replace_in_flight = false);
  void setAcquisition(const common::AcquisitionConfig &config) {
    acquisition_ = config;
  }
  void setAcquisitionHook(realtime::AcquisitionHook *hook) { hook_ = hook; }
  bool startMaintenance(const common::RuntimeConfig &config, uint32_t now_us);
  bool maintenanceReady() const {
    return phase_ == ApplyPhase::MaintenanceReady;
  }
  bool finishMaintenance(uint32_t now_us);
  void service(uint32_t now_us, keyboard::ActionSink &sink);
  bool busy() const {
    return phase_ != ApplyPhase::Idle && phase_ != ApplyPhase::Failed;
  }
  bool failed() const { return phase_ == ApplyPhase::Failed; }
  bool hasApplied() const { return stats_.applied > 0; }
  bool ready() const {
    return phase_ == ApplyPhase::Idle && stats_.applied > 0;
  }
  uint32_t epoch() const { return epoch_; }
  const common::RuntimeConfig &appliedConfig() const { return applied_; }
  const ApplyStats &stats() const { return stats_; }

private:
  realtime::RealtimeBridge &bridge_;
  keyboard::ActionMapper &mapper_;
  common::RuntimeConfig requested_{}, applied_{};
  common::AcquisitionConfig acquisition_{};
  realtime::AcquisitionHook *hook_ = nullptr;
  uint32_t epoch_ = 0, start_us_ = 0;
  ApplyPhase phase_ = ApplyPhase::Idle;
  ApplyStats stats_{};
  bool maintenance_ = false;
  uint16_t pause_mask_ = 0;
  uint8_t physical_step_ = 0;
};
} // namespace app
} // namespace fluxrail
