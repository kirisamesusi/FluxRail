#pragma once
#include "../protocol/ConfigProtocol.h"
namespace fluxrail {
namespace usb {
// Core0 only. Uses port-owned USB mutex; no blocking Serial operations or
// callbacks.
class CdcTransport final : public protocol::CdcPort {
public:
  bool connectionState(bool &online) override;
  bool txComplete() override;
  int32_t readByte() override;
  uint32_t write(const char *data, uint32_t length) override;
};
} // namespace usb
} // namespace fluxrail
