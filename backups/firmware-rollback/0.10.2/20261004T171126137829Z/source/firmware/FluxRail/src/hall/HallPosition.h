#pragma once
#include "HallTypes.h"

namespace fluxrail {
namespace hall {
// 所有方法仅采样核心1调用；settings/calibration由公共层采样边界传完整快照。
// load不读Flash，apply不回复CDC；成功后公共层才发送两核ACK。
class HallPosition {
public:
  bool apply(const Settings &settings, uint32_t revision, uint32_t epoch);
  bool load(const Calibration &calibration); // 失败不修改当前有效校准。
  void resetReference(); // 维护/失效恢复时清滤波和活动历史。
  Estimate process(const Sample &sample);
  const Calibration &calibration() const { return calibration_; }
  const Settings &settings() const { return settings_; }

private:
  friend class HallModule;
  // 仅HallModule把已完成全套检查的任务候选复制进来；不对外开放绕过校验入口。
  void adoptCompleted(const Calibration &calibration);
  Settings settings_{};
  Calibration calibration_{};
  uint32_t revision_ = 0, epoch_ = 0, last_us_ = 0, last_sequence_ = 0,
           activity_us_ = 0;
  float filtered_mm_ = 0, last_raw_mm_ = 0, activity_anchor_ = 0;
  float held_mm_ = 0, held_p_ = 0, held_output_ = 0;
  bool referenced_ = false, field_valid_ = false, activity_seen_ = false;
};
} // namespace hall
} // namespace fluxrail
