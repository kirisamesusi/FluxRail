#include "HallModule.h"

namespace fluxrail {
namespace hall {
bool HallModule::apply(const Settings &s, uint32_t revision) {
  if (!paused() || task_.running())
    return false;
  if (!position_.apply(s, revision, epoch_))
    return false;
  revision_ = revision;
  return true;
}
bool HallModule::load(const Calibration &c) {
  return paused() && !task_.running() && position_.load(c);
}
bool HallModule::startCalibration(uint32_t id, uint32_t now, bool removed,
                                  float left, float right) {
  if (!paused() || !task_.start(id, now, removed, left, right))
    return false;
  candidate_applied_ = false;
  position_.resetReference();
  return true;
}
void HallModule::pause() {
  // 本方法只由生产核心1执行。release发生于最后一次发布之后，配合公共ACK。
  position_.resetReference();
  paused_.store(true, std::memory_order_release);
}
uint32_t HallModule::discardPaused() {
  return paused() ? queue_.clearConsumer() : 0;
}
bool HallModule::resume(uint32_t epoch, uint32_t discarded) {
  if (!paused() || epoch == epoch_ ||
      !position_.apply(position_.settings(), revision_, epoch))
    return false;
  epoch_ = epoch;
  stats_.recovery_discarded += discarded;
  recovery_.store(false, std::memory_order_release);
  paused_.store(false, std::memory_order_release);
  return true;
}
void HallModule::sample(const Sample &s) {
  ++stats_.samples;
  if (sampled_) {
    const uint32_t dt = s.timestamp_us - last_us_;
    if (dt > stats_.max_interval_us)
      stats_.max_interval_us = dt;
    if (dt > 1000)
      ++stats_.deadline_misses; // 起始1kHz目标，仅统计，不宣称性能已达标。
    if (s.sequence - last_sequence_ != 1u)
      ++stats_.sequence_gaps;
  }
  sampled_ = true;
  last_us_ = s.timestamp_us;
  last_sequence_ = s.sequence;
  // 暂停发布仍可采集/诊断；校准不阻塞键盘/SSI，也不自行停另一核。
  task_.sample(s);
  if (task_.running())
    candidate_applied_ = false;
  if (task_.status().stage == CalibrationStage::Completed &&
      !candidate_applied_) {
    // 状态机已经逐周期做完整结构/歧义/扫覆盖检查；此处只复制，避免再次长验证。
    position_.adoptCompleted(task_.candidate());
    candidate_applied_ = true;
  }
  latest_ = position_.process(s);
  if (task_.running()) {
    latest_.valid = false;
    latest_.active = false;
    latest_.quality = Quality::Calibrating;
    position_.resetReference();
  }
  if (paused()) {
    latest_.valid = false;
    latest_.active = false;
    latest_.quality = Quality::Suspended;
    position_.resetReference();
  }
  if (latest_.valid) {
    if (valid_seen_) {
      const uint32_t interval = s.timestamp_us - last_valid_us_;
      if (interval > stats_.max_valid_interval_us)
        stats_.max_valid_interval_us = interval;
    }
    valid_seen_ = true;
    last_valid_us_ = s.timestamp_us;
  } else {
    ++stats_.invalid;
    switch (latest_.quality) {
    case Quality::Saturated:
      ++stats_.saturated;
      break;
    case Quality::WeakField:
      ++stats_.weak_field;
      break;
    case Quality::OffCurve:
      ++stats_.off_curve;
      break;
    case Quality::Ambiguous:
      ++stats_.ambiguous;
      break;
    case Quality::SamplingError:
      ++stats_.sampling_errors;
      break;
    default:
      break;
    }
  }
  if (paused())
    return;
  // 无效记录也传给来源路由；绝对坐标不可累计。队列满停流，禁止重放过时往返。
  if (!queue_.tryPush(latest_)) {
    ++stats_.queue_full;
    pause();
    recovery_.store(true, std::memory_order_release);
    latest_.valid = false;
    latest_.active = false;
    latest_.quality = Quality::Suspended;
  }
}
} // namespace hall
} // namespace fluxrail
