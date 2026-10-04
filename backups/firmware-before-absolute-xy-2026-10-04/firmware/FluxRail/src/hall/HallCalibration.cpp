#include "HallCalibration.h"
#include <math.h>

namespace fluxrail {
namespace hall {
bool HallCalibration::running() const {
  return status_.stage >= CalibrationStage::Baseline &&
         status_.stage <= CalibrationStage::Sweep;
}
void HallCalibration::fail(CalibrationError error) {
  candidate_.valid = false;
  status_.error = error;
  status_.stage = CalibrationStage::Failed;
}
void HallCalibration::beginCapture(CalibrationStage stage, uint16_t count) {
  status_.stage = stage;
  status_.collected = 0;
  status_.required = count;
  for (uint8_t i = 0; i < kChannels; ++i) {
    mean_[i] = 0;
    m2_[i] = 0;
  }
  have_sample_ = false;
}
bool HallCalibration::start(uint32_t id, uint32_t now, bool removed, float left,
                            float right) {
  if (running() || !id)
    return false;
  if (!removed) {
    status_ = {};
    status_.job_id = id;
    fail(CalibrationError::MagnetNotRemoved);
    return false;
  }
  if (!isfinite(left) || !isfinite(right) || left < -200 || left > 200 ||
      right < -200 || right > 200 || right - left < 1)
    return false;
  candidate_ = {};
  candidate_.left_mm = left;
  candidate_.right_mm = right;
  status_ = {};
  status_.job_id = id;
  started_us_ = now;
  beginCapture(CalibrationStage::Baseline, 256);
  return true;
}
bool HallCalibration::captureReference(uint8_t channel, uint32_t now) {
  tick(now);
  if (status_.stage != CalibrationStage::WaitReference ||
      channel != status_.next_reference)
    return false;
  beginCapture(CalibrationStage::Reference, 128);
  return true;
}
bool HallCalibration::captureNode(float mm, uint32_t now) {
  tick(now);
  if (status_.stage != CalibrationStage::WaitNode ||
      candidate_.node_count >= kMaxNodes || !isfinite(mm) ||
      mm < candidate_.left_mm || mm > candidate_.right_mm)
    return false;
  // 按左到右录入，错误标签不修改候选；不猜坐标、不自动拿80mm作行程。
  if ((!candidate_.node_count && magnitude(mm - candidate_.left_mm) > 0.01f) ||
      (candidate_.node_count &&
       mm - candidate_.nodes[candidate_.node_count - 1].mm < 0.5f))
    return false;
  node_mm_ = mm;
  beginCapture(CalibrationStage::NodeCapture, 128);
  return true;
}
bool HallCalibration::beginSweep(uint32_t now) {
  tick(now);
  if (status_.stage != CalibrationStage::WaitNode)
    return false;
  // 先验证地图自身；完整扫证据此时临时补齐，仅用于同一验证函数。
  candidate_.valid = true;
  candidate_.sweep_left = true;
  candidate_.sweep_right = true;
  candidate_.sweep_segments =
      (1u << (candidate_.node_count ? candidate_.node_count - 1 : 0)) - 1u;
  candidate_.noise_mm = 0;
  for (uint8_t j = 0; j + 1 < candidate_.node_count; ++j) {
    const Node &a = candidate_.nodes[j], &b = candidate_.nodes[j + 1];
    float d2 = 0, n2 = 0;
    for (uint8_t i = 0; i < kChannels; ++i) {
      const float d = b.response[i] - a.response[i];
      d2 += d * d;
      n2 += a.noise[i] * a.noise[i] + b.noise[i] * b.noise[i];
    }
    const float noise = d2 > 0 ? (b.mm - a.mm) * squareRoot(n2 / d2) : 100;
    if (noise > candidate_.noise_mm)
      candidate_.noise_mm = noise;
  }
  const bool good = validateCalibrationStructure(candidate_);
  candidate_.valid = false;
  candidate_.sweep_segments = 0;
  candidate_.sweep_left = false;
  candidate_.sweep_right = false;
  if (!good) {
    fail(CalibrationError::InvalidMap);
    return false;
  }
  // 每个采样周期只验证一个曲线点，避免一次完成最多80次投影阻塞其他输入。
  status_.stage = CalibrationStage::MapCheck;
  status_.required = (candidate_.node_count - 1) * 5;
  status_.collected = 0;
  have_sample_ = false;
  return true;
}
bool HallCalibration::finishSweep(uint32_t now) {
  tick(now);
  if (status_.stage != CalibrationStage::Sweep)
    return false;
  candidate_.valid = true;
  if (!validateCalibrationStructure(candidate_)) {
    fail(CalibrationError::MissingCoverage);
    return false;
  }
  status_.stage = CalibrationStage::Completed;
  return true;
}
void HallCalibration::tick(uint32_t now) {
  // 整个任务最长180秒；调用者可以显式取消后再开新任务。
  if (running() && now - started_us_ >= 180000000u)
    fail(CalibrationError::Timeout);
}
void HallCalibration::cancel() {
  if (!running())
    return;
  candidate_.valid = false;
  status_.stage = CalibrationStage::Cancelled;
  status_.error = CalibrationError::Cancelled;
}
void HallCalibration::sample(const Sample &s) {
  tick(s.timestamp_us);
  if (!running())
    return;
  if (status_.stage == CalibrationStage::WaitReference ||
      status_.stage == CalibrationStage::WaitNode)
    return;
  if (!s.ok) {
    fail(CalibrationError::SamplingError);
    return;
  }
  for (uint16_t r : s.raw)
    if (r <= 8 || r >= 4087) {
      fail(CalibrationError::Saturated);
      return;
    }
  // 不重复计入同一采样；采样断序或缺采大于10ms时拒绝静态校准。
  if (have_sample_) {
    if (s.sequence == last_sequence_ || s.timestamp_us == last_sample_us_)
      return;
    if (s.sequence - last_sequence_ != 1u ||
        s.timestamp_us - last_sample_us_ > 10000u) {
      fail(CalibrationError::SamplingError);
      return;
    }
  }
  have_sample_ = true;
  last_sequence_ = s.sequence;
  last_sample_us_ = s.timestamp_us;
  if (status_.stage == CalibrationStage::MapCheck) {
    const uint8_t segment = status_.collected / 5, step = status_.collected % 5;
    float r[kChannels]{};
    for (uint8_t i = 0; i < kChannels; ++i)
      r[i] = candidate_.nodes[segment].response[i] +
             (step * 0.25f) * (candidate_.nodes[segment + 1].response[i] -
                               candidate_.nodes[segment].response[i]);
    if (project(r, candidate_).ambiguous) {
      fail(CalibrationError::InvalidMap);
      return;
    }
    ++status_.collected;
    if (status_.collected == status_.required) {
      status_.stage = CalibrationStage::Sweep;
      status_.collected = 0;
      status_.required = 0;
      have_sample_ = false; // 随后的真实扫重新建立采样参考。
    }
    return;
  }
  if (status_.stage == CalibrationStage::Sweep) {
    float r[kChannels]{};
    responses(s, candidate_, r);
    const Projection p = project(r, candidate_);
    // 标定折线的35-count容差只是工程起点；偏离/失磁不算覆盖并拒绝本次任务。
    float signal = 0;
    for (float value : r)
      if (value > 0)
        signal += value;
    if (p.ambiguous || p.distance2 > 35 * 35 || signal < 80) {
      fail(CalibrationError::InvalidMap);
      return;
    }
    if (p.fraction >= 0.2f && p.fraction <= 0.8f)
      candidate_.sweep_segments |= 1u << p.segment;
    if (p.segment == 0 && p.fraction <= 0.05f)
      candidate_.sweep_left = true;
    if (p.segment + 2 == candidate_.node_count && p.fraction >= 0.95f)
      candidate_.sweep_right = true;
    status_.sweep_segments = candidate_.sweep_segments;
    status_.sweep_left = candidate_.sweep_left;
    status_.sweep_right = candidate_.sweep_right;
    return;
  }
  ++status_.collected;
  // Welford在线均值/方差，无保存样本数组、长窗口或等待循环。
  for (uint8_t i = 0; i < kChannels; ++i) {
    const float d = s.raw[i] - mean_[i];
    mean_[i] += d / status_.collected;
    m2_[i] += d * (s.raw[i] - mean_[i]);
  }
  if (status_.collected == status_.required)
    finishCapture();
}
void HallCalibration::finishCapture() {
  float noise[kChannels]{};
  for (uint8_t i = 0; i < kChannels; ++i) {
    noise[i] = squareRoot(m2_[i] / (status_.collected - 1));
    if (noise[i] > 8) {
      fail(CalibrationError::Noisy);
      return;
    }
  }
  if (status_.stage == CalibrationStage::Baseline) {
    for (uint8_t i = 0; i < kChannels; ++i) {
      candidate_.baseline[i] = mean_[i];
      candidate_.baseline_noise[i] = noise[i];
    }
    status_.stage = CalibrationStage::WaitReference;
    return;
  }
  if (status_.stage == CalibrationStage::Reference) {
    const uint8_t channel = status_.next_reference;
    const float d = mean_[channel] - candidate_.baseline[channel];
    if (magnitude(d) < 80) {
      fail(CalibrationError::WeakReference);
      return;
    }
    reference_[channel] = magnitude(d);
    candidate_.polarity[channel] = d > 0 ? 1 : -1;
    ++status_.next_reference;
    if (status_.next_reference < kChannels) {
      status_.stage = CalibrationStage::WaitReference;
      return;
    }
    const float average = (reference_[0] + reference_[1] + reference_[2]) / 3;
    for (uint8_t i = 0; i < kChannels; ++i)
      candidate_.gain[i] = average / reference_[i];
    status_.stage = CalibrationStage::WaitNode;
    return;
  }
  Node &n = candidate_.nodes[candidate_.node_count];
  n.mm = node_mm_;
  for (uint8_t i = 0; i < kChannels; ++i) {
    n.response[i] = (mean_[i] - candidate_.baseline[i]) *
                    candidate_.polarity[i] * candidate_.gain[i];
    // 空场及标签噪声合并，提醒集成层noise_mm只是由响应斜率估计。
    n.noise[i] =
        squareRoot(noise[i] * noise[i] + candidate_.baseline_noise[i] *
                                             candidate_.baseline_noise[i]) *
        candidate_.gain[i];
  }
  ++candidate_.node_count;
  status_.nodes = candidate_.node_count;
  status_.stage = CalibrationStage::WaitNode;
}
} // namespace hall
} // namespace fluxrail
