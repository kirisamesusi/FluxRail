#pragma once
#include <stdint.h>

namespace fluxrail {
namespace common {
constexpr uint8_t kKeyCount = 10;
constexpr uint8_t kSourceCount = 12;
constexpr uint16_t kSourceMask = 0x0fff;
enum class SourceId : uint8_t {
  BT1,
  BT2,
  BT3,
  BT4,
  BT5,
  BT6,
  BT7,
  BT8,
  BT9,
  BT10,
  P1,
  P2
};
struct InputEdge {
  uint32_t sequence = 0; // 十二路合并流，成功入队才+1，允许32位回绕。
  uint32_t timestamp_us = 0; // 候选电平首次观察时间，不是USB完成时间。
  uint32_t revision = 0;
  uint32_t epoch = 0; // 同一配置下也可因断连/溢出而更换流，拒绝旧流事件。
  uint8_t source_id = 0;
  bool down = false;
};
struct InputSnapshot {
  uint32_t revision = 0;
  uint32_t epoch = 0;
  uint32_t timestamp_us = 0;
  uint32_t last_sequence = 0; // 新epoch尚无边沿时为UINT32_MAX。
  uint16_t physical_mask =
      0; // bit0..9=BT1..10，bit10/11=P1/P2，低有效已转换为down。
  bool ok = false;
};
// 唯一普通键盘语义状态。USB编码器显式逐字段打包，不把对象内存当wire格式。
struct KeyboardState {
  uint8_t modifiers = 0;
  uint8_t keys[28]{};
  bool held(uint8_t usage) const {
    return usage < 0xe0
               ? (keys[usage >> 3] & (1u << (usage & 7))) != 0
               : usage <= 0xe7 && (modifiers & (1u << (usage - 0xe0))) != 0;
  }
};
} // namespace common
} // namespace fluxrail
