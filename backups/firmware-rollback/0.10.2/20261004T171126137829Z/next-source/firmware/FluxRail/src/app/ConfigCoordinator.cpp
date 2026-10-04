#include "ConfigCoordinator.h"
#include "../board/BoardConfig.h"
#include "../lighting/KeyLighting.h"

namespace fluxrail {
namespace app {
bool ConfigCoordinator::start(const common::RuntimeConfig &c, uint32_t now,
                              bool replace_in_flight) {
  if ((busy() && !replace_in_flight) || !keyboard::validConfig(c.keyboard) ||
      !lighting::validLightingConfig(c.lighting) ||
      c.scan.press_debounce_us > 20000 || c.scan.release_debounce_us > 20000)
    return false;
  maintenance_ = false;
  requested_ = c;
  start_us_ = now;
  ++epoch_;
  if (epoch_ == 0)
    ++epoch_; // 0保留为未启动/无故障标记。
  phase_ = ApplyPhase::QueuePause;
  return true;
}
bool ConfigCoordinator::startMaintenance(const common::RuntimeConfig &c,
                                         uint32_t now) {
  if (!ready() || !start(c, now))
    return false;
  maintenance_ = true;
  return true;
}
bool ConfigCoordinator::finishMaintenance(uint32_t now) {
  if (!maintenanceReady() ||
      !mapper_.reset(requested_.keyboard, requested_.revision, pause_mask_))
    return false;
  maintenance_ = false;
  start_us_ = now;
  physical_step_ = 1;
  phase_ = ApplyPhase::Restore;
  return true;
}
void ConfigCoordinator::service(uint32_t now, keyboard::ActionSink &sink) {
  if (!busy())
    return;
  if (now - start_us_ > board::kConfigTimeoutUs) {
    phase_ = ApplyPhase::Failed;
    ++stats_.timeouts;
    return;
  }
  if (maintenanceReady())
    return;
  if (phase_ == ApplyPhase::QueuePause || phase_ == ApplyPhase::QueueResume) {
    const bool pause = phase_ == ApplyPhase::QueuePause;
    const realtime::ConfigCommand command{
        pause ? realtime::ControlOp::PauseApply : realtime::ControlOp::Resume,
        requested_, epoch_, acquisition_};
    if (bridge_.config.commands.tryPush(command))
      phase_ = pause ? ApplyPhase::WaitPause : ApplyPhase::WaitResume;
    return;
  }
  if (phase_ == ApplyPhase::Restore) {
    bool restored = false;
    if (sink.usesPhysical()) {
      if (physical_step_ == 1 && sink.tryPhysicalState(0))
        physical_step_ = 2;
      else if (physical_step_ == 2 &&
               sink.tryPhysicalState(maintenance_ ? 0 : pause_mask_))
        physical_step_ = 3;
      restored = physical_step_ == 3;
    } else {
      mapper_.service(now, sink);
      restored = !mapper_.recovering();
    }
    if (restored && sink.outputsComplete())
      phase_ =
          maintenance_ ? ApplyPhase::MaintenanceReady : ApplyPhase::QueueResume;
    return;
  }
  common::InputSnapshot reply;
  if (!bridge_.config.replies.tryPop(reply))
    return;
  if (reply.epoch != epoch_ || reply.revision != requested_.revision) {
    ++stats_.stale_replies;
    return;
  }
  if (!reply.ok) {
    phase_ = ApplyPhase::Failed;
    ++stats_.rejected;
    return;
  }
  if (phase_ == ApplyPhase::WaitPause) {
    // 只有生产者暂停ACK到达后，消费者才能清旧事件/重置映射；epoch隔离重连。
    stats_.discarded_edges += bridge_.edges.clearConsumer();
    if (hook_)
      hook_->discardPaused();
    pause_mask_ = reply.physical_mask;
    if (!mapper_.reset(requested_.keyboard, requested_.revision,
                       maintenance_ ? 0 : reply.physical_mask)) {
      phase_ = ApplyPhase::Failed;
      ++stats_.rejected;
      return;
    }
    physical_step_ = 1;
    phase_ = ApplyPhase::Restore;
  } else if (phase_ == ApplyPhase::WaitResume) {
    applied_ = requested_;
    phase_ = ApplyPhase::Idle;
    ++stats_.applied;
  }
}
} // namespace app
} // namespace fluxrail
