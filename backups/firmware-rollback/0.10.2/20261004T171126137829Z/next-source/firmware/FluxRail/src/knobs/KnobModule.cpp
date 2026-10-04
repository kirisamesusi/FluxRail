#include "KnobModule.h"

namespace fluxrail {
namespace knobs {
bool KnobModule::apply(const Settings (&s)[kKnobCount], uint32_t revision) {
  if (!revision) return false;
  // 完整校验后原子应用，第二路畸形不得让第一路半更新。
  for (uint8_t i = 0; i < kKnobCount; ++i)
    if (!paused(i) || !drained_[i].load(std::memory_order_acquire) || !validate(s[i])) return false;
  for (uint8_t i = 0; i < kKnobCount; ++i) { motion_[i].apply(s[i], revision); push_[i].apply(s[i].push); }
  revision_ = revision;
  return true;
}
void KnobModule::pause(PushSink &sink, uint32_t now) {
  for (uint8_t i = 0; i < kKnobCount; ++i) {
    motion_[i].resetReference(); push_[i].reset();
    push_latest_[i].filtered_down = false;
    latest_[i].valid = false; latest_[i].delta_counts = 0; latest_[i].quality = Quality::Suspended;
    if (!sink.submitFilteredPush(static_cast<uint8_t>(common::SourceId::P1) + i, false, now))
      ++stats_.push_submit_failures;
    // 本方法同生产者顺序执行，release发布前不会再写旧记录。
    paused_[i].store(true, std::memory_order_release);
  }
}
bool KnobModule::resume(uint32_t epoch, const Discarded (&discarded)[kKnobCount]) {
  if (!revision_ || !epoch || epoch == epoch_) return false;
  for (uint8_t i = 0; i < kKnobCount; ++i)
    if (!paused(i) || !drained_[i].load(std::memory_order_acquire)) return false;
  epoch_ = epoch;
  for (uint8_t i = 0; i < kKnobCount; ++i) {
    stats_.recovery_discarded[i] += discarded[i].records;
    stats_.recovery_discarded_counts[i] += discarded[i].counts;
    stats_.recovery_discarded_absolute_counts[i] += discarded[i].absolute_counts;
    motion_[i].resetReference(); push_[i].reset();
    recovery_[i].store(false, std::memory_order_release);
    paused_[i].store(false, std::memory_order_release);
  }
  return true;
}
void KnobModule::sample(uint8_t id, const Sample &s, PushSink &sink) {
  if (id >= kKnobCount || !revision_) return;
  Motion result = motion_[id].sample(s, id, epoch_);
  // 暂停仍采角度作诊断，但不排历史。PUSH暂停时清参考并抑制，恢复需低电平再武装。
  if (paused(id)) {
    push_[id].reset(); result.delta_counts = 0; result.valid = false;
    result.quality = Quality::Suspended; ++stats_.paused_samples[id];
  }
  const bool down = paused(id) ? false : push_[id].sample(s, result.active);
  const Frame f = decode(s);
  push_latest_[id] = {s.timestamp_us, s.sequence, revision_, epoch_,
      static_cast<uint8_t>(static_cast<uint8_t>(common::SourceId::P1) + id),
      s.push_gpio, f.push, f.crc_ok, f.field_ok, down, result.active};
  // 每个样本提交当前过滤状态，公共层去重并赋十二路唯一序号；错误不重播DOWN。
  if (!sink.submitFilteredPush(static_cast<uint8_t>(common::SourceId::P1) + id, down, s.timestamp_us))
    ++stats_.push_submit_failures;
  latest_[id] = result;
  if (paused(id)) return;
  drained_[id].store(false, std::memory_order_relaxed);
  if (!queues_[id].tryPush(result)) {
    ++stats_.queue_full[id];
    // 只停止满队列这一路，另一轴继续服务；公共层随后pause/ACK/清队列/resume新epoch。
    // 本次未入队的运动是恢复丢弃，显式记录，不覆盖旧时间序列。
    ++stats_.recovery_discarded[id];
    stats_.recovery_discarded_counts[id] += result.delta_counts;
    stats_.recovery_discarded_absolute_counts[id] += result.delta_counts < 0 ? -result.delta_counts : result.delta_counts;
    motion_[id].resetReference(); push_[id].reset();
    // 遥测也必须反映本次主动UP/停流，不能保留过滤前的DOWN快照。
    push_latest_[id].filtered_down = false;
    latest_[id].valid = false; latest_[id].delta_counts = 0; latest_[id].quality = Quality::Suspended;
    if (!sink.submitFilteredPush(static_cast<uint8_t>(common::SourceId::P1) + id, false, s.timestamp_us))
      ++stats_.push_submit_failures;
    recovery_[id].store(true, std::memory_order_release);
    paused_[id].store(true, std::memory_order_release);
  }
}
bool KnobModule::paused(uint8_t id) const {
  return id >= kKnobCount || paused_[id].load(std::memory_order_acquire);
}
bool KnobModule::recoveryNeeded(uint8_t id) const {
  return id < kKnobCount && recovery_[id].load(std::memory_order_acquire);
}
bool KnobModule::tryTake(uint8_t id, Motion &m) {
  return id < kKnobCount && !paused(id) && queues_[id].tryPop(m);
}
Discarded KnobModule::discardPaused(uint8_t id) {
  Discarded dropped{};
  if (id >= kKnobCount || !paused(id)) return dropped;
  Motion m;
  // 生产者已ACK停止，该队列最多64条；有界读取以计净量和绝对量。
  for (uint32_t n = 0; n < 64 && queues_[id].tryPop(m); ++n) {
    ++dropped.records; dropped.counts += m.delta_counts;
    dropped.absolute_counts += m.delta_counts < 0 ? -m.delta_counts : m.delta_counts;
  }
  drained_[id].store(true, std::memory_order_release);
  return dropped;
}
} // namespace knobs
} // namespace fluxrail
