#pragma once
#include <stddef.h>
#include <stdint.h>

// Release version: change only this value before building the formal UF2.
#define FLUXRAIL_FIRMWARE_VERSION "0.10.0"

namespace fluxrail {
namespace firmware {
// Fixed little-endian layout in RP2040 Flash, also read by the Web UF2 checker.
struct FirmwareInfo {
  char magic[16];
  uint32_t format;
  char board[32];
  char version[32];
  uint32_t end_magic;
};
static_assert(sizeof(FirmwareInfo) == 88 && offsetof(FirmwareInfo, version) == 52,
              "Firmware metadata layout changed");
static_assert(sizeof(FLUXRAIL_FIRMWARE_VERSION) <= 32, "Version too long");
inline const FirmwareInfo &info() {
  static const FirmwareInfo record = {"FLUXRAIL-FW-V1", 1,
                                     "fluxrail-rp2040-4mib",
                                     FLUXRAIL_FIRMWARE_VERSION, 0x46525746};
  // A volatile pointer keeps the complete record reachable under LTO/GC, rather
  // than allowing the compiler to replace the hello fields with string literals.
  static const FirmwareInfo *volatile retained = &record;
  return *retained;
}
} // namespace firmware
} // namespace fluxrail
