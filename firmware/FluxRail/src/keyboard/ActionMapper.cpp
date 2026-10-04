#include "ActionMapper.h"

namespace fluxrail {
namespace keyboard {
namespace {
bool equal(const KeyboardState &a, const KeyboardState &b) {
  if (a.modifiers != b.modifiers)
    return false;
  for (uint8_t i = 0; i < 28; ++i)
    if (a.keys[i] != b.keys[i])
      return false;
  return true;
}
void setUsage(KeyboardState &state, uint8_t usage) {
  if (usage < 0xe0)
    state.keys[usage >> 3] |= static_cast<uint8_t>(1u << (usage & 7));
  else
    state.modifiers |= static_cast<uint8_t>(1u << (usage - 0xe0));
}
void clearUsage(KeyboardState &state, uint8_t usage) {
  if (usage < 0xe0)
    state.keys[usage >> 3] &= static_cast<uint8_t>(~(1u << (usage & 7)));
  else
    state.modifiers &= static_cast<uint8_t>(~(1u << (usage - 0xe0)));
}
} // namespace
KeyboardState ActionMapper::buildKeyboard(uint16_t mask) const {
  KeyboardState state;
  for (uint8_t id = 0; id < kSourceCount; ++id) {
    const auto &b = config_.bindings[id];
    if (!(mask & (1u << id)) || b.type != ActionType::Keyboard)
      continue;
    setUsage(state, b.usage);
    state.modifiers |= b.modifiers;
  }
  return state;
}
uint8_t ActionMapper::buildMouse(uint16_t mask) const {
  uint8_t buttons = 0;
  for (uint8_t id = 0; id < kSourceCount; ++id) {
    const auto &b = config_.bindings[id];
    if ((mask & (1u << id)) && b.type == ActionType::MouseButton)
      buttons |= b.button;
  }
  return buttons; // D02未冻结：首版采用来源OR持有，不对重复按钮自动重触发。
}
bool ActionMapper::reset(const KeyboardConfig &config, uint32_t revision,
                         uint16_t mask) {
  if (!validConfig(config) || (mask & ~kSourceMask))
    return false;
  stats_.discarded_on_reset += count_;
  config_ = config;
  revision_ = revision;
  restore_mask_ = mask;
  held_mask_ = 0;
  head_ = tail_ = count_ = 0;
  phase_ = Phase::Idle;
  waiting_gap_ = false;
  have_sequence_ = false;
  needs_resync_ = false;
  initialized_ = true;
  reset_step_ = 1;
  return true;
}
IngressResult ActionMapper::enqueue(uint8_t id, bool down, uint32_t sequence,
                                    uint32_t timestamp, uint32_t revision) {
  if (!initialized_ || id >= kSourceCount)
    return IngressResult::Invalid;
  if (revision != revision_) {
    ++stats_.stale_edges;
    return IngressResult::StaleRevision;
  }
  if (needs_resync_)
    return IngressResult::NeedsResync;
  // 一条公共流必须连续；多个生产者要先由核心1合并编号。序号本身允许32位回绕。
  if (have_sequence_ && sequence != last_sequence_ + 1u) {
    needs_resync_ = true;
    ++stats_.sequence_faults;
    return IngressResult::NeedsResync;
  }
  if (count_ == kCapacity) {
    ++stats_.backpressure;
    return IngressResult::Backpressure;
  }
  queue_[tail_] = {id, down, sequence, timestamp};
  tail_ = static_cast<uint8_t>((tail_ + 1u) % kCapacity);
  ++count_;
  if (count_ > stats_.queue_high_water)
    stats_.queue_high_water = count_;
  last_sequence_ = sequence;
  have_sequence_ = true;
  ++stats_.accepted_edges;
  return IngressResult::Accepted;
}
void ActionMapper::finishEvent() {
  held_mask_ = next_mask_;
  head_ = static_cast<uint8_t>((head_ + 1u) % kCapacity);
  --count_;
  phase_ = Phase::Idle;
  waiting_gap_ = false;
}
void ActionMapper::service(uint32_t now, ActionSink &sink) {
  if (!initialized_)
    return;
  if (reset_step_ != 0) {
    // 不因USB忙提前宣布应用完成。四份报告保序释放旧状态、再恢复真实状态。
    const KeyboardState released{};
    bool ok = false;
    switch (reset_step_) {
    case 1:
      ok = sink.tryKeyboard(released);
      if (ok)
        ++stats_.keyboard_reports;
      break;
    case 2:
      ok = sink.tryMouseButtons(0);
      if (ok)
        ++stats_.mouse_reports;
      break;
    case 3:
      ok = sink.tryKeyboard(buildKeyboard(restore_mask_));
      if (ok)
        ++stats_.keyboard_reports;
      break;
    case 4:
      ok = sink.tryMouseButtons(buildMouse(restore_mask_));
      if (ok)
        ++stats_.mouse_reports;
      break;
    }
    if (ok && ++reset_step_ == 5) {
      reset_step_ = 0;
      held_mask_ = restore_mask_;
    }
    return;
  }
  if (needs_resync_)
    return; // 公共层必须显式调用reset，不能补发损坏事件流。
  if (phase_ == Phase::Idle) {
    if (count_ == 0)
      return;
    const auto &e = queue_[head_];
    const uint16_t bit = static_cast<uint16_t>(1u << e.source_id);
    if (((held_mask_ & bit) != 0) == e.down) {
      ++stats_.redundant_edges;
      next_mask_ = held_mask_;
      finishEvent();
      return; // 重复消息不是新的物理DOWN。
    }
    next_mask_ = e.down ? (held_mask_ | bit) : (held_mask_ & ~bit);
    const auto &binding = config_.bindings[e.source_id];
    if (binding.type == ActionType::Disabled) {
      finishEvent();
      return;
    }
    if (binding.type == ActionType::MouseButton) {
      pending_mouse_ = buildMouse(next_mask_);
      if (pending_mouse_ == buildMouse(held_mask_)) {
        finishEvent();
        return;
      }
      phase_ = Phase::MouseFinal;
    } else {
      const auto before = buildKeyboard(held_mask_);
      pending_keyboard_ = buildKeyboard(next_mask_);
      if (e.down && before.held(binding.usage)) {
        // 只撤销本次目标usage；其他键及组合修饰保留。修饰键作为目标时
        // 只清对应一位，重按将影响依赖它的主机组合键，属于其真实语义。
        // UP阶段严格沿用旧修饰状态，新来源的组合修饰只在随后DOWN生效。
        pending_keyboard_ = before;
        clearUsage(pending_keyboard_, binding.usage);
        phase_ = Phase::KeyboardUp;
      } else {
        if (equal(before, pending_keyboard_)) {
          finishEvent();
          return;
        }
        phase_ = Phase::KeyboardFinal;
      }
    }
  }
  if (phase_ == Phase::KeyboardUp) {
    if (sink.tryKeyboard(pending_keyboard_)) {
      ++stats_.keyboard_reports;
      ++stats_.retriggers;
      up_accepted_at_ = now;
      waiting_gap_ = true;
      pending_keyboard_ = buildKeyboard(next_mask_);
      phase_ = Phase::KeyboardFinal;
    }
    return; // 同一次调用绝不发送DOWN，即使gap=0也保留独立报告。
  }
  if (phase_ == Phase::KeyboardFinal) {
    if (waiting_gap_ && now - up_accepted_at_ < config_.retrigger_gap_us)
      return;
    if (sink.tryKeyboard(pending_keyboard_)) {
      ++stats_.keyboard_reports;
      finishEvent();
    }
  } else if (phase_ == Phase::MouseFinal) {
    if (sink.tryMouseButtons(pending_mouse_)) {
      ++stats_.mouse_reports;
      finishEvent();
    }
  }
}
} // namespace keyboard
} // namespace fluxrail
