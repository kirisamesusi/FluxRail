#include "NormalHid.h"
#include <pico/mutex.h>
#include <string.h>
extern "C" mutex_t __usb_mutex;

namespace fluxrail {
namespace usb {
NormalHid &normalHid() {
  static NormalHid instance;
  return instance;
}
namespace {
// 本机RP2040端口会在软件IRQ中调用tud_task；同核也会被抢占。
// 使用库同一个非阻塞mutex，避免IRQ在报告复制/端点提交中间重入。不长时间关闭中断。
struct UsbGuard {
  bool locked = mutex_try_enter(&__usb_mutex, nullptr);
  ~UsbGuard() {
    if (locked)
      mutex_exit(&__usb_mutex);
  }
};
uint16_t keyboardGet(uint8_t id, hid_report_type_t type, uint8_t *b,
                     uint16_t n) {
  return normalHid().getKeyboard(id, type, b, n);
}
uint16_t mouseGet(uint8_t id, hid_report_type_t type, uint8_t *b, uint16_t n) {
  return normalHid().getMouse(id, type, b, n);
}
void keyboardSet(uint8_t id, hid_report_type_t type, const uint8_t *b,
                 uint16_t n) {
  normalHid().keyboardOutput(id, type, b, n);
}
} // namespace
bool NormalHid::configureBeforeConnect() {
  if (initialized_)
    return false;
  // 该固定人格恰有两个HID实例，默认CFG_TUD_HID=2足够；不无限追加实例。
  if (Adafruit_USBD_HID::getInstanceCount() != 0)
    return false;
  TinyUSBDevice.setManufacturerDescriptor("FluxRail");
  TinyUSBDevice.setProductDescriptor("FluxRail NKRO prototype");
  // 默认serial保持库从RP2040真实Flash唯一ID生成；不写伪固定序号。
  keyboard_.setReportCallback(keyboardGet, keyboardSet);
  mouse_.setReportCallback(mouseGet, nullptr);
  initialized_ = keyboard_.begin() && mouse_.begin();
  return initialized_;
}
bool NormalHid::connectionState(bool &state) const {
  if (!initialized_) {
    state = false;
    return true;
  }
  UsbGuard guard;
  if (!guard.locked)
    return false;
  state = tud_mounted() && !tud_suspended();
  return true;
}
bool NormalHid::quiescent() {
  UsbGuard guard;
  if (!guard.locked)
    return false;
  return initialized_ && !faulted_.load(std::memory_order_acquire) &&
         tud_mounted() && !tud_suspended() && keyboard_.ready() &&
         mouse_.ready();
}
bool NormalHid::detachIfQuiescent() {
  UsbGuard guard;
  return guard.locked && initialized_ &&
         !faulted_.load(std::memory_order_acquire) && tud_mounted() &&
         !tud_suspended() && keyboard_.ready() && mouse_.ready() &&
         TinyUSBDevice.detach();
}
bool NormalHid::readStats(HidStats &out) const {
  UsbGuard guard;
  if (!guard.locked)
    return false;
  out = stats_;
  return true;
}
bool NormalHid::connected() const {
  bool state = false;
  return connectionState(state) && state;
}
bool NormalHid::tryKeyboard(const common::KeyboardState &state) {
  UsbGuard guard;
  if (!guard.locked || !initialized_ || !tud_mounted() || tud_suspended() ||
      faulted_.load(std::memory_order_acquire) || !keyboard_.ready()) {
    ++stats_.busy;
    return false;
  }
  uint8_t bytes[kKeyboardPayloadBytes];
  encodeKeyboard(state, bytes);
  // tud_hid_n_report复制到独立端点缓冲，ready在上一传输完成前为false。
  // 同键UP/DOWN因此不会合并成单个latest状态；失败保持映射器原待发动作。
  if (!keyboard_.sendReport(0, bytes, sizeof(bytes)))
    return false;
  keyboard_state_ = state;
  ++stats_.keyboard_submitted;
  return true;
}
bool NormalHid::tryMouseButtons(uint8_t buttons) {
  UsbGuard guard;
  if (!guard.locked || !initialized_ || !tud_mounted() || tud_suspended() ||
      faulted_.load(std::memory_order_acquire) || !mouse_.ready()) {
    ++stats_.busy;
    return false;
  }
  uint8_t bytes[kMousePayloadBytes];
  encodeRelativeMouse(buttons, 0, 0, bytes);
  if (!mouse_.sendReport(kRelativeReportId, bytes, sizeof(bytes)))
    return false;
  buttons_ = buttons & 7;
  ++stats_.mouse_submitted;
  return true;
}
bool NormalHid::tryRelative(int16_t x, int16_t y) {
  UsbGuard guard;
  if (!guard.locked || !initialized_ || !tud_mounted() || tud_suspended() ||
      faulted_.load(std::memory_order_acquire) || !mouse_.ready()) {
    ++stats_.busy;
    return false;
  }
  uint8_t bytes[kMousePayloadBytes];
  encodeRelativeMouse(buttons_, x, y, bytes);
  if (!mouse_.sendReport(kRelativeReportId, bytes, sizeof(bytes)))
    return false;
  ++stats_.mouse_submitted;
  return true;
}
bool NormalHid::tryAbsolute(float output) {
  uint8_t bytes[kAbsolutePayloadBytes];
  if (!encodeAbsoluteX(output, bytes))
    return false;
  UsbGuard guard;
  if (!guard.locked || !initialized_ || !tud_mounted() || tud_suspended() ||
      faulted_.load(std::memory_order_acquire) || !mouse_.ready())
    return false;
  if (!mouse_.sendReport(kAbsoluteReportId, bytes, sizeof(bytes)))
    return false;
  absolute_x_ = output;
  ++stats_.mouse_submitted;
  return true;
}
void NormalHid::reportCompleted(uint8_t instance, uint16_t bytes) {
  if ((instance == 0 && bytes == kKeyboardPayloadBytes) ||
      (instance == 1 &&
       (bytes == kMousePayloadBytes + 1 || bytes == kAbsolutePayloadBytes + 1)))
    ++stats_.completed;
  else
    reportFailed(instance);
}
void NormalHid::reportFailed(uint8_t instance) {
  if (instance < 2) {
    faulted_.store(true, std::memory_order_release);
    ++stats_.failed;
  }
}
uint16_t NormalHid::getKeyboard(uint8_t id, hid_report_type_t type, uint8_t *b,
                                uint16_t requested) {
  if (id != 0 || !b)
    return 0;
  if (type == HID_REPORT_TYPE_OUTPUT) {
    if (!requested)
      return 0;
    b[0] = host_leds_;
    return 1;
  }
  if (type != HID_REPORT_TYPE_INPUT)
    return 0;
  uint8_t bytes[kKeyboardPayloadBytes];
  encodeKeyboard(keyboard_state_, bytes);
  const uint16_t count = requested < sizeof(bytes) ? requested : sizeof(bytes);
  memcpy(b, bytes, count);
  return count;
}
uint16_t NormalHid::getMouse(uint8_t id, hid_report_type_t type, uint8_t *b,
                             uint16_t requested) {
  if (type != HID_REPORT_TYPE_INPUT || !b)
    return 0;
  uint8_t bytes[kMousePayloadBytes];
  uint16_t size;
  if (id == kRelativeReportId) {
    encodeRelativeMouse(buttons_, 0, 0, bytes);
    size = kMousePayloadBytes;
  } else if (id == kAbsoluteReportId) {
    encodeAbsoluteX(absolute_x_, bytes);
    size = kAbsolutePayloadBytes;
  } else
    return 0;
  const uint16_t count = requested < size ? requested : size;
  memcpy(b, bytes, count);
  return count;
}
void NormalHid::keyboardOutput(uint8_t id, hid_report_type_t type,
                               const uint8_t *b, uint16_t n) {
  if (id == 0 && type == HID_REPORT_TYPE_OUTPUT && b && n == 1)
    host_leds_ = b[0] & 0x1f;
  else
    ++stats_.bad_output;
}
} // namespace usb
} // namespace fluxrail
