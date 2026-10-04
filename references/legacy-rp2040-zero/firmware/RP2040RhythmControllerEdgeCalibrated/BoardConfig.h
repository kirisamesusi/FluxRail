#pragma once

// Hardware assumptions that were not included in the request.
// Update these values if your PCB uses different routing.

namespace BoardConfig {
  constexpr uint8_t BUTTON_PINS[8] = {14, 13, 12, 3, 2, 1, 15, 4};

  constexpr uint8_t KEY_LED_PIN = 9;
  constexpr uint8_t KEY_LED_COUNT = 8;
  // 按键灯的物理串灯顺序。
  // 下标是逻辑按键编号（BT1~BT8 => 0~7），值是对应的物理灯珠索引。
  // 你当前提供的接线顺序是：BT8, BT6, BT5, BT4, BT3, BT2, BT1, BT7
  // 换算后得到：
  // BT1->6, BT2->5, BT3->4, BT4->3, BT5->2, BT6->1, BT7->7, BT8->0
  constexpr uint8_t KEY_LED_PIXEL_FOR_BUTTON[8] = {6, 5, 4, 3, 2, 1, 7, 0};

  // Assumption: the 8 ambient 4020 RGB LEDs are daisy-chained on GPIO 8.
  constexpr uint8_t AMBIENT_LED_PIN = 10;
  constexpr uint8_t AMBIENT_LED_COUNT = 8;

  // 4 个 SS49E 霍尔传感器接在 RP2040 的 ADC GPIO26~29 上。
  constexpr uint8_t HALL_PINS[4] = {26, 27, 28, 29};
  // 实际物理排列从左到右是：
  // H4(Hall on GP29), H3(GP28), H2(GP27), H1(GP26)
  // 但 HALL_PINS 的读取顺序是 {26,27,28,29}，
  // 所以下面的空间位置要按读取顺序反过来写：
  // GP26->最右, GP27->右中, GP28->左中, GP29->最左
  constexpr float HALL_SENSOR_POSITION[4] = {1.0f, 0.33f, -0.33f, -1.0f};

  constexpr uint16_t EEPROM_SIZE = 1024;
  constexpr uint16_t CONFIG_MAGIC = 0x5248;
  constexpr uint8_t CONFIG_VERSION = 2;

  constexpr uint32_t BUTTON_SCAN_US = 1000;
  constexpr uint32_t LED_FRAME_MS = 16;
  constexpr uint32_t CONFIG_BAUD = 115200;
  constexpr uint32_t CALIBRATION_WINDOW_MS = 5000;
}
