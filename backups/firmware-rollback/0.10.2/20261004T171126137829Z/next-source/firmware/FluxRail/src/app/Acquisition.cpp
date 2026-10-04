#include "Acquisition.h"
namespace fluxrail {
namespace app {
// 核心1暂停发布；PUSH释放通过同一个可靠输入所有者提交。
// 这一步完成后才允许PauseApply ACK，核心0不得仅凭模块paused自行清队列。
void Acquisition::pause(uint32_t now) {
  hall_.pause();
  knobs_.pause(*this, now);
}
// 核心0收到暂停ACK后调用。记录清理量给下一次Resume，
// mailbox的release/acquire把账本交还核心1，不跨核修改模块诊断对象。
void Acquisition::discardPaused() {
  hall_discarded_ = hall_.discardPaused();
  for (uint8_t i = 0; i < 2; ++i)
    knob_discarded_[i] = knobs_.discardPaused(i);
}
// 核心1执行Resume事务：参数合法、全局安装校准按generation更新、
// 两模块恢复到统一epoch后才成功ACK。失败维持暂停，公共Coordinator负责恢复。
bool Acquisition::resume(const common::AcquisitionConfig &c, uint32_t rev,
                         uint32_t epoch, uint32_t now) {
  if (!rev || !epoch || !hall::validateSettings(c.hall) ||
      !knobs::validate(c.knobs[0]) || !knobs::validate(c.knobs[1]))
    return false;
  const auto stage = hall_.calibrationStatus().stage;
  const bool running = stage != hall::CalibrationStage::Idle &&
                       stage != hall::CalibrationStage::Completed &&
                       stage != hall::CalibrationStage::Failed &&
                       stage != hall::CalibrationStage::Cancelled;
  // 校准或同revision恢复保留任务；配置修改在App被拒绝，不能隐式取消校准。
  if (running &&
      (rev != revision_ || c.calibration_generation != calibration_generation_))
    return false;
  if (!running) {
    // 模块load只接受有效校准。无校准时采集继续，但强制该来源禁用；
    // 旧对象可留作内部历史，绝不把失效/导入空校准当可输出的安装关系。
    auto effective = c.hall;
    if (!c.calibration.valid)
      effective.enabled = false;
    if (!hall_.apply(effective, rev))
      return false;
    // 开机及安装generation变化才完整load，不在每个暂停恢复重复全表校验。
    if ((!applied_ || c.calibration_generation != calibration_generation_) &&
        c.calibration.valid && !hall_.load(c.calibration))
      return false;
  }
  if (!knobs_.apply(c.knobs, rev) || !hall_.resume(epoch, hall_discarded_) ||
      !knobs_.resume(epoch, knob_discarded_)) {
    pause(now);
    return false;
  }
  revision_ = rev;
  epoch_ = epoch;
  calibration_generation_ = c.calibration_generation;
  applied_ = true;
  hall_discarded_ = 0;
  for (auto &d : knob_discarded_)
    d = {};
  knob_sampler_.reset();
  return true;
}
// 每轮最多消费一条核心0控制命令；回复队列忙时保留回复并停止取新命令。
// token对应请求，job_id对应校准任务；ACK只代表命令已被模块接受，
// 采样完成和合格候选通过独立状态快照/Result通道确认。
void Acquisition::control(uint32_t now) {
  if (reply_pending_) {
    if (replies_.tryPush(reply_))
      reply_pending_ = false;
    return;
  }
  common::CalibrationCommand c;
  if (!commands_.tryPop(c))
    return;
  bool ok = c.job_id && ((c.op == common::CalibrationOp::Start) ||
                         c.job_id == hall_.calibrationStatus().job_id);
  if (ok)
    switch (c.op) {
    case common::CalibrationOp::Start:
      ok = hall_.startCalibration(c.job_id, now, c.magnet_removed, c.left_mm,
                                  c.right_mm);
      break;
    case common::CalibrationOp::Reference:
      ok = hall_.captureReference(c.channel, now);
      break;
    case common::CalibrationOp::Node:
      ok = hall_.captureNode(c.mm, now);
      break;
    case common::CalibrationOp::BeginSweep:
      ok = hall_.beginSweep(now);
      break;
    case common::CalibrationOp::Finish:
      ok = hall_.finishSweep(now);
      break;
    case common::CalibrationOp::Cancel:
      hall_.cancelCalibration();
      break;
    }
  reply_ = {c.token, ok};
  reply_pending_ = true;
}
// 核心1合作式服务：模块Sampler各自管理周期和预算，不追赶补采。
// 诊断20Hz按值发布，队列满丢新快照并计数；诊断慢消费者不阻塞实时输入。
void Acquisition::service(uint32_t now, hall::AdcReader &adc,
                          knobs::Reader &ssi) {
  control(now);
  hall::Sample h;
  const bool hall_sampled = hall_sampler_.poll(adc.nowUs(), adc, h);
  if (hall_sampled) {
    raw_ = h;
    hall_.sample(h);
  }
  hall_.tick(adc.nowUs()); // 无数据也保证异步校准超时。
  for (uint8_t i = 0; i < 2; ++i) {
    knobs::Sample k;
    // 使用完成前一路工作的真实时钟，避免ADC/投影耗时污染SSI到期判断。
    if (knob_sampler_.poll(i, adc.nowUs(), ssi, k))
      knobs_.sample(i, k, *this);
  }
  // Completed命令尚未采用时不能发布候选；只在真实sample已替换后复制有效结果。
  const auto &status = hall_.calibrationStatus();
  if (hall_sampled && status.stage == hall::CalibrationStage::Completed &&
      status.job_id != result_job_ && hall_.calibration().valid) {
    result_ = {status.job_id, hall_.calibration()};
    result_job_ = status.job_id;
    result_pending_ = true;
  }
  if (result_pending_ && results_.tryPush(result_))
    result_pending_ = false;
  const uint32_t snapshot_now = adc.nowUs();
  if (snapshot_now - last_snapshot_us_ < 50000)
    return;
  last_snapshot_us_ = snapshot_now;
  snapshot_ = {};
  snapshot_.timestamp_us = snapshot_now;
  snapshot_.revision = revision_;
  snapshot_.epoch = epoch_;
  snapshot_.raw = raw_;
  snapshot_.hall = hall_.latest();
  snapshot_.hall_stats = hall_.statistics();
  snapshot_.adc_stats = hall_sampler_.statistics();
  snapshot_.calibration = status;
  for (uint8_t i = 0; i < 2; ++i) {
    snapshot_.knobs[i] = knobs_.latest(i);
    snapshot_.push[i] = knobs_.latestPush(i);
    snapshot_.knob_stats[i] = knobs_.motionStats(i);
  }
  snapshot_.sampler_stats = knob_sampler_.stats();
  snapshot_.telemetry_dropped = dropped_;
  snapshot_.core1_max_service_us = max_service_us_;
  snapshot_.adc_started = adc_started_;
  snapshot_.ssi_started = ssi_started_;
  if (!snapshots_.tryPush(snapshot_))
    ++dropped_;
}
} // namespace app
} // namespace fluxrail
