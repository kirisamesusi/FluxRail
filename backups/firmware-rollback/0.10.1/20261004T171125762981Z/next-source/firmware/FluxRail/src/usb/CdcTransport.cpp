#include "CdcTransport.h"
#include <Adafruit_TinyUSB.h>
#include <pico/mutex.h>
extern "C" mutex_t __usb_mutex;
extern "C" bool fluxrail_cdc_tx_idle(uint8_t);
namespace fluxrail {
namespace usb {
namespace {
struct UsbGuard {
  bool locked = mutex_try_enter(&__usb_mutex, nullptr);
  ~UsbGuard() {
    if (locked)
      mutex_exit(&__usb_mutex);
  }
};
} // namespace
bool CdcTransport::connectionState(bool &online) {
  UsbGuard guard;
  if (!guard.locked)
    return false;
  online = tud_mounted() && !tud_suspended() && tud_cdc_n_connected(0);
  if (!online) {
    tud_cdc_n_read_flush(0);
    tud_cdc_n_write_clear(0);
  }
  return true;
}
bool CdcTransport::txComplete() {
  UsbGuard guard;
  return guard.locked && tud_mounted() && !tud_suspended() && tud_cdc_n_connected(0) && fluxrail_cdc_tx_idle(0);
}
int32_t CdcTransport::readByte() {
  UsbGuard guard;
  if (!guard.locked || !tud_mounted() || tud_suspended() ||
      !tud_cdc_n_connected(0))
    return -1;
  uint8_t byte;
  return tud_cdc_n_read(0, &byte, 1) == 1 ? byte : -1;
}
uint32_t CdcTransport::write(const char *data, uint32_t length) {
  UsbGuard guard;
  if (!guard.locked || !tud_mounted() || tud_suspended() ||
      !tud_cdc_n_connected(0))
    return 0;
  const uint32_t available = tud_cdc_n_write_available(0);
  const uint32_t count = length < available ? length : available;
  if (!count)
    return 0;
  const uint32_t copied = tud_cdc_n_write(0, data, count);
  tud_cdc_n_write_flush(
      0); // Starts an available transfer; TinyUSB never waits here.
  return copied;
}
} // namespace usb
} // namespace fluxrail
