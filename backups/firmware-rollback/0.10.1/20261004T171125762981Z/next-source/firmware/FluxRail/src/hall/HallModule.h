#pragma once
#include "../realtime/SpscQueue.h"
#include "HallCalibration.h"
#include "HallPosition.h"
#include <atomic>

namespace fluxrail {
namespace hall {
// 模块边界适配器，使用现有公共SPSC实现；没有USB/Serial/Flash/第二个loop。
// 64条仅保存采样结果的有界时间序列。公共层以后冻结MotionRecord时由适配器转换。
// 核心1: apply/load/resume/pause/sample/tick/task/statistics/latest。
// 核心0: tryTake/paused/recoveryNeeded/discardPaused。不能跨核直接读诊断引用。
class HallModule {
public:
  bool apply(const Settings &settings,
             uint32_t revision); // 仅暂停且清队列后采样边界应用。
  bool load(const Calibration &calibration); // 启动复用，不采空场。
  void pause();
  bool resume(uint32_t new_epoch,
              uint32_t discarded = 0); // 必须使用公共层新epoch。
  void sample(const Sample &sample);
  void tick(uint32_t now_us) { task_.tick(now_us); }
  // start需先获暂停ACK/清旧队列，成功后可resume新epoch，校准中发布invalid。
  bool startCalibration(uint32_t job_id, uint32_t now_us, bool magnet_removed,
                        float left_mm, float right_mm);
  bool captureReference(uint8_t channel, uint32_t now_us) {
    return task_.captureReference(channel, now_us);
  }
  bool captureNode(float mm, uint32_t now_us) {
    return task_.captureNode(mm, now_us);
  }
  bool beginSweep(uint32_t now_us) { return task_.beginSweep(now_us); }
  bool finishSweep(uint32_t now_us) { return task_.finishSweep(now_us); }
  void cancelCalibration() { task_.cancel(); }
  const CalibrationStatus &calibrationStatus() const { return task_.status(); }
  const Estimate &latest() const { return latest_; }
  const Statistics &statistics() const { return stats_; }
  const Calibration &calibration() const { return position_.calibration(); }
  bool tryTake(Estimate &estimate) {
    return !paused() && queue_.tryPop(estimate);
  }
  bool paused() const { return paused_.load(std::memory_order_acquire); }
  bool recoveryNeeded() const {
    return recovery_.load(std::memory_order_acquire);
  }
  // 核心0获公共暂停ACK后调用，清旧结果并返回明确丢弃量；不可在采样仍发布时调用。
  uint32_t discardPaused();

private:
  HallPosition position_{};
  HallCalibration task_{};
  realtime::SpscQueue<Estimate, 64> queue_{};
  std::atomic<bool> paused_{true}, recovery_{false};
  Statistics stats_{};
  Estimate latest_{};
  uint32_t revision_ = 0, epoch_ = 0, last_us_ = 0, last_sequence_ = 0,
           last_valid_us_ = 0;
  bool sampled_ = false, valid_seen_ = false, candidate_applied_ = false;
};
} // namespace hall
} // namespace fluxrail
