#pragma once
#include "KnobSampler.h"
#include <stdint.h>

namespace fluxrail {
namespace knobs {
struct SsiStats {
  uint32_t timeouts[kKnobCount]{}, max_read_us[kKnobCount]{};
  uint32_t actual_clock_hz[kKnobCount]{};
};
// 唯一核心1所有者，U4=spi1/U5=spi0，板引脚只来自公共BoardConfig。
// 直接访问SDK有界FIFO；不调用无超时的SPI.transfer/spi_read_blocking。
// 不初始化TX/MOSI GPIO，GP19只初始化为PUSH输入，禁止别处使用SPI/SPI1。
class Mt6701SsiRp2040 final : public Reader {
public:
  bool begin(uint32_t clock_hz = 2000000); // 仅一次，候选1..2MHz。
  bool read(uint8_t knob_id, Sample &sample) override;
  uint32_t nowUs() const;
  const SsiStats &stats() const { return stats_; }
private:
  void configure(uint8_t knob_id);
  uint32_t clock_hz_ = 2000000;
  bool begun_ = false, needs_init_[kKnobCount]{};
  SsiStats stats_{};
};
} // namespace knobs
} // namespace fluxrail
