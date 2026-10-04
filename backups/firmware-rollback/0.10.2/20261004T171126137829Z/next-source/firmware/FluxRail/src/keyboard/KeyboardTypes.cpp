#include "KeyboardTypes.h"

namespace fluxrail {
namespace keyboard {
bool validBinding(const Binding &b) {
  switch (b.type) {
  case ActionType::Disabled:
    return b.usage == 0 && b.modifiers == 0 && b.button == 0;
  case ActionType::Keyboard:
    // 范围包含常用字母/数字/标点/导航/F键/小键盘，以及独立修饰键。
    // 00..03为无键/错误usage；独立修饰键使用E0..E7。
    return b.button == 0 && ((b.usage >= 4 && b.usage <= 0xa4) ||
                             (b.usage >= 0xe0 && b.usage <= 0xe7));
  case ActionType::MouseButton:
    return b.usage == 0 && b.modifiers == 0 &&
           (b.button == 1 || b.button == 2 || b.button == 4);
  }
  return false;
}
bool validConfig(const KeyboardConfig &c) {
  // 上限防止导入配置把单次重触发无限拖长；具体UI可选范围由公共层冻结。
  if (c.retrigger_gap_us > 100000)
    return false;
  for (const auto &b : c.bindings)
    if (!validBinding(b))
      return false;
  return true;
}
KeyboardConfig defaultKeyboardConfig() {
  KeyboardConfig c;
  const uint8_t usages[kKeyCount] = {0x33, 0x0f, 0x0e, 0x0d, 0x09,
                                     0x07, 0x16, 0x04, 0x19, 0x11};
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    c.bindings[i].type = ActionType::Keyboard;
    c.bindings[i].usage = usages[i];
  }
  // PUSH默认禁用只是D16建议；没有初始化PUSH硬件或重复保存其动作。
  return c;
}
bool bindingFromUsAscii(char ch, Binding &out) {
  Binding b;
  b.type = ActionType::Keyboard;
  if (ch >= 'a' && ch <= 'z')
    b.usage = static_cast<uint8_t>(4 + ch - 'a');
  else if (ch >= 'A' && ch <= 'Z') {
    b.usage = static_cast<uint8_t>(4 + ch - 'A');
    b.modifiers = 2;
  } else if (ch >= '1' && ch <= '9')
    b.usage = static_cast<uint8_t>(0x1e + ch - '1');
  else if (ch == '0')
    b.usage = 0x27;
  else {
    // ASCII标点表按US物理键映射；不是主机字符输入保证。
    const char shifted[] = {0,   '_',  '+', '{', '}', '|',
                            ':', 0x22, '<', '>', '?', '~'};
    const uint8_t usages[] = {0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
                              0x33, 0x34, 0x36, 0x37, 0x38, 0x35};
    bool found = false;
    // 同一个物理键的两组ASCII；数字常量避免引号转义歧义。
    const char base[] = {' ', '-',  '=', '[', ']', 0x5c,
                         ';', 0x27, ',', '.', '/', 0x60};
    for (uint8_t i = 0; i < 12; ++i) {
      if (ch == base[i]) {
        b.usage = usages[i];
        found = true;
        break;
      }
      if (i != 0 && ch == shifted[i]) {
        b.usage = usages[i];
        b.modifiers = 2;
        found = true;
        break;
      }
    }
    if (!found) {
      const char symbols[] = "!@#$%^&*()";
      for (uint8_t i = 0; i < 10; ++i)
        if (ch == symbols[i]) {
          b.usage = static_cast<uint8_t>(0x1e + i);
          b.modifiers = 2;
          found = true;
          break;
        }
    }
    if (!found)
      return false;
  }
  out = b;
  return true;
}
} // namespace keyboard
} // namespace fluxrail
