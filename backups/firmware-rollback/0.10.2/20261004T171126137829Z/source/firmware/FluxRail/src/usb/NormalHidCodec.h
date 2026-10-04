#pragma once
#include "../common/InputTypes.h"
#include <stdint.h>

namespace fluxrail {
namespace usb {
constexpr uint8_t kKeyboardPayloadBytes = 29;
constexpr uint8_t kMousePayloadBytes = 5;
constexpr uint8_t kRelativeReportId = 1, kAbsoluteReportId = 2;
constexpr uint8_t kAbsolutePayloadBytes = 5;
constexpr uint16_t kAbsoluteAxisMax = 32767, kAbsoluteAxisCenter = 16384;
// 绝对模式一count为1/1024屏幕跨度，不依赖主机像素分辨率。
constexpr int32_t kAbsoluteUnitsPerCount = 32;
bool encodeAbsoluteXY(float output, uint16_t y,
                      uint8_t out[kAbsolutePayloadBytes]);
extern const uint8_t kKeyboardDescriptor[];
extern const uint16_t kKeyboardDescriptorBytes;
extern const uint8_t kMouseDescriptor[];
extern const uint16_t kMouseDescriptorBytes;
void encodeKeyboard(const common::KeyboardState &state,
                    uint8_t out[kKeyboardPayloadBytes]);
void encodeRelativeMouse(uint8_t buttons, int16_t x, int16_t y,
                         uint8_t out[kMousePayloadBytes]);
} // namespace usb
} // namespace fluxrail
