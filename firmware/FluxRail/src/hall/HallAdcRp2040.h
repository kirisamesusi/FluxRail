#pragma once
#include "HallSampler.h"

namespace fluxrail {
namespace hall {
// 核心1唯一ADC所有者。公共层必须禁止analogRead/温度ADC/其他核同时访问。
// 不使用ADC IRQ/FIFO/DMA/硬件核FIFO。每路换mux后丢首样，再取12位结果。
class HallAdcRp2040 final : public AdcReader {
public:
  bool begin(); // 有界复位和READY等待，失败交公共RuntimeHealth。
  uint32_t nowUs() const override;
  bool read12(uint8_t channel, uint16_t &raw) override;

private:
  bool conversion(uint16_t &raw);
  bool begun_ = false;
};
} // namespace hall
} // namespace fluxrail
