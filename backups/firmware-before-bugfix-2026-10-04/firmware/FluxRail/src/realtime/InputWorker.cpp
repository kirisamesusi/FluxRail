#include "InputWorker.h"
#include "../lighting/KeyLighting.h"

namespace fluxrail {
namespace realtime {
namespace {
bool valid(const common::RuntimeConfig &config) {
  return keyboard::validConfig(config.keyboard) &&
         lighting::validLightingConfig(config.lighting) &&
         config.scan.press_debounce_us <= 20000 &&
         config.scan.release_debounce_us <= 20000;
}
} // namespace
void InputWorker::begin(uint16_t raw, uint32_t now) {
  physical_mask_ = raw & 0x03ff;
  scanner_.reset(config_.scan, physical_mask_, now, config_.revision);
  initialized_ = true;
  paused_ = true;
  updatePhysical();
  bridge_.sample_heartbeat_us.store(now, std::memory_order_release);
}
void InputWorker::updatePhysical() {
  physical_mask_ = (physical_mask_ & 0x0c00) | scanner_.physicalMask();
  bridge_.physical_mask.store(physical_mask_, std::memory_order_release);
}
bool InputWorker::tryEdge(uint8_t id, bool down, uint32_t timestamp,
                          uint32_t revision) {
  if (id >= common::kSourceCount)
    return false;
  const uint16_t bit = static_cast<uint16_t>(1u << id);
  physical_mask_ = down ? (physical_mask_ | bit) : (physical_mask_ & ~bit);
  bridge_.physical_mask.store(physical_mask_, std::memory_order_release);
  if (paused_)
    return true; // 维护中只保当前物理状态，绝不积累历史动作。
  const common::InputEdge edge{next_sequence_, timestamp, revision,
                               epoch_,         id,        down};
  if (bridge_.edges.tryPush(edge)) {
    ++next_sequence_;
    return true;
  }
  paused_ = true;
  ++overflows_;
  bridge_.producer_overflows.store(overflows_, std::memory_order_release);
  bridge_.producer_fault_epoch.store(epoch_, std::memory_order_release);
  return false;
}
bool InputWorker::submitPush(uint8_t id, bool down, uint32_t now) {
  if (!initialized_ || id < 10 || id >= 12)
    return false;
  const bool current = (physical_mask_ & (1u << id)) != 0;
  if (current == down)
    return true;
  return tryEdge(id, down, now, config_.revision);
}
void InputWorker::handle(const ConfigCommand &command, uint16_t raw,
                         uint32_t now) {
  if (command.op == ControlOp::PauseApply) {
    paused_ = true; // 即使配置拒绝，也不继续发送可能不一致的旧流。
    if (acquisition_)
      acquisition_->pause(now);
    const bool ok = valid(command.config);
    if (ok) {
      config_ = command.config;
      epoch_ = command.epoch;
      next_sequence_ = 0;
      scanner_.reset(config_.scan, raw, now, config_.revision);
      bridge_.producer_fault_epoch.store(0, std::memory_order_release);
      updatePhysical();
      pause_snapshot_mask_ = physical_mask_;
    }
    pending_reply_ = {command.config.revision, command.epoch,  now,
                      next_sequence_ - 1,      physical_mask_, ok};
  } else {
    const bool ok =
        paused_ && command.epoch == epoch_ &&
        command.config.revision == config_.revision &&
        !scanner_.needsResync() &&
        (!acquisition_ || acquisition_->resume(command.acquisition,
                                               config_.revision, epoch_, now));
    if (ok) {
      // USB恢复期间真实状态可能改变；只补当前mask相对暂停快照的净变化，
      // 不回放维护期间往返历史。发布完成后序号继续由本生产者赋号。
      paused_ = false;
      const uint16_t changed = physical_mask_ ^ pause_snapshot_mask_;
      const uint16_t current = physical_mask_;
      for (uint8_t id = 0; id < common::kSourceCount; ++id) {
        if ((changed & (1u << id)) &&
            !tryEdge(id, (current & (1u << id)) != 0, now, config_.revision))
          break;
      }
    }
    pending_reply_ = {config_.revision,   command.epoch,  now,
                      next_sequence_ - 1, physical_mask_, ok && !paused_};
  }
  have_reply_ = true;
}
void InputWorker::service(uint16_t raw, uint32_t now) {
  if (!initialized_)
    return;
  scanner_.sample(raw, now, *this);
  updatePhysical();
  bridge_.sample_heartbeat_us.store(now, std::memory_order_release);
  // 回复满时保留整个值，停止处理下一命令；每次最多处理一条命令和一条回复。
  if (have_reply_) {
    if (bridge_.config.replies.tryPush(pending_reply_))
      have_reply_ = false;
    return;
  }
  ConfigCommand command;
  if (bridge_.config.commands.tryPop(command))
    handle(command, raw, now);
}
} // namespace realtime
} // namespace fluxrail
