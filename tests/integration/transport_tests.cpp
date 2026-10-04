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
extern "C" int test_absolute_xy_center_y_preservation_failure_and_bounds() {
  auto &hid = usb::normalHid();
  CHECK(hid.configureBeforeConnect());
  CHECK(hid.setAbsoluteMode(true));
  uint8_t cached[8]{};
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,8)==5);
  CHECK(cached[0]==0 && cached[1]==0 && cached[2]==64 && cached[3]==0 && cached[4]==64);
  CHECK(hid.tryAbsolute(0.25f));
  const auto &e = mockusb::endpoints[1];
  CHECK(e.length==6 && e.payload[0]==2 && e.payload[1]==0);
  CHECK(e.payload[2]==0 && e.payload[3]==32 && e.payload[4]==0 && e.payload[5]==64);
  CHECK(!hid.tryRelative(0,64)); // 忙时不可修改Y。
  complete(1);
  mockusb::reject_next=true;
  CHECK(!hid.tryRelative(0,64));
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,8)==5 && cached[3]==0 && cached[4]==64);
  CHECK(hid.tryRelative(0,64));
  CHECK(e.payload[2]==0 && e.payload[3]==32 && e.payload[4]==0 && e.payload[5]==72);
  complete(1);
  CHECK(hid.tryAbsolute(0.75f));
  CHECK(e.payload[2]==255 && e.payload[3]==95 && e.payload[4]==0 && e.payload[5]==72);
  complete(1);
  CHECK(hid.tryRelative(32767,32767));
  CHECK(e.payload[2]==255 && e.payload[3]==127 && e.payload[4]==255 && e.payload[5]==127);
  complete(1);
  CHECK(hid.tryRelative(0,-1)); // 边界立即反向，不能积攒边界外余量。
  CHECK(e.payload[4]==223 && e.payload[5]==127);
  complete(1);
  CHECK(hid.tryRelative(-32768,-32768));
  CHECK(e.payload[2]==0 && e.payload[3]==0 && e.payload[4]==0 && e.payload[5]==0);
  complete(1);
  CHECK(hid.tryRelative(0,1));
  CHECK(e.payload[4]==32 && e.payload[5]==0);
  complete(1);
  CHECK(hid.tryMouseButtons(5)); // 真按钮只在相对TLC，绝对兼容按钮仍释放。
  CHECK(e.payload[0]==1 && e.payload[1]==5);
  complete(1);
  CHECK(hid.tryAbsolute(0.5f));
  CHECK(e.payload[0]==2 && e.payload[1]==0 && e.payload[4]==32 && e.payload[5]==0);
  complete(1);
  CHECK(hid.setAbsoluteMode(false));
  CHECK(hid.tryRelative(-12,31));
  CHECK(e.payload[0]==1 && e.payload[1]==5 && e.payload[2]==244 && e.payload[4]==31);
  complete(1);
  CHECK(hid.setAbsoluteMode(true));
  CHECK(hid.tryAbsolute(0.1f));
  CHECK(e.payload[4]==32 && e.payload[5]==0); // 切模式不重置Y。
  CHECK(!hid.tryAbsolute(0.9f, 3)); // 联合报告忙时，两轴都不能提前提交。
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,8)==5 &&
        cached[1]==205 && cached[2]==12 && cached[3]==32 && cached[4]==0);
  complete(1);
  mockusb::reject_next=true;
  CHECK(!hid.tryAbsolute(0.9f, 3));
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,8)==5 &&
        cached[1]==205 && cached[2]==12 && cached[3]==32 && cached[4]==0);
  CHECK(hid.tryAbsolute(0.9f, 3)); // 原样重试只增加一次Y。
  CHECK(e.payload[2]==50 && e.payload[3]==115 && e.payload[4]==128 && e.payload[5]==0);
  complete(1);
  for(auto &b:cached) b=0x7f;
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,3)==3 && cached[3]==0x7f);
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,cached,0)==0);
  CHECK(hid.getMouse(2,HID_REPORT_TYPE_INPUT,nullptr,5)==0);
  __usb_mutex.locked=true;
  CHECK(!hid.setAbsoluteMode(false));
  __usb_mutex.locked=false;
  // 同长不同ID合法完成；旧3字节绝对完成已不合法。
  hid.reportCompleted(1,3);
  CHECK(hid.transportFaulted());
  hid.clearTransportFault();
  CHECK(!hid.tryAbsolute(-1) && !hid.tryAbsolute(2));
  usb::HidStats stats;
  CHECK(hid.readStats(stats) && stats.failed==1);
  return 0;
}
extern "C" int test_absolute_xy_async_failure_restore_without_new_motion() {
  auto &hid = usb::normalHid();
  CHECK(hid.configureBeforeConnect() && hid.setAbsoluteMode(true));
  hid.requestAbsoluteRestore();
  hid.service(true);
  CHECK(mockusb::endpoints[1].ready); // 尚无有效提交，不能凭恢复标志伪造居中运动。
  CHECK(hid.tryAbsolute(0.25f, 64));
  auto &e = mockusb::endpoints[1];
  __usb_mutex.locked = true;
  tud_hid_report_failed_cb(1,HID_REPORT_TYPE_INPUT,e.payload,e.length);
  __usb_mutex.locked = false;
  e.ready = true;
  hid.service(true);
  CHECK(hid.transportFaulted() && e.ready);
  hid.clearTransportFault();
  hid.service(false);
  CHECK(e.ready); // 维护中不能重发。
  mockusb::reject_next=true;
  hid.service(true);
  CHECK(e.ready); // 同步拒绝保留待重发。
  hid.service(true);
  CHECK(!e.ready && e.payload[0]==2 && e.payload[2]==0 && e.payload[3]==32 &&
        e.payload[4]==0 && e.payload[5]==72); // 仍18432，不二次增加64counts。
  complete(1);
  hid.service(true);
  CHECK(e.ready); // 成功后的静止状态没有无休止重发。
  hid.requestAbsoluteRestore(); // 断连恢复使用同一幂等入口。
  CHECK(hid.setAbsoluteMode(false));
  hid.service(true); CHECK(e.ready);
  CHECK(hid.setAbsoluteMode(true));
  __usb_mutex.locked=true;
  hid.service(true);
  __usb_mutex.locked=false;
  CHECK(e.ready);
  hid.service(true);
  CHECK(!e.ready && e.payload[4]==0 && e.payload[5]==72);
  complete(1);
  usb::HidStats stats;
  CHECK(hid.readStats(stats) && stats.failed==1 && stats.mouse_submitted==3);
  CHECK(mockusb::unlocked_access==0);
  return 0;
}
