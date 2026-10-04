#include "usb/NormalHid.h"
#include <stddef.h>
using namespace fluxrail;
extern "C" void *memset(void *d, int v, size_t n) {
  auto p = static_cast<uint8_t *>(d);
  for (size_t i = 0; i < n; ++i)
    p[i] = v;
  return d;
}
extern "C" void *memcpy(void *d, const void *s, size_t n) {
  auto p = static_cast<uint8_t *>(d);
  auto q = static_cast<const uint8_t *>(s);
  for (size_t i = 0; i < n; ++i)
    p[i] = q[i];
  return d;
}
void operator delete(void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
extern "C" void __cxa_pure_virtual() {
  for (;;) {
  }
}
// 指令测试不运行系统启动器；仅为项目函数静态单例提供最小C++运行时钩子。
// ARM EABI的guard是32位；不得按桌面Itanium的64位guard覆盖紧邻单例。
extern "C" int __cxa_guard_acquire(uint32_t *guard) { return !*guard; }
extern "C" void __cxa_guard_release(uint32_t *guard) { *guard = 1; }
extern "C" void __cxa_guard_abort(uint32_t *) {}
extern "C" int atexit(void (*)()) { return 0; }
extern "C" int __aeabi_atexit(void *, void (*)(void *), void *) { return 0; }
extern "C" {
void *__dso_handle = nullptr;
mutex_t __usb_mutex;
}
extern "C" void tud_hid_report_complete_cb(uint8_t, const uint8_t *, uint16_t);
extern "C" void tud_hid_report_failed_cb(uint8_t, hid_report_type_t,
                                         const uint8_t *, uint16_t);
volatile uint32_t checks = 0;
#define CHECK(c)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(c))                                                                  \
      return __LINE__;                                                         \
  } while (0)
