#include "KeyScannerRp2040.h"
#include "../board/BoardConfig.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/gpio.h>

namespace fluxrail {
namespace keyboard {
void beginKeyPinsRp2040() {
  // 从真实硬件文档提取：BT1..BT10=GP0..9，均接地按下；未使用矩阵扫描。
  for (uint8_t id = 0; id < common::kKeyCount; ++id) {
    const uint8_t pin = board::kKeyPins[id];
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
  }
}
uint16_t readKeyDownMaskRp2040() {
  // 一次读取SIO快照，减少逐键时间差；只反相十个按键位。
  const uint32_t levels = gpio_get_all();
  uint16_t down = 0;
  for (uint8_t id = 0; id < common::kKeyCount; ++id)
    if (!(levels & (1u << board::kKeyPins[id])))
      down |= static_cast<uint16_t>(1u << id);
  return down;
}
} // namespace keyboard
} // namespace fluxrail
#endif
