#pragma once
#include "../common/ConfigTypes.h"

namespace fluxrail {
namespace lighting {
constexpr uint8_t kKeyLights = 10;
using common::LightingConfig;
using common::LightMode;
using common::Rgb;
struct LightFrame {
  Rgb keys[kKeyLights]{}; // 物理BT顺序，尚未重排为串联灯链顺序。
  Rgb indicator{};        // 独立GP11，不是十灯链第十一颗。
};
struct LedOrder {
  // 输出第i灯对应哪个物理BT索引；灯链已由用户确认，色序/实板点灯仍待验证。
  uint8_t chain_to_bt[kKeyLights] = {8, 7, 6, 5, 4, 3, 2, 1, 0, 9};
};
bool validLightingConfig(const LightingConfig &config);
bool validLedOrder(const LedOrder &order);
// 键灯唯一用户亮度换算：floor(channel * envelope/255 * level/200)。
// 本地效果和IO4主机覆盖共用；level>100安全输出黑色，最终pack仍再次限幅。
Rgb scaleKeyColor(Rgb color, uint8_t envelope, uint8_t brightness_level);
// 仅传入实际生效的身份：普通保留模板自定义色，IO4优先工程身份色。
// 调用方在配置首次确认前保留默认白色，开关/25%由renderLights统一处理。
Rgb modeIndicatorColor(Rgb applied_template_color, bool active_io4);
// core0的纯算法；调用方约60Hz调度，传入真实物理mask而非映射后的usage。
// 指示色由公共层在模板/USB身份实际生效后选择，模块不私自定义身份优先级。
LightFrame renderLights(const LightingConfig &config, uint16_t physical_mask,
                        uint32_t now_ms, bool indicator_enabled,
                        Rgb effective_indicator_color);
// 最终输出硬限制：外部测试/IO4灯帧也必须走此函数，不存在裸写亮度的出口。
void packLightWords(const LightFrame &frame, const LedOrder &order, bool grb,
                    uint32_t keys[kKeyLights], uint32_t &indicator);
} // namespace lighting
} // namespace fluxrail
