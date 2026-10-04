#include "Io4Codec.h"
#include <math.h>
#include <string.h>
namespace fluxrail {
namespace usb {
const char kIo4BoardName[] = "I/O CONTROL "
                             "BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_"
                             "ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6;";
// SEGA835Lib product parser expects eight semicolon-separated fields; the
// interface reference includes a final delimiter. Keep distinct positions.
const char kIo4ProductName[] = "I/O CONTROL "
                               "BD;15257;01;90;1831;6679A;00;GOUT=14_ADIN=8,E_"
                               "ROTIN=4_COININ=2_SWIN=2,E_UQ1=41,6";
// Fourteen 16-bit analog values, forty-eight switch/status bits,29 reserved
// bytes. Both reports are exactly ID +63 bytes; No in-memory struct is sent.
const uint8_t kIo4Descriptor[] = {
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0x85, 0x01, 0x09, 0x01, 0xa1, 0x00,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x30, 0x09, 0x31, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34, 0x09, 0x33, 0x09, 0x34,
    0x09, 0x36, 0x09, 0x36, 0x15, 0,    0x27, 0xff, 0xff, 0,    0,    0x35,
    0,    0x47, 0xff, 0xff, 0,    0,    0x95, 14,   0x75, 16,   0x81, 2,
    0xc0, 0x05, 0x09, 0x19, 1,    0x29, 48,   0x15, 0,    0x25, 1,    0x45,
    1,    0x75, 1,    0x95, 48,   0x81, 2,    0x09, 0,    0x75, 8,    0x95,
    29,   0x81, 1,    0x06, 0xa0, 0xff, 0x09, 0,    0x85, 0x10, 0xa1, 1,
    0x09, 0,    0x15, 0,    0x26, 0xff, 0,    0x75, 8,    0x95, 63,   0x91,
    2,    0xc0, 0xc0};
const uint16_t kIo4DescriptorBytes = sizeof(kIo4Descriptor);
void encodeIo4(const Io4Input &s, uint8_t (&b)[63]) {
  memset(b, 0, 63);
  uint8_t n = 0;
  auto word = [&](uint16_t v) {
    b[n++] = uint8_t(v);
    b[n++] = uint8_t(v >> 8);
  };
  for (auto v : s.adc)
    word(v);
  for (auto v : s.spinner)
    word(v);
  for (auto v : s.coin)
    word(v);
  for (auto v : s.switches)
    word(v);
  b[n++] = s.system;
  b[n] = s.usb;
}
bool normalizeIo4Output(uint8_t instance, uint8_t id, uint8_t type,
                        const uint8_t *b, uint16_t n, Io4Output &out) {
  if (instance != 0 || type != 2 || !b)
    return false;
  if (id == 0) {
    if (n != 64 || b[0] != 0x10)
      return false;
    ++b;
  } else if (id != 0x10 || n != 63)
    return false;
  out.command = b[0];
  memcpy(out.payload, b + 1, 62);
  return true;
}
void mapIo4Switches(uint16_t mask, const common::Io4Function (&b)[12],
                    uint16_t (&out)[2]) {
  out[0] = 0x4000;
  out[1] = 0x8000;
  const uint8_t word[] = {0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0};
  const uint8_t bit[] = {0, 0, 5, 4, 1, 0, 15, 14, 13, 15, 14, 9, 6};
  for (uint8_t i = 0; i < 12; ++i) {
    const uint8_t f = uint8_t(b[i]);
    if (!(mask & (1u << i)) || !f || f > 12)
      continue;
    const uint16_t v = uint16_t(1u << bit[f]);
    if (f == 9 || f == 10)
      out[word[f]] &= uint16_t(~v);
    else
      out[word[f]] |= v;
  }
}
bool io4Absolute(float p, uint16_t &adc) {
  if (!isfinite(p) || p < 0 || p > 1)
    return false;
  adc = uint16_t(p * 65535.0f + 0.5f);
  return true;
}
bool Io4State::accept(const Io4Output &o, uint32_t now) {
  switch (o.command) {
  case 1:
    timeout_units = o.payload[0];
    input.system = 0x30;
    break;
  case 2:
    sampling_count = o.payload[0];
    input.system = 0x30;
    break;
  case 3:
    input.system = 0;
    input.usb = 0;
    input.coin[0] = input.coin[1] = 0;
    break;
  case 4:
    // GPIO bit stream is MSB first, unlike the little endian IN words.
    gpio = (uint32_t(o.payload[0]) << 24) | (uint32_t(o.payload[1]) << 16) |
           (uint32_t(o.payload[2]) << 8) | o.payload[3];
    gpio_active = true;
    break;
  default:
    ++rejected;
    return false;
  }
  last_output = o;
  last_command_us = now;
  ++supported;
  return true;
}
void Io4State::tick(uint32_t now) {
  if (timeout_units &&
      now - last_command_us >= uint32_t(timeout_units) * 100000u) {
    input.system = 2;
    gpio_active = false;
    timeout_units = 0;
    ++timeouts;
  }
}
void Io4State::disconnect() {
  input = Io4Input{};
  last_output = Io4Output{};
  timeout_units = sampling_count = 0;
  gpio = 0;
  gpio_active = false;
}
} // namespace usb
} // namespace fluxrail