void complete(uint8_t id) {
  auto &e = mockusb::endpoints[id];
  e.ready = true;
  __usb_mutex.locked = true;
  tud_hid_report_complete_cb(id, e.payload, e.length);
  __usb_mutex.locked = false;
}
extern "C" int test_transport_order_busy_fault_and_mutex() {
  auto &hid = usb::normalHid();
  CHECK(hid.configureBeforeConnect());
  CHECK(!hid.configureBeforeConnect());
  CHECK(mockusb::instances == 2 && hid.startupOk());
  CHECK(mockusb::endpoints[0].descriptor == usb::kKeyboardDescriptor);
  CHECK(mockusb::endpoints[1].descriptor == usb::kMouseDescriptor);
  CHECK(mockusb::endpoints[0].protocol == 0 &&
        mockusb::endpoints[1].protocol == 0);
  CHECK(mockusb::endpoints[0].interval == 1 && !mockusb::endpoints[0].out);
  bool link = false;
  CHECK(hid.connectionState(link) && link);
  common::KeyboardState state;
  state.keys[0] = 0x10;
  state.modifiers = 2;
  CHECK(hid.tryKeyboard(state));
  CHECK(mockusb::endpoints[0].length == 29);
  state.keys[0] = 0; // 外部对象改变不能影响已经复制的报告。
  CHECK(mockusb::endpoints[0].payload[1] == 0x10 &&
        mockusb::endpoints[0].payload[0] == 2);
  CHECK(!hid.tryKeyboard(state)); // UP不能覆盖还没完成的DOWN。
  complete(0);
  CHECK(hid.tryKeyboard(state));
  CHECK(mockusb::endpoints[0].payload[1] == 0);
  state.keys[0] = 0x10;
  CHECK(!hid.tryKeyboard(state));
  complete(0);
  CHECK(hid.tryKeyboard(state)); // 独立完成之后才接受新的DOWN。
  complete(0);
  mockusb::reject_next = true;
  state.keys[0] = 0;
  CHECK(!hid.tryKeyboard(state));
  uint8_t get[29]{};
  __usb_mutex.locked = true;
  CHECK(hid.getKeyboard(0, HID_REPORT_TYPE_INPUT, get, 29) == 29 &&
        get[1] == 0x10);
  __usb_mutex.locked = false;
  CHECK(hid.tryKeyboard(state)); // 失败没有改cached状态，可以原样重试。
  complete(0);
  __usb_mutex.locked = true;
  tud_hid_report_failed_cb(0, HID_REPORT_TYPE_INPUT, nullptr, 29);
  __usb_mutex.locked = false;
  CHECK(hid.transportFaulted() && !hid.tryKeyboard(state));
  hid.clearTransportFault();
  CHECK(hid.tryKeyboard(state));
  complete(0);
  __usb_mutex.locked = true;
  link = true;
  CHECK(!hid.connectionState(link) && link);
  CHECK(!hid.tryKeyboard(state));
  usb::HidStats stats;
  CHECK(!hid.readStats(stats));
  __usb_mutex.locked = false;
  mockusb::mounted = false;
  CHECK(hid.connectionState(link) && !link);
  CHECK(!hid.tryKeyboard(state));
  mockusb::mounted = true;
  mockusb::suspended = true;
  CHECK(!hid.tryKeyboard(state));
  mockusb::suspended = false;
  CHECK(hid.readStats(stats) && stats.failed == 1 && stats.completed == 5 &&
        stats.keyboard_submitted == 5);
  CHECK(mockusb::unlocked_access == 0 && !__usb_mutex.locked);
  return 0;
}
extern "C" int test_transport_mouse_owner_and_control_bounds() {
  auto &hid = usb::normalHid();
  CHECK(hid.configureBeforeConnect());
  CHECK(hid.tryMouseButtons(5));
  CHECK(!hid.tryRelative(-1, 256));
  complete(1);
  CHECK(hid.tryRelative(-1, 256));
  const auto &payload = mockusb::endpoints[1].payload;
  CHECK(payload[0] == 1 && payload[1] == 5 && payload[2] == 0xff && payload[3] == 0xff &&
        payload[4] == 0 && payload[5] == 1);
  complete(1);
  mockusb::reject_next = true;
  CHECK(!hid.tryMouseButtons(2));
  uint8_t bytes[32];
  for (auto &b : bytes)
    b = 0x7f;
  __usb_mutex.locked = true;
  CHECK(hid.getMouse(1, HID_REPORT_TYPE_INPUT, bytes, 2) == 2 &&
        bytes[0] == 5 && bytes[1] == 0 && bytes[2] == 0x7f);
  CHECK(hid.getMouse(1, HID_REPORT_TYPE_INPUT, bytes, 32) == 5 &&
        bytes[5] == 0x7f);
  CHECK(hid.getMouse(0, HID_REPORT_TYPE_INPUT, bytes, 5) == 0);
  CHECK(hid.getMouse(0, HID_REPORT_TYPE_OUTPUT, bytes, 5) == 0);
  CHECK(hid.getKeyboard(0, HID_REPORT_TYPE_INPUT, nullptr, 29) == 0);
  CHECK(hid.getKeyboard(0, HID_REPORT_TYPE_INPUT, bytes, 0) == 0);
  const uint8_t leds = 0xff;
  hid.keyboardOutput(0, HID_REPORT_TYPE_OUTPUT, &leds, 1);
  CHECK(hid.getKeyboard(0, HID_REPORT_TYPE_OUTPUT, bytes, 32) == 1 &&
        bytes[0] == 0x1f);
  CHECK(hid.getKeyboard(0, HID_REPORT_TYPE_OUTPUT, bytes, 0) == 0);
  hid.keyboardOutput(1, HID_REPORT_TYPE_OUTPUT, &leds, 1);
  hid.keyboardOutput(0, HID_REPORT_TYPE_OUTPUT, nullptr, 1);
  __usb_mutex.locked = false;
  CHECK(hid.tryRelative(0, -256));
  CHECK(mockusb::endpoints[1].payload[0] == 1 && mockusb::endpoints[1].payload[1] == 5 &&
        mockusb::endpoints[1].payload[5] == 0xff);
  usb::HidStats stats;
  CHECK(hid.readStats(stats) && stats.bad_output == 2);
  CHECK(mockusb::unlocked_access == 0 && !__usb_mutex.locked);
  return 0;
}
