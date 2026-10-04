#pragma once
#include "../hall/HallCalibration.h"
#include "../hall/HallSampler.h"
#include "../knobs/KnobModule.h"
#include "../knobs/KnobSampler.h"
#include "DeviceConfig.h"
namespace fluxrail {
namespace common {
// 运行采集投影只经控制邮箱复制，不独立序列化/保存。所有单位复用原模块。
struct AcquisitionConfig {
  hall::Settings hall{};
  knobs::Settings knobs[2]{};
  hall::Calibration calibration{};
  uint32_t calibration_generation = 0;
};
inline AcquisitionConfig acquisitionConfig(const DeviceConfig &device) {
  AcquisitionConfig out;
  for (uint8_t i = 0; i < device.profile_count; ++i)
    if (device.profiles[i].id == device.active_id) {
      out.hall = device.profiles[i].hall;
      for (uint8_t k = 0; k < 2; ++k)
        out.knobs[k] = device.profiles[i].knobs[k];
      break;
    }
  out.calibration = device.calibration;
  out.calibration_generation = device.calibration_generation;
  return out;
}
// 低频值快照由核心1唯一写入，核心0只取队列副本，绝不读取模块诊断引用。
struct MotionSnapshot {
  uint32_t timestamp_us = 0, revision = 0, epoch = 0, telemetry_dropped = 0;
  hall::Sample raw{};
  hall::Estimate hall{};
  hall::Statistics hall_stats{};
  hall::SamplerStatistics adc_stats{};
  hall::CalibrationStatus calibration{};
  knobs::Motion knobs[2]{};
  knobs::PushSnapshot push[2]{};
  knobs::MotionStats knob_stats[2]{};
  knobs::SamplerStats sampler_stats{};
  uint32_t core1_max_service_us = 0;
  bool adc_started = false, ssi_started = false;
};
enum class CalibrationOp : uint8_t {
  Start,
  Reference,
  Node,
  BeginSweep,
  Finish,
  Cancel
};
// 控制邮箱值对象：核心0唯一生产、核心1唯一消费；不包含指针或引用。
// token是本次ACK关联号，job_id是人工校准会话号；mm均来自实际测量。
struct CalibrationCommand {
  CalibrationOp op = CalibrationOp::Cancel;
  uint32_t token = 0, job_id = 0;
  float left_mm = 0, right_mm = 0, mm = 0;
  uint8_t channel = 0;
  bool magnet_removed = false;
};
struct CalibrationReply {
  uint32_t token = 0;
  bool ok = false;
};
struct CalibrationResult {
  uint32_t job_id = 0;
  hall::Calibration calibration{};
};
} // namespace common
} // namespace fluxrail
