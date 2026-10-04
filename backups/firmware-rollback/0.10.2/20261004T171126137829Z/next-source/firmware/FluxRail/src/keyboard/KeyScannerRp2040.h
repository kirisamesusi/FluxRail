#pragma once
#include <stdint.h>

namespace fluxrail {
namespace keyboard {
// 仅由核心1调用一次初始化、再定时读电平；不读取PUSH、ADC或SSI。
void beginKeyPinsRp2040();
uint16_t readKeyDownMaskRp2040();
} // namespace keyboard
} // namespace fluxrail
