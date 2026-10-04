#include "HallAdcRp2040.h"
#include "../board/BoardConfig.h"
#include <hardware/adc.h>
#include <hardware/resets.h>
#include <pico/time.h>

namespace fluxrail {
namespace hall {
uint32_t HallAdcRp2040::nowUs() const { return time_us_32(); }
bool HallAdcRp2040::begin() {
  if (begun_)
    return true;
  // SDK adc_init/adc_read有无期限的等待；此处使用同一寄存器语义加明确超时。
  reset_block_num(RESET_ADC);
  unreset_block_num(RESET_ADC);
  uint32_t start = nowUs();
  while (!(resets_hw->reset_done & (1u << RESET_ADC)))
    if (nowUs() - start >= 100)
      return false;
  adc_hw->cs = ADC_CS_EN_BITS;
  start = nowUs();
  while (!(adc_hw->cs & ADC_CS_READY_BITS))
    if (nowUs() - start >= 100)
      return false;
  for (uint8_t i = 0; i < kChannels; ++i)
    adc_gpio_init(board::kHallPins[i]);
  adc_set_round_robin(0);
  adc_set_temp_sensor_enabled(false);
  adc_fifo_setup(false, false, 0, false, false);
  adc_irq_set_enabled(false);
  adc_set_clkdiv(0);
  begun_ = true;
  return true;
}
bool HallAdcRp2040::conversion(uint16_t &raw) {
  if (!(adc_hw->cs & ADC_CS_READY_BITS))
    return false;
  hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);
  const uint32_t start = nowUs();
  while (!(adc_hw->cs & ADC_CS_READY_BITS))
    if (nowUs() - start >= 20)
      return false;
  if (adc_hw->cs & ADC_CS_ERR_BITS)
    return false;
  raw = static_cast<uint16_t>(adc_hw->result & 0x0fffu);
  return true;
}
bool HallAdcRp2040::read12(uint8_t channel, uint16_t &raw) {
  if (!begun_ || channel >= kChannels || !(adc_hw->cs & ADC_CS_READY_BITS))
    return false;
  adc_select_input(board::kHallPins[channel] - 26);
  uint16_t discarded = 0;
  return conversion(discarded) && conversion(raw);
}
} // namespace hall
} // namespace fluxrail
