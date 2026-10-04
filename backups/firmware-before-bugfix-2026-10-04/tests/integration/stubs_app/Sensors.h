#pragma once
#include <stdint.h>
namespace mocksensor {
inline bool enabled=false,adc_ok=true,ssi_ok=true;
inline uint16_t raw[3]{2048,2048,2048},angle[2]{};
inline bool push[2]{};
inline uint32_t adc_reads=0,ssi_reads[2]{},advance_ticks[2]{};
}
