#pragma once
#include "../common/DeviceConfig.h"
#include "RuntimeCodec.h"
namespace fluxrail {
namespace protocol {
void encodeMotion(JsonWriter &writer, const common::Profile &profile);
bool decodeMotion(const Json &json, uint16_t token, common::Profile &profile);
} // namespace protocol
} // namespace fluxrail
