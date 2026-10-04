#pragma once
#include "HallTypes.h"

namespace fluxrail {
namespace hall {
enum class CalibrationStage : uint8_t {
  Idle,
  Baseline,
  WaitReference,
  Reference,
  WaitNode,
  NodeCapture,
  MapCheck,
  Sweep,
  Completed,
  Failed,
  Cancelled
};
enum class CalibrationError : uint8_t {
  None,
  MagnetNotRemoved,
  SamplingError,
  Saturated,
  Noisy,
  WeakReference,
  InvalidMap,
  MissingCoverage,
  Timeout,
  Cancelled
};
struct CalibrationStatus {
  uint32_t job_id = 0;
  CalibrationStage stage = CalibrationStage::Idle;
  CalibrationError error = CalibrationError::None;
  uint16_t collected = 0, required = 0;
  uint8_t next_reference = 0, nodes = 0;
  uint32_t sweep_segments = 0;
  bool sweep_left = false, sweep_right = false;
};
// 候选校准与正在用的有效校准分离。仅核心1调用，通过公共命令队列控制。
// 参考点由用户确认磁铁在相应传感器上方，标签mm由实际测量提供。
class HallCalibration {
public:
  bool start(uint32_t job_id, uint32_t now_us, bool magnet_removed,
             float left_mm, float right_mm);
  bool captureReference(uint8_t channel, uint32_t now_us);
  bool captureNode(float actual_mm, uint32_t now_us);
  bool beginSweep(uint32_t now_us);
  bool finishSweep(uint32_t now_us);
  void sample(const Sample &sample);
  void tick(uint32_t now_us); // 即使ADC停采也必须调用，保证任务超时。
  void cancel();
  bool running() const;
  const CalibrationStatus &status() const { return status_; }
  const Calibration &candidate() const { return candidate_; }

private:
  void fail(CalibrationError error);
  void beginCapture(CalibrationStage stage, uint16_t count);
  void finishCapture();
  Calibration candidate_{};
  CalibrationStatus status_{};
  float mean_[kChannels]{}, m2_[kChannels]{}, reference_[kChannels]{};
  float node_mm_ = 0;
  uint32_t started_us_ = 0, last_sequence_ = 0, last_sample_us_ = 0;
  bool have_sample_ = false;
};
} // namespace hall
} // namespace fluxrail
