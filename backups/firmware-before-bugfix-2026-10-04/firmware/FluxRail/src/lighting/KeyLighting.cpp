#include "KeyLighting.h"

namespace fluxrail {
namespace lighting {
namespace {
Rgb wheel(uint8_t hue) {
  // 三段线性RGB轮；固定整数时间，不耗浮点三角函数或修改已保存的配色。
  if (hue < 85)
    return {static_cast<uint8_t>(255 - hue * 3), static_cast<uint8_t>(hue * 3),
            0};
  if (hue < 170) {
    hue -= 85;
    return {0, static_cast<uint8_t>(255 - hue * 3),
            static_cast<uint8_t>(hue * 3)};
  }
  hue -= 170;
  return {static_cast<uint8_t>(hue * 3), 0,
          static_cast<uint8_t>(255 - hue * 3)};
}
uint8_t scale(uint8_t channel, uint8_t envelope, uint8_t level) {
  // floor RGB * (envelope/255) * (level/100) * 0.5；32位不会溢出。
  return static_cast<uint8_t>(
      (static_cast<uint32_t>(channel) * envelope * level) / (255u * 200u));
}
uint32_t pack(Rgb c, uint8_t cap, bool grb) {
  // 这一层再次限幅，防止测试/启动/IO4主机灯命令绕过renderLights。
  if (c.r > cap)
    c.r = cap;
  if (c.g > cap)
    c.g = cap;
  if (c.b > cap)
    c.b = cap;
  const uint8_t first = grb ? c.g : c.r;
  const uint8_t second = grb ? c.r : c.g;
  // PIO左移取最高24位；不把RGB值当作普通little-endian DMA字节串。
  return (static_cast<uint32_t>(first) << 24) |
         (static_cast<uint32_t>(second) << 16) |
         (static_cast<uint32_t>(c.b) << 8);
}
} // namespace
Rgb scaleKeyColor(Rgb color, uint8_t envelope, uint8_t level) {
  if (level > 100)
    return {}; // 导入/外部调用异常也不能产生超亮输出。
  return {scale(color.r, envelope, level), scale(color.g, envelope, level),
          scale(color.b, envelope, level)};
}
Rgb modeIndicatorColor(Rgb template_color, bool active_io4) {
  // 身份固定到本次启动；RAM申请/已保存模式不等于当前USB身份。
  // 品红是D06工程默认，普通模板颜色由既有配置存储/读回，不覆写它。
  return active_io4 ? Rgb{255, 0, 255} : template_color;
}
bool validLightingConfig(const LightingConfig &c) {
  return c.brightness_level <= 100 &&
         static_cast<uint8_t>(c.mode) <=
             static_cast<uint8_t>(LightMode::Press) &&
         c.period_ms >= 100 && c.period_ms <= 60000;
}
bool validLedOrder(const LedOrder &order) {
  uint16_t seen = 0;
  for (auto id : order.chain_to_bt) {
    if (id >= kKeyLights || (seen & (1u << id)))
      return false;
    seen |= static_cast<uint16_t>(1u << id);
  }
  return seen == 0x03ff;
}
LightFrame renderLights(const LightingConfig &c, uint16_t physical,
                        uint32_t now, bool indicator_enabled,
                        Rgb indicator_color) {
  LightFrame frame;
  // GP11独立25%实际刻度，关键灯/调键灯滑块不会改变模式灯亮度。
  if (indicator_enabled)
    frame.indicator = {static_cast<uint8_t>(indicator_color.r / 4),
                       static_cast<uint8_t>(indicator_color.g / 4),
                       static_cast<uint8_t>(indicator_color.b / 4)};
  if (!validLightingConfig(c) || !c.enabled)
    return frame; // 非法参数不输出键灯。
  uint8_t phase =
      static_cast<uint8_t>((now % c.period_ms) * 256u / c.period_ms);
  if (c.reverse)
    phase = static_cast<uint8_t>(255 - phase);
  const uint8_t positions[kKeyLights] = {8, 7, 6, 5, 3, 2,
                                         1, 0, 4, 9}; // 左右布局ASDFV/JKL;N。
  for (uint8_t id = 0; id < kKeyLights; ++id) {
    uint8_t envelope = 255;
    Rgb color = c.per_key ? c.key_colors[id] : c.global_color;
    switch (c.mode) {
    case LightMode::Static:
      break;
    case LightMode::Breathing:
      envelope = phase < 128 ? static_cast<uint8_t>(phase * 2)
                             : static_cast<uint8_t>((255 - phase) * 2);
      break;
    case LightMode::ColorCycle:
      color = wheel(phase);
      break;
    case LightMode::RainbowWave:
      color = wheel(
          static_cast<uint8_t>(phase + positions[id] * 256u / kKeyLights));
      break;
    case LightMode::Press:
      envelope = (physical & (1u << id)) ? 255 : 0;
      break;
    }
    frame.keys[id] = scaleKeyColor(color, envelope, c.brightness_level);
  }
  return frame;
}
void packLightWords(const LightFrame &frame, const LedOrder &order, bool grb,
                    uint32_t keys[kKeyLights], uint32_t &indicator) {
  const bool valid = validLedOrder(order);
  for (uint8_t i = 0; i < kKeyLights; ++i)
    keys[i] = valid ? pack(frame.keys[order.chain_to_bt[i]], 127, grb) : 0;
  indicator = pack(frame.indicator, 63, grb);
}
} // namespace lighting
} // namespace fluxrail
