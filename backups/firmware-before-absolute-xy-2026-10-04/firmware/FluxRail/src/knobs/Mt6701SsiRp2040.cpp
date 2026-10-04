#include "Mt6701SsiRp2040.h"
#include "../board/BoardConfig.h"
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <pico/time.h>

namespace fluxrail {
namespace knobs {
namespace {
spi_inst_t *bus(uint8_t id) { return id == 0 ? spi1 : spi0; }
constexpr uint32_t kTimeoutUs = 100; // 故障单路有界等待；真实正常耗时须实测。
constexpr uint32_t kMaxPolls = 10000; // 第二保险，不依赖停滞的时钟无限等待。
}
uint32_t Mt6701SsiRp2040::nowUs() const { return time_us_32(); }
void Mt6701SsiRp2040::configure(uint8_t id) {
  spi_inst_t *spi = bus(id);
  stats_.actual_clock_hz[id] = spi_init(spi, clock_hz_);
  // 厂家：CLK空闲高，下降沿采样，CPOL=1/CPHA=0（mode2）。
  // 24个时钟必须含首位D13；此候选必须用逻辑分析仪与实帧CRC确认。
  spi_set_format(spi, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);
  needs_init_[id] = false;
}
bool Mt6701SsiRp2040::begin(uint32_t hz) {
  if (begun_ || hz < 1000000 || hz > 2000000) return false;
  clock_hz_ = hz;
  for (uint8_t i = 0; i < kKnobCount; ++i) {
    const auto &p = board::kKnobPins[i];
    // 在SPI启用前片选拉高；片选用SIO手控，不让库按每字节自动拉高。
    gpio_init(p.cs); gpio_put(p.cs, true); gpio_set_dir(p.cs, GPIO_OUT);
    gpio_init(p.push); gpio_set_dir(p.push, GPIO_IN); gpio_pull_down(p.push);
    configure(i);
    gpio_set_function(p.rx, GPIO_FUNC_SPI);
    gpio_set_function(p.sck, GPIO_FUNC_SPI);
    // 没有gpio_set_function(TX)，GP19保持PUSH输入；内部发送dummy不需要物理MOSI。
  }
  begun_ = true;
  return true;
}
bool Mt6701SsiRp2040::read(uint8_t id, Sample &s) {
  s = Sample{};
  s.timestamp_us = nowUs();
  if (!begun_ || id >= kKnobCount) return false;
  spi_inst_t *spi = bus(id);
  const auto &p = board::kKnobPins[id];
  if (needs_init_[id]) configure(id);
  // 每次上一帧已读完RX；若意外残留，只读最多硬件FIFO深度，绝不无限drain。
  for (uint8_t i = 0; i < 8 && spi_is_readable(spi); ++i)
    (void)spi_get_hw(spi)->dr;
  const uint32_t start = nowUs();
  gpio_put(p.cs, false);
  busy_wait_us_32(1); // TH>=100ns；用1us候选同时覆盖首位准备时间。
  uint8_t sent = 0, received = 0;
  uint32_t frame = 0;
  bool ok = false;
  for (uint32_t polls = 0; polls < kMaxPolls; ++polls) {
    if (sent < 3 && spi_is_writable(spi)) { spi_get_hw(spi)->dr = 0; ++sent; }
    if (received < 3 && spi_is_readable(spi)) {
      frame = (frame << 8) | (spi_get_hw(spi)->dr & 0xffu); ++received;
    }
    if (received == 3 && !spi_is_busy(spi)) { ok = true; break; }
    if (nowUs() - start >= kTimeoutUs) break;
  }
  busy_wait_us_32(1); // TL>=0.5*TCLK，保留完整末位保持时间。
  gpio_put(p.cs, true);
  const uint32_t end = nowUs();
  s.frame = frame; s.span_us = end - start;
  s.timestamp_us = start + s.span_us / 2;
  s.push_gpio = gpio_get(p.push); s.transport_ok = ok;
  if (s.span_us > stats_.max_read_us[id]) stats_.max_read_us[id] = s.span_us;
  if (!ok) {
    ++stats_.timeouts[id];
    // 立即停止残余时钟，下次重新初始化FIFO；失败帧绝不用于角度。
    spi_deinit(spi); needs_init_[id] = true;
  }
  return ok;
}
} // namespace knobs
} // namespace fluxrail
