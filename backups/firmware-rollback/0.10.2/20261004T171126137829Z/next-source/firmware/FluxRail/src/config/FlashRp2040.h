#pragma once
#include "ConfigStore.h"
namespace fluxrail {
namespace config {
class FlashRp2040 final : public FlashDevice {
public:
  bool available() const override;
  bool read(uint8_t slot, uint32_t offset, uint8_t *data,
            uint32_t length) override;
  bool erase(uint8_t slot) override;
  bool program(uint8_t slot, uint32_t offset, const uint8_t *data,
               uint32_t length) override;
};
} // namespace config
} // namespace fluxrail
