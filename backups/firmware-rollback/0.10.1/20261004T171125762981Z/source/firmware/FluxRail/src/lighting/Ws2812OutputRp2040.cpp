#include "Ws2812OutputRp2040.h"
#include "../board/BoardConfig.h"
#if defined(ARDUINO_ARCH_RP2040)
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/pio_instructions.h>
#include <pico/time.h>

namespace fluxrail {
namespace lighting {
namespace {
// 标准800kHz、每bit十个PIO周期：T1=2、T2=5、T3=3。
// 0：高2/低8；1：高7/低3。FIFO为空时out side0停住，保持低电平。
const uint16_t instructions[] = {
    static_cast<uint16_t>(pio_encode_out(pio_x, 1) | pio_encode_sideset(1, 0) |
                          pio_encode_delay(2)),
    static_cast<uint16_t>(pio_encode_jmp_not_x(3) | pio_encode_sideset(1, 1) |
                          pio_encode_delay(1)),
    static_cast<uint16_t>(pio_encode_jmp(0) | pio_encode_sideset(1, 1) |
                          pio_encode_delay(4)),
    static_cast<uint16_t>(pio_encode_nop() | pio_encode_sideset(1, 0) |
                          pio_encode_delay(4))};
// origin=-1支持动态装载，pio_add_program会重定位jmp目标。
const pio_program program = {instructions, 4, -1, 0};
constexpr uint32_t kDrainAndLatchUs =
    570; // TX FIFO8+OSR1最多270us，再低电平300us。
constexpr uint32_t kTransferTimeoutUs = 5000; // 工程诊断阈值，非实测性能承诺。
} // namespace
bool Ws2812OutputRp2040::begin(const LedOrder &order, bool grb) {
  if (pio_ || !validLedOrder(order))
    return false;
  // 所有资源预先申请；失败不能panic，也不能偷偷占用另一模块已使用的SM。
  const PIO candidates[] = {pio0, pio1};
  for (PIO candidate : candidates) {
    if (!pio_can_add_program(candidate, &program))
      continue;
    const int first = pio_claim_unused_sm(candidate, false);
    if (first < 0)
      continue;
    const int second = pio_claim_unused_sm(candidate, false);
    if (second < 0) {
      pio_sm_unclaim(candidate, first);
      continue;
    }
    pio_ = candidate;
    sm_[0] = first;
    sm_[1] = second;
    break;
  }
  if (!pio_)
    return false;
  dma_[0] = dma_claim_unused_channel(false);
  if (dma_[0] >= 0)
    dma_[1] = dma_claim_unused_channel(false);
  if (dma_[0] < 0 || dma_[1] < 0) {
    releaseResources();
    return false;
  }
  offset_ = pio_add_program(pio_, &program);
  if (offset_ < 0) {
    releaseResources();
    return false;
  }
  order_ = order;
  grb_ = grb;
  faulted_ = false;
  for (uint8_t lane = 0; lane < 2; ++lane) {
    const uint pin = lane == 0 ? board::kKeyLedPin : board::kIndicatorPin;
    pio_gpio_init(pio_, pin);
    pio_sm_set_consecutive_pindirs(pio_, sm_[lane], pin, 1, true);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset_, offset_ + 3);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_clkdiv(&c, static_cast<float>(clock_get_hz(clk_sys)) /
                                 (800000.0f * 10.0f));
    if (pio_sm_init(pio_, sm_[lane], offset_, &c) != 0) {
      releaseResources();
      return false;
    }
    pio_sm_set_pins_with_mask(pio_, sm_[lane], 0, 1u << pin);
    pio_sm_set_enabled(pio_, sm_[lane], true);
    auto d = dma_channel_get_default_config(dma_[lane]);
    channel_config_set_transfer_data_size(&d, DMA_SIZE_32);
    channel_config_set_read_increment(&d, true);
    channel_config_set_write_increment(&d, false);
    channel_config_set_dreq(&d, pio_get_dreq(pio_, sm_[lane], true));
    dma_channel_configure(dma_[lane], &d, &pio_->txf[sm_[lane]],
                          lane == 0 ? words_ : &indicator_word_, 0, false);
  }
  // 首帧也先保证reset低电平；无需delay阻塞初始化核心0。
  active_ = tail_wait_ = true;
  tail_since_us_ = time_us_32();
  start_us_ = tail_since_us_;
  return true;
}
void Ws2812OutputRp2040::service(uint32_t now) {
  const uint32_t entered = time_us_32();
  if (pio_ && active_ && !faulted_) {
    if (!tail_wait_ && !dma_channel_is_busy(dma_[0]) &&
        !dma_channel_is_busy(dma_[1])) {
      tail_wait_ = true;
      tail_since_us_ = now;
    }
    if (tail_wait_ && now - tail_since_us_ >= kDrainAndLatchUs) {
      active_ = tail_wait_ = false;
    } else if (!tail_wait_ && now - start_us_ > kTransferTimeoutUs) {
      // 不调用会等待DMA硬件完成的abort。保持缓冲不再被改写，交公共健康层恢复。
      faulted_ = true;
      ++stats_.timeouts;
    }
  }
  const uint32_t cost = time_us_32() - entered;
  if (cost > stats_.max_service_us)
    stats_.max_service_us = cost;
}
bool Ws2812OutputRp2040::trySubmit(const LightFrame &frame, uint32_t now) {
  service(now);
  if (!pio_ || faulted_)
    return false;
  if (active_) {
    ++stats_.busy_rejections;
    return false;
  }
  // 唯一入口重新限幅、映射，DMA运行中不会写其源缓冲。
  packLightWords(frame, order_, grb_, words_, indicator_word_);
  dma_channel_set_read_addr(dma_[0], words_, false);
  dma_channel_set_read_addr(dma_[1], &indicator_word_, false);
  dma_channel_set_trans_count(dma_[0], kKeyLights, false);
  dma_channel_set_trans_count(dma_[1], 1, false);
  dma_start_channel_mask((1u << dma_[0]) | (1u << dma_[1]));
  active_ = true;
  tail_wait_ = false;
  start_us_ = now;
  ++stats_.frames;
  return true;
}
bool Ws2812OutputRp2040::end() {
  // 故障DMA仍busy时不能释放/重用缓冲；由集成执行受控重启或专门停机。
  if (active_)
    return false;
  releaseResources();
  return true;
}
void Ws2812OutputRp2040::releaseResources() {
  for (uint8_t lane = 0; lane < 2; ++lane) {
    if (dma_[lane] >= 0) {
      dma_channel_unclaim(dma_[lane]);
      dma_[lane] = -1;
    }
    if (pio_ && sm_[lane] >= 0) {
      pio_sm_set_enabled(pio_, sm_[lane], false);
      pio_sm_clear_fifos(pio_, sm_[lane]);
      pio_sm_unclaim(pio_, sm_[lane]);
      sm_[lane] = -1;
    }
  }
  if (pio_ && offset_ >= 0)
    pio_remove_program(pio_, &program, offset_);
  if (pio_)
    for (uint8_t lane = 0; lane < 2; ++lane) {
      const uint pin = lane == 0 ? board::kKeyLedPin : board::kIndicatorPin;
      gpio_init(pin);
      gpio_set_dir(pin, GPIO_OUT);
      gpio_put(pin, 0);
    }
  offset_ = -1;
  pio_ = nullptr;
  active_ = tail_wait_ = faulted_ = false;
}
} // namespace lighting
} // namespace fluxrail
#endif
