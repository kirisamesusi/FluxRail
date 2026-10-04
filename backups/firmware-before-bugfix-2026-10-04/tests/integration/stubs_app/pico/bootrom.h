#pragma once
#include <stdint.h>
namespace mockapp {inline uint32_t bootloaders=0;}
inline void reset_usb_boot(uint32_t,uint32_t){++mockapp::bootloaders;}
