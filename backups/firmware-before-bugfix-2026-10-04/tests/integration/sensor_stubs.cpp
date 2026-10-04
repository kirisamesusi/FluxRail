// 只替换ADC/SSI外设边界，实际Hall/Knob算法和App控制/路由仍执行原始C++。
#include "hall/HallAdcRp2040.h"
#include "knobs/Mt6701SsiRp2040.h"
#include <Arduino.h>
#include <Sensors.h>
namespace fluxrail {namespace hall {
bool HallAdcRp2040::begin(){return mocksensor::adc_ok;}
uint32_t HallAdcRp2040::nowUs()const{return mockapp::now;}
bool HallAdcRp2040::read12(uint8_t channel,uint16_t &out){++mocksensor::adc_reads;out=mocksensor::raw[channel];return mocksensor::enabled&&mocksensor::adc_ok;}
}namespace knobs {
bool Mt6701SsiRp2040::begin(uint32_t){return mocksensor::ssi_ok;}
uint32_t Mt6701SsiRp2040::nowUs()const{return mockapp::now;}
bool Mt6701SsiRp2040::read(uint8_t id,Sample &s){
  ++mocksensor::ssi_reads[id];mocksensor::angle[id]=uint16_t((mocksensor::angle[id]+mocksensor::advance_ticks[id])&16383);
  const uint32_t payload=(uint32_t(mocksensor::angle[id])<<4)|(mocksensor::push[id]?4u:0u);
  s.frame=(payload<<6)|crc6(payload);s.timestamp_us=mockapp::now;s.push_gpio=mocksensor::push[id];s.transport_ok=mocksensor::enabled&&mocksensor::ssi_ok;return s.transport_ok;
}
} }
