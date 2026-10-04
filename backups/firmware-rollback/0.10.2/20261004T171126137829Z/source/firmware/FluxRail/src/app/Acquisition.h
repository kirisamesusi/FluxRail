#pragma once
#include "../hall/HallModule.h"
#include "../realtime/InputWorker.h"
namespace fluxrail {
namespace app {
// 核心1拥有算法状态，核心0只访问已有SPSC消费者及控制ACK后的清理入口。
class Acquisition final : public realtime::AcquisitionHook,
                          public knobs::PushSink {
public:
  explicit Acquisition(realtime::InputWorker &input) : input_(input) {}
  void pause(uint32_t now) override;
  bool resume(const common::AcquisitionConfig &, uint32_t revision,
              uint32_t epoch, uint32_t now) override;
  void discardPaused() override;
  bool submitFilteredPush(uint8_t id, bool down, uint32_t now) override {
    return input_.submitPush(id, down, now);
  }
  // now与adc.nowUs及SSI帧时间戳必须来自同一32位微秒时钟。
  void service(uint32_t now, hall::AdcReader &adc, knobs::Reader &ssi);
  bool recoveryNeeded() const {
    return hall_.recoveryNeeded() || knobs_.recoveryNeeded(0) ||
           knobs_.recoveryNeeded(1);
  }
  bool takeHall(hall::Estimate &out) { return hall_.tryTake(out); }
  bool takeKnob(uint8_t id, knobs::Motion &out) {
    return knobs_.tryTake(id, out);
  }
  bool takeSnapshot(common::MotionSnapshot &out) {
    return snapshots_.tryPop(out);
  }
  bool command(const common::CalibrationCommand &c) {
    return commands_.tryPush(c);
  }
  bool takeReply(common::CalibrationReply &out) { return replies_.tryPop(out); }
  bool takeResult(common::CalibrationResult &out) {
    return results_.tryPop(out);
  }
  void hardwareStarted(bool adc, bool ssi) {
    adc_started_ = adc;
    ssi_started_ = ssi;
  }
  void recordServiceCost(uint32_t cost) {
    if (cost > max_service_us_)
      max_service_us_ = cost;
  }

private:
  realtime::InputWorker &input_;
  hall::HallModule hall_;
  hall::HallSampler hall_sampler_;
  knobs::KnobModule knobs_;
  knobs::KnobSampler knob_sampler_;
  realtime::SpscQueue<common::MotionSnapshot, 2> snapshots_;
  realtime::SpscQueue<common::CalibrationCommand, 4> commands_;
  realtime::SpscQueue<common::CalibrationReply, 4> replies_;
  realtime::SpscQueue<common::CalibrationResult, 2> results_;
  hall::Sample raw_{};
  // 核心0仅在暂停ACK后写，核心1仅在随后Resume命令acquire后读；邮箱提供同步。
  uint32_t hall_discarded_ = 0;
  knobs::Discarded knob_discarded_[2]{};
  uint32_t revision_ = 0, epoch_ = 0, calibration_generation_ = 0,
           last_snapshot_us_ = 0;
  uint32_t dropped_ = 0, max_service_us_ = 0, result_job_ = 0;
  bool applied_ = false, adc_started_ = false, ssi_started_ = false;
  common::MotionSnapshot snapshot_{};
  common::CalibrationResult result_{};
  common::CalibrationReply reply_{};
  bool reply_pending_ = false, result_pending_ = false;
  void control(uint32_t now);
};
} // namespace app
} // namespace fluxrail
