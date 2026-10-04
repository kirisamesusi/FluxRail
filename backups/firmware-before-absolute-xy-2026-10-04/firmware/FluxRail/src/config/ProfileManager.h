#pragma once
#include "../common/DeviceConfig.h"
namespace fluxrail {
namespace config {
bool validName(const char *name);
bool validProfile(const common::Profile &profile);
bool validDevice(const common::DeviceConfig &config);
void defaults(common::DeviceConfig &config);
common::Profile *find(common::DeviceConfig &config, uint32_t id);
const common::Profile *find(const common::DeviceConfig &config, uint32_t id);
common::RuntimeConfig activeRuntime(const common::DeviceConfig &config);
uint32_t nextRevision(uint32_t revision);
// 修改的是隔离候选；调用者校验、两核确认后才发布到活动配置。
bool createProfile(common::DeviceConfig &config, uint32_t copy_id,
                   const char *name);
bool renameProfile(common::DeviceConfig &config, uint32_t id, const char *name);
bool deleteProfile(common::DeviceConfig &config, uint32_t id);
bool restorePreset(common::DeviceConfig &config, uint32_t id);
} // namespace config
} // namespace fluxrail
