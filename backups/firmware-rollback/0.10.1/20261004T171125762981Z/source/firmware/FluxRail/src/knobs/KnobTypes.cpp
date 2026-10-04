#include "KnobTypes.h"

namespace fluxrail {
namespace knobs {
namespace {
bool range(double v, double lo, double hi) { return v >= lo && v <= hi; }
}
uint8_t crc6(uint32_t payload) {
  // CRC-6: x^6+x+1，MSB先行，零初值、无最终异或/反射。
  // 厂家手册未列初值向量，此实现需实帧确认；不能用CRC失败时退化成无CRC角度。
  uint8_t crc = 0;
  for (int bit = 17; bit >= 0; --bit) {
    const bool feedback = ((crc >> 5) ^ (payload >> bit)) & 1u;
    crc = static_cast<uint8_t>((crc << 1) & 0x3f);
    if (feedback) crc ^= 0x03;
  }
  return crc;
}
Frame decode(const Sample &s) {
  Frame f{};
  if (!s.transport_ok || s.frame > 0xffffffu) return f;
  f.angle = static_cast<uint16_t>(s.frame >> 10);
  f.status = static_cast<uint8_t>((s.frame >> 6) & 0xf);
  f.crc_ok = crc6(s.frame >> 6) == (s.frame & 0x3f);
  if (!f.crc_ok) { f.quality = Quality::CrcError; return f; }
  f.push = (f.status & 4) != 0;
  // 状态位独立，失跟踪优先作为运动拒绝原因，各位错误统计在运动层分别记录。
  if (f.status & 8) f.quality = Quality::LossOfTrack;
  else if ((f.status & 3) == 1) f.quality = Quality::StrongField;
  else if ((f.status & 3) == 2) f.quality = Quality::WeakField;
  else if ((f.status & 3) == 3) f.quality = Quality::ReservedField;
  else { f.field_ok = true; f.quality = Quality::Valid; }
  return f;
}
bool validate(const Settings &s) {
  // 比较式同时拒绝NaN/Inf；微秒跨度始终小于半个32位周期，支持无符号dt。
  return static_cast<uint8_t>(s.axis) <= 1 && range(s.gain, 0.01, 16) &&
      range(s.counts_per_turn_at_gain1, 1, 1000000) &&
      s.filter_tau_us <= 20000 && s.max_gap_us >= 500 && s.max_gap_us <= 100000 &&
      range(s.max_turns_per_second, 0.01, 1000) &&
      s.activity_ticks >= 1 && s.activity_ticks < 8192 && s.activity_hold_us <= 1000000 &&
      s.push.press_us <= 20000 && s.push.rotating_press_us >= s.push.press_us &&
      s.push.rotating_press_us <= 50000 && s.push.release_us <= 20000 &&
      s.push.rearm_us >= 500 && s.push.rearm_us <= 100000 &&
      s.push.max_sample_gap_us >= 500 && s.push.max_sample_gap_us <= 100000 &&
      s.push.max_hold_us >= 1000 && s.push.max_hold_us <= 16000000;
}
Settings defaults(uint8_t id) {
  Settings s{};
  s.axis = id == 1 ? Axis::Y : Axis::X;
  return s;
}
} // namespace knobs
} // namespace fluxrail
