#pragma once
#include "InputTypes.h"
namespace fluxrail {
namespace common {
enum class Io4Function : uint8_t {
  Disabled,
  LeftA,
  LeftB,
  LeftC,
  RightA,
  RightB,
  RightC,
  LeftMenu,
  RightMenu,
  LeftSide,
  RightSide,
  Test,
  Service
};
inline bool validIo4Function(Io4Function f) { return uint8_t(f) <= 12; }
// 工程布局，不是用户已确认的音击物理摆位。
inline void defaultIo4Bindings(Io4Function (&b)[kSourceCount]) {
  const Io4Function defaults[kSourceCount] = {
      Io4Function::RightMenu, Io4Function::RightC,   Io4Function::RightB,
      Io4Function::RightA,    Io4Function::LeftMenu, Io4Function::LeftC,
      Io4Function::LeftB,     Io4Function::LeftA,    Io4Function::LeftSide,
      Io4Function::RightSide, Io4Function::Disabled, Io4Function::Disabled};
  for (uint8_t i = 0; i < kSourceCount; ++i)
    b[i] = defaults[i];
}
} // namespace common
} // namespace fluxrail
