#pragma once
#include "ProfileManager.h"
#include <stddef.h>
namespace fluxrail {
namespace config {
uint32_t crc32(const uint8_t *data, size_t size);
class DeviceCodec {
public:
  static size_t encode(const common::DeviceConfig &config, uint8_t *data,
                       size_t capacity);
  bool decode(const uint8_t *data, size_t size, common::DeviceConfig &out);

private:
  common::DeviceConfig candidate_{}; // 核心0隔离工作区，不在4KiB栈放完整配置。
};
} // namespace config
} // namespace fluxrail
