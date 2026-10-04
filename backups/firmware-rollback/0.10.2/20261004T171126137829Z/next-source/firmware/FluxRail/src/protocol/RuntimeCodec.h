#pragma once
#include "../common/ConfigTypes.h"
#include "Json.h"
namespace fluxrail {
namespace protocol {
bool decodeRuntime(const Json &json, uint16_t token,
                   common::RuntimeConfig &config);
void encodeRuntime(JsonWriter &writer, const common::RuntimeConfig &config);
} // namespace protocol
} // namespace fluxrail
