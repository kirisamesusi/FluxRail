#pragma once
#include "../common/Io4Types.h"
#include <stddef.h>
namespace fluxrail {
namespace usb {
constexpr uint8_t kIo4InId = 1, kIo4OutId = 0x10;
constexpr uint16_t kIo4PayloadBytes = 63, kIo4WireBytes = 64;
static_assert(8 * 2 + 4 * 2 + 2 * 2 + 2 * 2 + 2 + 29 == kIo4PayloadBytes,
              "IO4 payload");
struct Io4Input {
  uint16_t adc[8]{0x7fff}, spinner[4]{}, coin[2]{}, switches[2]{0x4000, 0x8000};
  uint8_t system = 2, usb = 0;
};
struct Io4Output {
  uint8_t command = 0, payload[62]{};
};
static_assert(sizeof(Io4Output) == 63, "OUT values size");
void encodeIo4(const Io4Input &state, uint8_t (&out)[63]);
bool normalizeIo4Output(uint8_t instance, uint8_t id, uint8_t type,
                        const uint8_t *bytes, uint16_t size, Io4Output &out);
void mapIo4Switches(uint16_t mask, const common::Io4Function (&bindings)[12],
                    uint16_t (&out)[2]);
bool io4Absolute(float output, uint16_t &adc);
// Protocol behavior independently implemented from local fixed snapshots.
class Io4State {
public:
  Io4Input input{};
  Io4Output last_output{};
  uint32_t rejected = 0, supported = 0, timeouts = 0;
  uint8_t timeout_units = 0, sampling_count = 0;
  uint32_t gpio = 0, last_command_us = 0;
  bool gpio_active = false;
  bool accept(const Io4Output &out, uint32_t now);
  void tick(uint32_t now);
  void disconnect();
};
extern const uint8_t kIo4Descriptor[];
extern const uint16_t kIo4DescriptorBytes;
extern const char kIo4BoardName[];
extern const char kIo4ProductName[];
} // namespace usb
} // namespace fluxrail
