#pragma once
#include "KeyLighting.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/pio.h>

namespace fluxrail {
namespace lighting {
struct LedOutputStats {
  uint32_t frames = 0;
  uint32_t busy_rejections = 0;
  uint32_t max_service_us = 0;
  uint32_t timeouts = 0;
};
// 核心0单一所有者；不可复制/移动，DMA读缓冲必须在整个输出期间保持存活。
// 使用两个PIO状态机+两个DMA，GP10与GP11并行输出，不使用关中断show。
class Ws2812OutputRp2040 {
public:
  Ws2812OutputRp2040() = default;
  Ws2812OutputRp2040(const Ws2812OutputRp2040 &) = delete;
  Ws2812OutputRp2040 &operator=(const Ws2812OutputRp2040 &) = delete;
  // board/integration提前规划PIO/DMA资源；申请失败返回false，回收已申请资源。
  // GRB为工程起点，实灯色序待测；支持RGB。灯链顺序必须为0..9的排列。
  bool begin(const LedOrder &order, bool grb = true);
  void service(uint32_t now_us);
  bool trySubmit(const LightFrame &frame, uint32_t now_us);
  bool busy() const { return active_; }
  bool faulted() const { return faulted_; }
  const LedOutputStats &stats() const { return stats_; }
  // 只在!busy()时结束；不忙等dma_channel_abort、不在析构中接触外设。
  bool end();

private:
  void releaseResources();
  PIO pio_ = nullptr;
  int sm_[2] = {-1, -1};
  int dma_[2] = {-1, -1};
  int offset_ = -1;
  LedOrder order_{};
  bool grb_ = true, active_ = false, tail_wait_ = false, faulted_ = false;
  uint32_t tail_since_us_ = 0, start_us_ = 0;
  uint32_t words_[kKeyLights]{};
  uint32_t indicator_word_ = 0;
  LedOutputStats stats_{};
};
} // namespace lighting
} // namespace fluxrail
#endif
