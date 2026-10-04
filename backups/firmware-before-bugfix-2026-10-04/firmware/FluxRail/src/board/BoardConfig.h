#pragma once
#include "../common/InputTypes.h"
#include <stdint.h>

namespace fluxrail {
namespace board {
// 工程恢复键BT10/GP9；仅开机判定，待用户选择。
constexpr uint8_t kUsbRecoverySource=9;
constexpr uint8_t kKeyPins[common::kKeyCount] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr uint8_t kKeyLedPin = 10;
constexpr uint8_t kIndicatorPin = 11;
constexpr uint8_t kHallPins[3] = {26, 27, 28};
struct KnobPins {
  uint8_t rx, cs, sck, push;
};
constexpr KnobPins kKnobPins[2] = {{12, 13, 14, 19}, {16, 17, 18, 20}};
// 用户确认灯链BT9→BT8→BT7→BT6→BT5→BT4→BT3→BT2→BT1→BT10（2026-10-03）。
// 此数组为BT的0基索引；RGB/GRB仍待实板验证，不搬动bindings或逐BT配色。
constexpr uint8_t kLedChainToBt[common::kKeyCount] = {8, 7, 6, 5, 4,
                                                      3, 2, 1, 0, 9};
constexpr bool kLedGrb = true;
constexpr uint32_t kScanPeriodUs = 500;
constexpr uint32_t kLedPeriodUs = 16667;
constexpr uint32_t kConfigTimeoutUs = 2000000;
constexpr uint32_t kInputCoreTimeoutUs = 100000;
} // namespace board
} // namespace fluxrail
