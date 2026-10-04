#pragma once
#include <stdint.h>
#include <Adafruit_TinyUSB.h>
namespace mockapp {inline uint32_t now=0;inline uint16_t raw=0;inline bool complete_usb=true;}
inline uint32_t micros(){return mockapp::now;}
inline uint32_t millis(){return mockapp::now/1000;}
void TinyUSB_Device_Task();

inline void delayMicroseconds(uint32_t) {}
struct MockRp2040 {uint32_t reboots=0;void reboot(){++reboots;}};
inline MockRp2040 rp2040;
