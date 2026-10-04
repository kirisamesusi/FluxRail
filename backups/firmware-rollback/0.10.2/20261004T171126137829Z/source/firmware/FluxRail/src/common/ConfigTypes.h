#pragma once
#include "InputTypes.h"

namespace fluxrail {
namespace common {
enum class ActionType : uint8_t { Disabled, Keyboard, MouseButton };
enum class MouseButton : uint8_t { Left = 1, Right = 2, Middle = 4 };
// 与type无关的字段必须为0；禁用要求usage/modifiers/button全为0。
// 键盘usage在04..A4或E0..E7，modifiers是8位HID修饰掩码；鼠标只选1/2/4之一。
struct Binding {
  ActionType type = ActionType::Disabled;
  uint8_t usage = 0;
  uint8_t modifiers = 0;
  uint8_t button = 0;
};
struct KeyboardConfig {
  Binding bindings[kSourceCount]{};
  // 0..100000us；从目标UP可靠提交开始计时，DOWN还需等USB端点完成。
  uint32_t retrigger_gap_us = 1000;
};
// 上下沿独立消抖，均0..20000us；暂停期间仍更新当前真实物理状态。
struct ScanConfig {
  uint32_t press_debounce_us = 1000;
  uint32_t release_debounce_us = 1000;
};
struct Rgb {
  uint8_t r = 0, g = 0, b = 0;
};
enum class LightMode : uint8_t {
  Static,
  Breathing,
  ColorCycle,
  RainbowWave,
  Press
};
struct LightingConfig {
  bool enabled = true;
  LightMode mode = LightMode::Press;
  // 页面0..100，最终键灯输出最大50%（通道<=127），不是原始PWM值。
  uint8_t brightness_level = 50;
  bool per_key = false;
  Rgb global_color{255, 255, 255};
  Rgb key_colors[kKeyCount]{};
  uint32_t period_ms = 2000; // 100..60000ms，不用服务调用次数作灯效时钟。
  bool reverse = false;
};
// 仅是当前公共实时运行子集，不是完整Profile/Flash schema。
// 完整模板、模块2/3设置与全局机械校准见DeviceConfig.h；本对象只是活动模板投影。
struct RuntimeConfig {
  KeyboardConfig keyboard{};
  ScanConfig scan{};
  LightingConfig lighting{};
  Rgb indicator_color{255, 255, 255}; // D06工程起点，实际生效配置提供。
  bool indicator_enabled = true;
  uint32_t revision =
      1; // 同一整体快照的版本；恢复流另用epoch，不伪造配置变化。
};
} // namespace common
} // namespace fluxrail
