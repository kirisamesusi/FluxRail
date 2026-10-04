#include "FlashRp2040.h"
#include <Arduino.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <hardware/watchdog.h>
#include <string.h>
#ifndef FLUXRAIL_STORAGE_PARTITION
#error                                                                         \
    "Use tests/integration/compile_firmware.py with reserved independent Flash slots"
#endif
extern "C" uint8_t __flash_binary_end, _EEPROM_start, _FS_start, _FS_end;
namespace fluxrail {
namespace config {
bool FlashRp2040::available() const {
  return uintptr_t(&__flash_binary_end) <= XIP_BASE + kSlotOffset[0] &&
         uintptr_t(&_EEPROM_start) == XIP_BASE + 0x3ff000 &&
         uintptr_t(&_FS_start) == uintptr_t(&_FS_end);
}
bool FlashRp2040::read(uint8_t slot, uint32_t off, uint8_t *data,
                       uint32_t len) {
  if (!available() || slot > 1 || off > kSlotBytes || len > kSlotBytes - off)
    return false;
  memcpy(data,
         reinterpret_cast<const void *>(XIP_BASE + kSlotOffset[slot] + off),
         len);
  return true;
}
bool FlashRp2040::erase(uint8_t slot) {
  if (!available() || slot > 1)
    return false;
  for (uint32_t off = 0; off < kSlotBytes; off += FLASH_SECTOR_SIZE) {
    watchdog_update();
    const uint32_t irq = save_and_disable_interrupts();
    rp2040.idleOtherCore();
    flash_range_erase(kSlotOffset[slot] + off, FLASH_SECTOR_SIZE);
    rp2040.resumeOtherCore();
    restore_interrupts(irq);
  }
  return true;
}
bool FlashRp2040::program(uint8_t slot, uint32_t off, const uint8_t *data,
                          uint32_t len) {
  if (!available() || slot > 1 || off % 256 || len != 256 ||
      off > kSlotBytes - len)
    return false;
  if (uintptr_t(data) < 0x20000000 || uintptr_t(data) > 0x20042000 - len)
    return false;
  watchdog_update();
  const uint32_t irq = save_and_disable_interrupts();
  rp2040.idleOtherCore();
  flash_range_program(kSlotOffset[slot] + off, data, len);
  rp2040.resumeOtherCore();
  restore_interrupts(irq);
  return true;
}
} // namespace config
} // namespace fluxrail
