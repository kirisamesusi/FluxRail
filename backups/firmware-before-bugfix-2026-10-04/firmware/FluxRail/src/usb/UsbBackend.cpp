#include "UsbBackend.h"
#include <Arduino.h>
#include <pico/mutex.h>
#include <string.h>
extern "C" mutex_t __usb_mutex;
namespace fluxrail {
namespace usb {
UsbBackend &usbBackend() {
  static UsbBackend b;
  return b;
}
namespace {
struct Guard {
  bool locked = mutex_try_enter(&__usb_mutex, nullptr);
  ~Guard() {
    if (locked)
      mutex_exit(&__usb_mutex);
  }
};
uint16_t getIo4(uint8_t id, hid_report_type_t t, uint8_t *p, uint16_t n) {
  return usbBackend().get(id, t, p, n);
}
void setIo4(uint8_t id, hid_report_type_t t, const uint8_t *p, uint16_t n) {
  usbBackend().output(id, t, p, n);
}
} // namespace
bool UsbBackend::configureBeforeConnect(uint8_t mode) {
  if (configuration_attempted_ || mode > 1)
    return false;
  configuration_attempted_ = true;
  mode_ = mode;
  if (!mode_)
    return normalHid().configureBeforeConnect();
  if (Adafruit_USBD_HID::getInstanceCount() != 0)
    return false;
  TinyUSBDevice.setID(0x0ca3, 0x0021);
  TinyUSBDevice.setDeviceVersion(0x0100);
  TinyUSBDevice.setManufacturerDescriptor("SEGA");
  // HID interface and USB product have distinct descriptor positions. Both
  // expose the complete identity;
  // Unique serial stays platform-generated.
  TinyUSBDevice.setProductDescriptor(kIo4ProductName);
  TinyUSBDevice.setConfigurationAttribute(0x80);
  TinyUSBDevice.setConfigurationMaxPower(100);
  io4_.setStringDescriptor(kIo4BoardName);
  io4_.setReportCallback(getIo4, setIo4);
  initialized_ = io4_.begin();
  return initialized_;
}
bool UsbBackend::connectionState(bool &s) const {
  if (!mode_)
    return normalHid().connectionState(s);
  Guard g;
  if (!g.locked)
    return false;
  s = initialized_ && tud_mounted() && !tud_suspended();
  return true;
}
bool UsbBackend::quiescent() {
  if (!mode_)
    return normalHid().quiescent();
  Guard g;
  return g.locked && initialized_ &&
         !faulted_.load(std::memory_order_acquire) && tud_mounted() &&
         !tud_suspended() && io4_.ready();
}
bool UsbBackend::detachIfQuiescent() {
  if (!mode_)
    return normalHid().detachIfQuiescent();
  Guard g;
  return g.locked && initialized_ &&
         !faulted_.load(std::memory_order_acquire) && tud_mounted() &&
         !tud_suspended() && io4_.ready() && TinyUSBDevice.detach();
}
void UsbBackend::applyPending() {
  // Flags belong to core0; payload/state mutation remains under USB mutex.
  if (reset_pending_) {
    state_.disconnect();
    mask_ = 0;
    have_sequence_ = false;
    reset_pending_ = false;
  }
  if (clear_pending_) {
    state_.input.adc[0] = 0x7fff;
    state_.gpio_active = false;
    clear_pending_ = false;
  }
}
bool UsbBackend::setProfile(const common::Profile &p, uint32_t revision) {
  Guard g;
  if (!g.locked)
    return false;
  for (uint8_t i = 0; i < 12; ++i)
    bindings_[i] = p.io4_bindings[i];
  // Whole config adoption is done while public input is pausing.
  revision_ = revision;
  have_sequence_ = false;
  state_.gpio_active = false;
  return true;
}
bool UsbBackend::send(uint16_t mask) {
  applyPending();
  if (!initialized_ || !tud_mounted() || tud_suspended() ||
      faulted_.load(std::memory_order_acquire) || !io4_.ready()) {
    ++stats_.busy;
    return false;
  }
  Io4Input input = state_.input;
  mapIo4Switches(mask, bindings_, input.switches);
  uint8_t b[63];
  encodeIo4(input, b);
  if (!io4_.sendReport(1, b, 63)) {
    ++stats_.busy;
    return false;
  }
  state_.input = input;
  mask_ = mask;
  ++stats_.submitted;
  last_sent_us_ = micros();
  return true;
}
bool UsbBackend::tryPhysicalState(uint16_t mask) {
  Guard g;
  if (!g.locked || !mode_ || (mask & ~common::kSourceMask))
    return false;
  if (!send(mask))
    return false;
  have_sequence_ = false;
  return true;
}
bool UsbBackend::edge(const common::InputEdge &e) {
  Guard g;
  if (!g.locked || !mode_)
    return false;
  if (e.source_id >= 12 || e.revision != revision_ ||
      (have_sequence_ && e.sequence != last_sequence_ + 1u)) {
    faulted_.store(true, std::memory_order_release);
    return false;
  }
  uint16_t bit = uint16_t(1u << e.source_id),
           next = e.down ? uint16_t(mask_ | bit) : uint16_t(mask_ & ~bit);
  if (!send(next))
    return false;
  have_sequence_ = true;
  last_sequence_ = e.sequence;
  return true;
}
bool UsbBackend::absolute(float p, bool valid, uint32_t rev, uint32_t epoch,
                          uint32_t expected_rev, uint32_t expected_epoch) {
  if (!mode_) {
    return valid && rev == expected_rev && epoch == expected_epoch &&
           normalHid().tryAbsolute(p);
  }
  Guard g;
  if (!g.locked)
    return false;
  uint16_t v;
  if (!valid || rev != expected_rev || epoch != expected_epoch ||
      !io4Absolute(p, v)) {
    ++stats_.invalid_motion;
    return false;
  }
  applyPending();
  state_.input.adc[0] = v;
  return true;
}
void UsbBackend::service(uint32_t now, bool streaming) {
  if (!mode_)
    return;
  Guard g;
  if (!g.locked)
    return;
  applyPending();
  state_.tick(now);
  if (!streaming)
    state_.gpio_active = false;
  if (streaming && now - last_sent_us_ >= 1000)
    send(mask_);
}
void UsbBackend::disconnect() {
  if (!mode_)
    return;
  reset_pending_ = true;
  ++stats_.disconnects;
  Guard g;
  if (g.locked)
    applyPending();
}
void UsbBackend::clearMotionAndLights() {
  if (!mode_)
    return;
  clear_pending_ = true;
  Guard g;
  if (g.locked)
    applyPending();
}
void UsbBackend::applyHostLights(lighting::LightFrame &f,
                                 const common::LightingConfig &config) {
  if (!mode_)
    return;
  Guard g;
  if (!g.locked || !state_.gpio_active)
    return;
  const uint8_t channels[18] = {1,  0, 3,  5,  4,  2,  8,  6,  7,
                                11, 9, 10, 14, 12, 13, 17, 15, 16};
  for (uint8_t i = 0; i < 10; ++i) {
    uint8_t function = uint8_t(bindings_[i]);
    if (function < 1 || function > 6)
      continue;
    const uint8_t k = uint8_t((function - 1) * 3);
    auto value = [&](uint8_t c) -> uint8_t {
      return (state_.gpio & (1u << (31 - channels[k + c]))) ? 255 : 0;
    };
    // 主机命令选择颜色，亮度与关灯仍由用户配置控制；与本地动画
    // 使用同一换算，100=实际50%。失效配置安全黑屏，不绕过硬限幅。
    f.keys[i] = config.enabled && lighting::validLightingConfig(config)
                    ? lighting::scaleKeyColor({value(2), value(1), value(0)},
                                              255, config.brightness_level)
                    : common::Rgb{};
  }
}
bool UsbBackend::readStats(Io4Stats &s, uint32_t &r, uint32_t &t) {
  Guard g;
  if (!g.locked)
    return false;
  s = stats_;
  r = state_.rejected;
  t = state_.timeouts;
  return true;
}
uint16_t UsbBackend::get(uint8_t id, hid_report_type_t type, uint8_t *b,
                         uint16_t n) {
  if (!mode_ || !b)
    return 0;
  uint8_t payload[63];
  if (id == 1 && type == HID_REPORT_TYPE_INPUT)
    encodeIo4(state_.input, payload);
  else if (id == 0x10 && type == HID_REPORT_TYPE_OUTPUT) {
    payload[0] = state_.last_output.command;
    memcpy(payload + 1, state_.last_output.payload, 62);
  } else
    return 0;
  const uint16_t count = n < 63 ? n : 63;
  memcpy(b, payload, count);
  return count;
}
void UsbBackend::output(uint8_t id, hid_report_type_t t, const uint8_t *p,
                        uint16_t n) {
  Io4Output out;
  if (!mode_ || !normalizeIo4Output(0, id, uint8_t(t), p, n, out)) {
    ++stats_.bad_output;
    return;
  }
  state_.accept(out, micros());
}
void UsbBackend::completed(uint8_t i, uint16_t n) {
  if (!mode_) {
    normalHid().reportCompleted(i, n);
    return;
  }
  if (i == 0 && n == 64)
    ++stats_.completed;
  else
    failed(i);
}
void UsbBackend::failed(uint8_t i) {
  if (!mode_) {
    normalHid().reportFailed(i);
    return;
  }
  if (i == 0) {
    ++stats_.failed;
    faulted_.store(true, std::memory_order_release);
  }
}
} // namespace usb
} // namespace fluxrail
extern "C" void tud_hid_report_complete_cb(uint8_t i, const uint8_t *,
                                           uint16_t n) {
  fluxrail::usb::usbBackend().completed(i, n);
}
extern "C" void tud_hid_report_failed_cb(uint8_t i, hid_report_type_t,
                                         const uint8_t *, uint16_t) {
  fluxrail::usb::usbBackend().failed(i);
}
