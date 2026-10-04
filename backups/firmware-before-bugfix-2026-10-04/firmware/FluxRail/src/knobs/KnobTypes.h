#pragma once
#include "../common/InputTypes.h"
#include <stdint.h>

namespace fluxrail {
namespace knobs {
constexpr uint8_t kKnobCount = 2;
constexpr uint32_t kSamplePeriodUs = 500;
constexpr int32_t kTicksPerTurn = 16384;
// 算法值对象，不是独立Profile、Flash schema或wire包。
// 公共v0.7的common::KnobSettings直接别名复用Settings；不另建配置结构。
// Motion仍为局部算法输出，公共实时运动通道须由集成负责人冻结/适配。
enum class Axis : uint8_t { X, Y };
enum class Quality : uint8_t {
  Valid, TransportError, CrcError, StrongField, WeakField, ReservedField,
  LossOfTrack, Reference, Ambiguous, Gap, Suspended
};
struct Sample {
  uint32_t frame = 0; // 完整24位，禁止静默忽略超出24位的畸形输入。
  uint32_t timestamp_us = 0; // SSI帧起止中点，micros允许32位回绕。
  uint32_t sequence = 0; // 每路采样尝试独立递增；失败也占一个序号。
  uint32_t span_us = 0;
  bool transport_ok = false;
  bool push_gpio = false; // 芯片输出高有效；不是机械按键低有效。
};
struct Frame {
  uint16_t angle = 0; // 0..16383，未通过CRC时不得用于运动。
  uint8_t status = 0;
  bool crc_ok = false, field_ok = false, push = false;
  Quality quality = Quality::TransportError;
};
struct PushSettings {
  bool enabled = false; // 默认禁用，动作只保存在公共bindings[10/11]。
  uint32_t press_us = 2000, rotating_press_us = 5000, release_us = 1000;
  uint32_t rearm_us = 2000, max_hold_us = 2000000;
  uint32_t max_sample_gap_us = 2000; // 断采不算稳定确认，恢复重新武装。
};
struct Settings {
  bool enabled = false; // 模板层提供开关；音击默认两路关。
  Axis axis = Axis::X;
  bool invert = false;
  double gain = 1, counts_per_turn_at_gain1 = 1024;
  // 输出量低通在展开之后，默认0直接输出，不吞慢转、不默认速度加速。
  uint32_t filter_tau_us = 0;
  uint32_t max_gap_us = 2000;
  // 最大物理转速仅为展开可信边界，不是HID限速；须与实测最大有效间隔共同确认。
  double max_turns_per_second = 100;
  uint16_t activity_ticks = 8; // 累积物理偏移门槛，与gain和counts无关。
  uint32_t activity_hold_us = 10000;
  PushSettings push{};
};
struct Motion {
  uint32_t timestamp_us = 0, sequence = 0, revision = 0, epoch = 0;
  uint8_t knob_id = 0; // 0=U4, 1=U5，PUSH source_id使用公共枚举。
  Axis axis = Axis::X;
  uint16_t raw_angle = 0;
  uint8_t field_status = 0;
  int32_t delta_ticks = 0; // 物理原始增量，首次/失效恢复为0。
  int64_t raw_ticks = 0; // 只累计可信增量，失效期间运动不可重构。
  double raw_turns = 0, scaled_turns = 0;
  double delta_counts = 0; // 保留小数；公共后端成功提交整数后扣除，不能clamp丢弃。
  bool valid = false, active = false, enabled = false;
  Quality quality = Quality::Reference;
};
// 诊断快照仅核心1所有；公共遥测复制后跨核，不能把它当可靠边沿队列。
struct PushSnapshot {
  uint32_t timestamp_us = 0, sequence = 0, revision = 0, epoch = 0;
  uint8_t source_id = 0;
  bool raw_gpio = false, ssi_push = false, crc_ok = false, field_ok = false;
  bool filtered_down = false, rotating = false;
};
struct MotionStats {
  uint32_t samples = 0, crc_errors = 0, transport_errors = 0;
  uint32_t strong_field = 0, weak_field = 0, reserved_field = 0, loss_of_track = 0;
  uint32_t reacquire = 0, ambiguous = 0, gaps = 0, sequence_gaps = 0;
  uint32_t max_interval_us = 0, max_valid_gap_us = 0, deadline_misses = 0;
  double filter_discarded_ticks = 0; // 参考重建明确丢弃的滤波尾量，可正可负。
};
struct PushStats {
  uint32_t presses = 0, releases = 0, rejected_candidates = 0;
  uint32_t mismatches = 0, quality_releases = 0, hold_timeouts = 0;
};
uint8_t crc6(uint32_t payload18);
Frame decode(const Sample &sample);
bool validate(const Settings &settings);
// 两路轴默认建议U4=X/U5=Y；启用状态由模板决定，模块不擅自开启。
Settings defaults(uint8_t knob_id);
} // namespace knobs
} // namespace fluxrail
