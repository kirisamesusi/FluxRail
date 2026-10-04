#pragma once
#include "../hall/HallTypes.h"
#include "../knobs/KnobTypes.h"
#include "ConfigTypes.h"
#include "Io4Types.h"
namespace fluxrail {
namespace common {
constexpr uint8_t kPresetCount = 4, kMaxProfiles = 12, kProfileNameBytes = 32;
constexpr uint32_t kDeviceSchema = 2;
using MotionAxis = knobs::Axis;
using KnobSettings = knobs::Settings;
// D12/D13工程起点；活动来源接管、同轴先动者持有，后续路由按此适配。
struct RoutingSettings {
  uint32_t idle_us = 10000;
  bool simultaneous_hall_first = true;
};
struct Profile {
  uint32_t id = 0;
  char name[kProfileNameBytes]{};
  RuntimeConfig
      runtime{}; // revision为DeviceConfig整体revision的投影，不独立持久化。
  hall::Settings hall{};
  KnobSettings knobs[2]{};
  RoutingSettings routing{};
  Io4Function io4_bindings[kSourceCount]{};
};
struct DeviceConfig {
  uint32_t schema = kDeviceSchema, revision = 1, active_id = 4, next_id = 5;
  uint8_t profile_count = 4;
  uint8_t startup_usb_mode =
      0; // 0=NORMAL_NKRO, 1=IO4; 本次身份在首次连接前固定。
  Profile profiles[kMaxProfiles]{};
  uint32_t calibration_generation = 0;
  hall::Calibration calibration{}; // 全局安装校准，不在任何模板中。
};
} // namespace common
} // namespace fluxrail
