#pragma once
#include <stdint.h>

namespace fluxrail {
namespace hall {
constexpr uint8_t kChannels = 3;
constexpr uint8_t kMaxNodes = 17;
constexpr uint8_t kMinNodes = 7;
constexpr uint16_t kCalibrationVersion = 1;
// H2为物理零点，H1->H3为正。不能以外传感器中心替代实际磁铁端点。
constexpr float kSensorMm[kChannels] = {-33.750487f, 0.0f, 33.75f};

// 均为模块算法的值对象，不是公共Profile、wire包或持久化schema。
// 公共层尚未冻结Hall类型；集成时由唯一适配器复制这些值，不直接共享可变对象。
struct Sample {
  uint16_t raw[kChannels]{}; // 12位ADC: 0..4095，顺序H1/H2/H3。
  uint32_t timestamp_us = 0; // 整向量首末采样的中点，允许micros回绕。
  uint32_t
      channel_offset_us[kChannels]{}; // 每路有效样本相对向量起点的时间偏差。
  uint32_t span_us = 0; // 三通道顺序采样造成的整向量时间跨度。
  uint32_t sequence = 0; // 每次采样尝试递增，失败同样占序号。
  bool ok = true;        // 超时/ADC硬件错误必须置false。
};
struct Node {
  float mm = 0;
  float response[kChannels]{}; // (raw-baseline)*polarity*gain，保留有符号值。
  float noise[kChannels]{}; // 该标签静止采集的响应标准差，单位ADC count。
};
struct Calibration {
  uint16_t version = kCalibrationVersion;
  bool valid = false;
  float baseline[kChannels]{};
  float baseline_noise[kChannels]{};
  float gain[kChannels]{1, 1, 1};
  int8_t polarity[kChannels]{1, 1, 1}; // 磁极方向与输出invert完全独立。
  float left_mm = 0, right_mm = 0; // 磁铁中心真实端点，不默认±40mm。
  uint8_t node_count = 0;
  Node nodes[kMaxNodes]{}; // 物理mm严格递增；三维响应允许各路变号。
  uint32_t sweep_segments = 0; // 每段中间区域已实际采到的覆盖证据。
  bool sweep_left = false, sweep_right = false;
  float noise_mm = 0; // 根据各节点噪声/响应斜率估算，不是实测定位误差。
};
struct Settings {
  bool enabled = false; // 无有效校准禁用；模板开关由公共层提供。
  bool invert = false;
  float center_gain = 1;              // 建议0.1..8，非count/mm。
  float range_min = 0, range_max = 1; // 后端无关归一化范围，0<=min<max<=1。
  uint32_t quiet_tau_us = 1500; // 建议静止滤波时间常数，0..20000；实际dt计算。
  uint32_t moving_tau_us = 0; // 快移默认直接跟随，0..quiet_tau_us。
  float fast_speed_mm_s = 200; // 由安静到快速的连续滤波过渡速度，建议1..5000。
  float activity_mm = 0.20f; // 物理位移累积门槛，不随输出倍率变。
  uint32_t activity_hold_us = 10000;
  float enter_signal = 80, exit_signal = 60; // 扣基线后正响应和；有效性滞回。
  float curve_tolerance = 35; // 三维响应距折线的容差，ADC count。
  uint32_t max_gap_us = 10000; // 缺采超过此值重建参考，不滤入旧位置。
};
enum class Quality : uint8_t {
  Valid,
  Disabled,
  Uncalibrated,
  SamplingError,
  Saturated,
  WeakField,
  OffCurve,
  Ambiguous,
  Calibrating,
  Suspended
};
struct Estimate {
  float raw_mm = 0, mm = 0; // raw_mm=折线投影；mm=时间滤波结果。
  float p = 0, output = 0; // p方向/倍率无关；output包含invert/中心倍率/范围。
  float centroid_mm = 0; // 非负权重重心只作诊断，不当最终机械mm。
  float response[kChannels]{};
  float signal = 0, confidence = 0;
  uint32_t timestamp_us = 0, sequence = 0, revision = 0, epoch = 0;
  bool valid = false, active = false;
  Quality quality = Quality::Uncalibrated;
};
struct Statistics {
  uint32_t samples = 0, invalid = 0, saturated = 0, weak_field = 0;
  uint32_t off_curve = 0, ambiguous = 0, sampling_errors = 0;
  uint32_t max_interval_us = 0, max_valid_interval_us = 0;
  uint32_t deadline_misses = 0, sequence_gaps = 0;
  uint32_t queue_full = 0, recovery_discarded = 0;
};
// 投影诊断是算法内部类型，不是通用运动队列类型。
struct Projection {
  float mm = 0, distance2 = 0, fraction = 0;
  uint8_t segment = 0;
  bool ambiguous = false;
};
float clamp(float x, float lo, float hi);
float magnitude(float x);
float squareRoot(float x); // 校准使用的有界sqrt，无libm/动态分配依赖。
bool validateSettings(const Settings &s);
bool validateCalibration(const Calibration &c);
// 任务内部结构检查；完整导入必须使用上面的完整校验。
bool validateCalibrationStructure(const Calibration &c);
void responses(const Sample &s, const Calibration &c, float out[kChannels]);
Projection project(const float response[kChannels], const Calibration &c);
} // namespace hall
} // namespace fluxrail
