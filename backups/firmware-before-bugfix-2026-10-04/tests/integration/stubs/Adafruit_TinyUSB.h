#pragma once
// 仅测试边界替身。执行真实NormalHid.cpp；不声称模拟TinyUSB控制请求或USB总线。
#include <pico/mutex.h>
#include <stdint.h>
#define FLUXRAIL_TINYUSB_STARTUP_ADAPTER 2
constexpr uint8_t HID_ITF_PROTOCOL_NONE = 0;
enum hid_report_type_t {
  HID_REPORT_TYPE_INVALID,
  HID_REPORT_TYPE_INPUT,
  HID_REPORT_TYPE_OUTPUT,
  HID_REPORT_TYPE_FEATURE
};
extern "C" mutex_t __usb_mutex;
namespace mockusb {
struct Endpoint {
  bool ready = true;
  uint8_t payload[64]{};
  uint16_t length = 0;
  const uint8_t *descriptor = nullptr;
  uint16_t descriptor_bytes = 0;
  uint8_t protocol = 0xff, interval = 0;
  bool out = false;
};
inline Endpoint endpoints[2]{};
inline bool mounted = true, suspended = false, reject_next = false;
inline uint8_t instances = 0;
inline uint32_t unlocked_access = 0;
inline void checkGuard() {
  if (!__usb_mutex.locked)
    ++unlocked_access;
}
} // namespace mockusb
inline bool tud_mounted() {
  mockusb::checkGuard();
  return mockusb::mounted;
}
inline bool tud_suspended() {
  mockusb::checkGuard();
  return mockusb::suspended;
}
class Adafruit_USBD_HID {
public:
  using Get = uint16_t (*)(uint8_t, hid_report_type_t, uint8_t *, uint16_t);
  using Set = void (*)(uint8_t, hid_report_type_t, const uint8_t *, uint16_t);
  Adafruit_USBD_HID(const uint8_t *d, uint16_t n, uint8_t p, uint8_t interval,
                    bool out)
      : d_(d), n_(n), p_(p), interval_(interval), out_(out) {}
  static uint8_t getInstanceCount() { return mockusb::instances; }
  void setReportCallback(Get get, Set set) {
    get_ = get;
    set_ = set;
  }
  bool begin() {
    if (mockusb::instances >= 2)
      return false;
    instance_ = mockusb::instances++;
    auto &e = mockusb::endpoints[instance_];
    e.descriptor = d_;
    e.descriptor_bytes = n_;
    e.protocol = p_;
    e.interval = interval_;
    e.out = out_;
    return true;
  }
  void setStringDescriptor(const char *) {}
  bool ready() const {
    mockusb::checkGuard();
    return instance_ < 2 && mockusb::endpoints[instance_].ready;
  }
  bool sendReport(uint8_t id, const void *bytes, uint16_t n) {
    mockusb::checkGuard();
    if (instance_ >= 2 || n+(id?1:0) > 64 || !mockusb::endpoints[instance_].ready)
      return false;
    if (mockusb::reject_next) {
      mockusb::reject_next = false;
      return false;
    }
    auto &e = mockusb::endpoints[instance_];
    const auto p = static_cast<const uint8_t *>(bytes);
    for (uint16_t i = 0; i < n; ++i)
      e.payload[i+(id?1:0)] = p[i];
    if(id)e.payload[0]=id;
    e.length = n+(id?1:0);
    e.ready = false;
    return true;
  }

private:
  const uint8_t *d_;
  uint16_t n_;
  uint8_t p_, interval_;
  bool out_;
  uint8_t instance_ = 0xff;
  Get get_ = nullptr;
  Set set_ = nullptr;
};
struct MockDevice {
  bool detach_ok=true,detached=false;
  void setID(uint16_t,uint16_t) {}
  void setDeviceVersion(uint16_t) {}
  void setConfigurationAttribute(uint8_t) {}
  void setConfigurationMaxPower(uint16_t) {}
  bool detach() { if(!detach_ok)return false;detached=true;return true; }
  const char *manufacturer = nullptr;
  const char *product = nullptr;
  void setManufacturerDescriptor(const char *s) { manufacturer = s; }
  void setProductDescriptor(const char *s) { product = s; }
};
inline MockDevice TinyUSBDevice;

namespace mockcdc {
inline bool connected = true;
inline uint8_t rx[256]{}, tx[256]{};
inline uint32_t rx_n = 0, rx_pos = 0, tx_n = 0, capacity = 64, copy_limit = 64,
                flushes = 0;
} // namespace mockcdc
inline bool tud_cdc_n_connected(uint8_t) {
  mockusb::checkGuard();
  return mockcdc::connected;
}
inline uint32_t tud_cdc_n_read(uint8_t, void *out, uint32_t size) {
  mockusb::checkGuard();
  uint32_t n = 0;
  auto p = static_cast<uint8_t *>(out);
  while (n < size && mockcdc::rx_pos < mockcdc::rx_n)
    p[n++] = mockcdc::rx[mockcdc::rx_pos++];
  return n;
}
inline void tud_cdc_n_read_flush(uint8_t) {
  mockusb::checkGuard();
  mockcdc::rx_pos = mockcdc::rx_n;
}
inline bool tud_cdc_n_write_clear(uint8_t) {
  mockusb::checkGuard();
  mockcdc::tx_n = 0;
  return true;
}
inline uint32_t tud_cdc_n_write_available(uint8_t) {
  mockusb::checkGuard();
  return mockcdc::capacity;
}
inline uint32_t tud_cdc_n_write(uint8_t, const void *data, uint32_t size) {
  mockusb::checkGuard();
  if (size > mockcdc::copy_limit)
    size = mockcdc::copy_limit;
  auto p = static_cast<const uint8_t *>(data);
  for (uint32_t i = 0; i < size; ++i)
    mockcdc::tx[mockcdc::tx_n++] = p[i];
  return size;
}
inline uint32_t tud_cdc_n_write_flush(uint8_t) {
  mockusb::checkGuard();
  ++mockcdc::flushes;
  return 0;
}

namespace mockcdc {inline bool bus_idle=true;}
extern "C" inline bool fluxrail_cdc_tx_idle(uint8_t) {mockusb::checkGuard();return mockcdc::bus_idle;}
