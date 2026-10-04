#pragma once
#include "../common/ConfigTypes.h"

namespace fluxrail {
namespace keyboard {
// 兼容原模块API名称，但所有数据只在common定义，避免出现两份绑定/报告。
using common::ActionType;
using common::Binding;
using common::KeyboardConfig;
using common::KeyboardState;
using common::kKeyCount;
using common::kSourceCount;
using common::kSourceMask;
using common::MouseButton;
bool validBinding(const Binding &binding);
bool validConfig(const KeyboardConfig &config);
KeyboardConfig defaultKeyboardConfig();
// 独立工具供统一协议层调用；仅解释US ASCII，不处理布局/输入法/字符串宏。
// 失败时不修改out，大写及移位符号明确返回Shift修饰。
bool bindingFromUsAscii(char character, Binding &out);

// 所有回调均在核心0执行。true表示后端已接收并复制当前状态，而且保序。
// 后端忙/未完成上一报告时返回false；不可返回true后只覆盖一个latest快照。
// 映射器下一次service才提交下一份，UP和DOWN绝不能合并。
class ActionSink {
public:
  virtual ~ActionSink() = default;
  virtual bool usesPhysical() const { return false; }
  virtual bool tryPhysicalState(uint16_t) { return false; }
  virtual bool outputsComplete() { return true; }
  virtual bool tryKeyboard(const KeyboardState &state) = 0;
  virtual bool tryMouseButtons(uint8_t buttons) = 0;
};
} // namespace keyboard
} // namespace fluxrail
