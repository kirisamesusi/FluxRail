#include "app/App.h"
#include "app/ConfigCoordinator.h"
#include "protocol/ConfigProtocol.h"
#include "realtime/InputWorker.h"
#include "usb/CdcTransport.h"
#include "usb/NormalHid.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
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
extern "C" size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n])
    ++n;
  return n;
}
extern "C" int memcmp(const void *a, const void *b, size_t n) {
  auto p = static_cast<const uint8_t *>(a);
  auto q = static_cast<const uint8_t *>(b);
  for (size_t i = 0; i < n; ++i)
    if (p[i] != q[i])
      return int(p[i]) - int(q[i]);
  return 0;
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

volatile uint32_t checks = 0;
#define CHECK(c)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(c))                                                                  \
      return __LINE__;                                                         \
  } while (0)
size_t length(const char *s) {
  size_t n = 0;
  while (s[n])
    ++n;
  return n;
}
bool contains(const char *s, const char *part) {
  for (size_t i = 0; s[i]; ++i) {
    size_t j = 0;
    while (part[j] && s[i + j] && s[i + j] == part[j])
      ++j;
    if (!part[j])
      return true;
  }
  return false;
}
struct Port : protocol::CdcPort {
  char rx[12000]{}, tx[16000]{};
  uint32_t n = 0, pos = 0, out = 0, max_read_tick = 0, max_write_tick = 0,
           reads = 0;
  uint32_t write_limit = 64;
  bool online = true, observed = true;
  bool connectionState(bool &v) override {
    if (!observed)
      return false;
    v = online;
    return true;
  }
  int32_t readByte() override {
    ++reads;
    return pos < n ? static_cast<uint8_t>(rx[pos++]) : -1;
  }
  uint32_t write(const char *p, uint32_t count) override {
    if (count > max_write_tick)
      max_write_tick = count;
    if (count > write_limit)
      count = write_limit;
    for (uint32_t i = 0; i < count && out + 1 < sizeof(tx); ++i)
      tx[out++] = p[i];
    tx[out] = 0;
    return count;
  }
  void append(const char *s) {
    while (*s && n < sizeof(rx))
      rx[n++] = *s++;
  }
  void clearOutput() {
    out = 0;
    tx[0] = 0;
  }
};
struct Sink : keyboard::ActionSink {
  bool blocked = false;
  bool tryKeyboard(const common::KeyboardState &) override { return !blocked; }
  bool tryMouseButtons(uint8_t) override { return !blocked; }
};
struct Host : protocol::ConfigHost {
  realtime::RealtimeBridge bridge;
  keyboard::ActionMapper mapper;
  realtime::InputWorker worker{bridge};
  app::ConfigCoordinator coordinator{bridge, mapper};
  Sink sink;
  common::RuntimeConfig requested;
  uint32_t now = 0, apply_calls = 0;
  bool accept = true;
  Host() {
    requested.keyboard = keyboard::defaultKeyboardConfig();
    worker.begin(0, 0);
    coordinator.start(requested, now);
    for (uint8_t i = 0; i < 30; ++i)
      tick();
  }
  void tick() {
    now += 500;
    worker.service(0, now);
    coordinator.service(now, sink);
  }
  const common::RuntimeConfig &appliedConfig() const override {
    return coordinator.appliedConfig();
  }
  uint32_t requestedRevision() const override { return requested.revision; }
  bool ready() const override { return coordinator.ready(); }
  bool busy() const override { return coordinator.busy(); }
  bool failed() const override { return coordinator.failed(); }
  uint16_t physicalMask() const override { return 0x81; }
  const char *serial() const override { return "0123456789ABCDEF"; }
  bool apply(const common::RuntimeConfig &c, uint32_t time) override {
    ++apply_calls;
    if (!accept || !coordinator.start(c, time))
      return false;
    requested = c;
    return true;
  }
};
void service(protocol::ConfigProtocol &p, Port &port, Host &h,
             bool tick = true) {
  if (tick)
    h.tick();
  const auto before = port.reads;
  p.service(h.now, port, h);
  const auto cost = port.reads - before;
  if (cost > port.max_read_tick)
    port.max_read_tick = cost;
}
void drain(protocol::ConfigProtocol &p, Port &port, Host &h,
           uint32_t loops = 120) {
  for (uint32_t i = 0; i < loops; ++i)
    service(p, port, h);
}
void request(Port &p, uint32_t id, const char *op, const char *payload = "{}",
             uint32_t version = 1) {
  protocol::JsonWriter w(p.rx + p.n, sizeof(p.rx) - p.n);
  w.literal("{\"protocol_version\":");
  w.number(version);
  w.literal(",\"id\":");
  w.number(id);
  w.literal(",\"op\":\"");
  w.literal(op);
  w.literal("\",\"payload\":");
  w.literal(payload);
  w.literal("}\n");
  p.n += w.size();
}
void setRequest(Port &p, const common::RuntimeConfig &c, uint32_t id,
                uint32_t revision) {
  protocol::JsonWriter w(p.rx + p.n, sizeof(p.rx) - p.n);
  w.literal("{\"protocol_version\":1,\"id\":");
  w.number(id);
  w.literal(",\"op\":\"set_config\",\"payload\":{\"expected_revision\":");
  w.number(revision);
  w.literal(",\"config\":");
  protocol::encodeRuntime(w, c);
  w.literal("}}\n");
  p.n += w.size();
}
struct MemoryFlash : config::FlashDevice {
  uint8_t bytes[2][8192];
  int cutoff = -1, operations = 0, partial = 0;
  bool enabled = true, bad_read = false, corrupt_write = false;
  MemoryFlash() { memset(bytes, 255, sizeof(bytes)); }
  bool available() const override { return enabled; }
  bool read(uint8_t s, uint32_t off, uint8_t *d, uint32_t n) override {
    if (bad_read || s > 1 || off + n > 8192)
      return false;
    memcpy(d, bytes[s] + off, n);
    return true;
  }
  bool interrupted() {
    const bool fail = cutoff >= 0 && operations == cutoff;
    ++operations;
    return fail;
  }
  bool erase(uint8_t s) override {
    for (uint32_t off = 0; off < 8192; off += 4096) {
      if (interrupted()) {
        memset(bytes[s] + off, 255, partial);
        return false;
      }
      memset(bytes[s] + off, 255, 4096);
    }
    return true;
  }
  bool program(uint8_t s, uint32_t off, const uint8_t *d, uint32_t n) override {
    bool fail = interrupted();
    uint32_t count = fail ? uint32_t(partial) : n;
    if (count > n)
      count = n;
    for (uint32_t i = 0; i < count; ++i)
      bytes[s][off + i] &= d[i];
    if (corrupt_write)
      bytes[s][off] ^= 1;
    return !fail;
  }
};

MemoryFlash *app_flash = nullptr;
// 仅替换外设边界；busy/fault分别可控，验证App不能把超时当DMA停止。
namespace mocklighting {
lighting::LightFrame last_frame;
bool busy = false, faulted = false;
uint32_t frames = 0;
}
namespace fluxrail {
namespace config {
bool FlashRp2040::available() const {
  return app_flash && app_flash->available();
}
bool FlashRp2040::read(uint8_t s, uint32_t o, uint8_t *d, uint32_t n) {
  return app_flash->read(s, o, d, n);
}
bool FlashRp2040::erase(uint8_t s) { return app_flash->erase(s); }
bool FlashRp2040::program(uint8_t s, uint32_t o, const uint8_t *d, uint32_t n) {
  return app_flash->program(s, o, d, n);
}
} // namespace config
namespace keyboard {
void beginKeyPinsRp2040() {}
uint16_t readKeyDownMaskRp2040() { return mockapp::raw; }
} // namespace keyboard
namespace lighting {
bool Ws2812OutputRp2040::begin(const LedOrder &, bool) { return true; }
void Ws2812OutputRp2040::service(uint32_t) {
  active_ = mocklighting::busy;
  faulted_ = mocklighting::faulted;
}
bool Ws2812OutputRp2040::trySubmit(const LightFrame &frame, uint32_t) {
  if (active_ || faulted_)
    return false;
  mocklighting::last_frame = frame;
  ++mocklighting::frames;
  return true;
}
} // namespace lighting
} // namespace fluxrail
void TinyUSB_Device_Task() {
  if (!mockapp::complete_usb)
    return;
  for (uint8_t i = 0; i < 2; ++i) {
    auto &e = mockusb::endpoints[i];
    if (!e.ready) {
      e.ready = true;
      __usb_mutex.locked = true;
      usb::usbBackend().completed(i, e.length);
      __usb_mutex.locked = false;
    }
  }
}
void tickApp(app::App &h, uint32_t loops = 50) {
  for (uint32_t i = 0; i < loops; ++i) {
    mockapp::now += 500;
    h.serviceCore1();
    h.serviceCore0();
  }
}
extern "C" int test_app_full_snapshot_save_boot_and_preview() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::normalHid().configureBeforeConnect());
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.ready() && h.deviceConfig()->active_id == 4 &&
        h.savedRevision() == 0);
  auto c = *h.deviceConfig();
  CHECK(config::createProfile(c, 2, "saved"));
  c.active_id = 5;
  c.revision = 2;
  c.profiles[4].runtime.lighting.brightness_level = 79;
  c.profiles[4].knobs[1].push.enabled = true;
  c.profiles[4].knobs[1].push.max_hold_us = 345000;
  CHECK(h.applyDevice(c, mockapp::now));
  tickApp(h);
  CHECK(h.ready() && h.deviceConfig()->revision == 2 &&
        h.appliedConfig().lighting.brightness_level == 79 &&
        flash.operations == 0);
  CHECK(h.save(mockapp::now));
  tickApp(h);
  CHECK(h.ready() && !h.saving() && h.saveResult() == config::SaveResult::Ok &&
        h.savedRevision() == 2);
  // Module1 preview only replaces the active runtime projection.
  auto r = h.appliedConfig();
  r.revision = 3;
  r.lighting.brightness_level = 23;
  CHECK(h.apply(r, mockapp::now));
  tickApp(h);
  CHECK(h.deviceConfig()->revision == 3 &&
        h.deviceConfig()->profiles[4].knobs[1].push.max_hold_us == 345000 &&
        h.savedRevision() == 2);
  app::App boot;
  boot.beginCore0();
  boot.beginCore1();
  tickApp(boot);
  CHECK(boot.ready() && boot.deviceConfig()->revision == 2 &&
        boot.deviceConfig()->active_id == 5 &&
        boot.appliedConfig().lighting.brightness_level == 79 &&
        boot.deviceConfig()->profiles[4].knobs[1].push.enabled);
  return 0;
}
extern "C" int
test_app_save_waits_usb_completion_and_resumes_on_flash_failure() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::normalHid().configureBeforeConnect());
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.ready());
  CHECK(h.save(mockapp::now));
  // Freeze the actual last endpoint transfer during release, before ACK.
  mockapp::complete_usb=false;
  tickApp(h,10);
  CHECK(h.saving() && h.savedRevision() == 0);
  tickApp(h, 10);
  CHECK(h.savedRevision() == 0);
  mockapp::complete_usb = true;
  flash.corrupt_write = true;
  tickApp(h);
  CHECK(!h.saving() && h.ready() &&
        h.saveResult() == config::SaveResult::VerifyFailed &&
        h.savedRevision() == 0);
  flash.corrupt_write = false;
  CHECK(h.save(mockapp::now));
  tickApp(h);
  CHECK(h.saveResult() == config::SaveResult::Ok && h.savedRevision() == 1);
  return 0;
}
extern "C" int test_app_maintenance_disconnect_timeout_no_write() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::normalHid().configureBeforeConnect());
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.ready() && h.save(mockapp::now));
  mockusb::mounted = false;
  tickApp(h, 5);
  CHECK(h.saving());
  mockapp::now += 2100000;
  tickApp(h, 1);
  CHECK(!h.saving() && h.savedRevision() == 0 && h.failed());
  mockusb::mounted = true;
  tickApp(h);
  CHECK(h.ready() && h.savedRevision() == 0);
  return 0;
}

// LED超时是故障标志，不是DMA已退出；不能抢先擦写Flash。
extern "C" int test_app_save_faulted_busy_led_no_flash_and_input_recovers() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App h;
  h.beginCore0();
  h.beginCore1();
  mockapp::raw = 1u << 7;
  tickApp(h);
  CHECK(h.ready() && h.physicalMask() == (1u << 7));
  CHECK(h.save(mockapp::now));
  mocklighting::busy = mocklighting::faulted = true;
  tickApp(h, 50);
  CHECK(h.saving() && flash.operations == 0 && h.savedRevision() == 0);
  mockapp::now += 2100000;
  tickApp(h, 1);
  CHECK(!h.saving() && h.failed() && flash.operations == 0 &&
        h.saveResult() == config::SaveResult::VerifyFailed);
  tickApp(h);
  CHECK(h.ready() && h.stats().led_failed && flash.operations == 0);
  CHECK(usb::normalHid().getKeyboard(0, HID_REPORT_TYPE_INPUT,
        mockusb::endpoints[0].payload, 29) == 29);
  CHECK((mockusb::endpoints[0].payload[1] & (1u << 4)) != 0);
  // 故障保持后再申请save也不能绕过busy；只能先受控结束外设/重启。
  CHECK(h.save(mockapp::now));
  tickApp(h);
  CHECK(flash.operations == 0);
  mockapp::now += 2100000;
  tickApp(h, 1);
  CHECK(!h.saving() && flash.operations == 0);
  return 0;
}
extern "C" int test_app_indicator_uses_confirmed_template_and_actual_persona() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.ready() && mocklighting::frames > 0);
  CHECK(mocklighting::last_frame.indicator.r == 0 &&
        mocklighting::last_frame.indicator.g == 63 &&
        mocklighting::last_frame.indicator.b == 0);
  auto r = h.appliedConfig();
  r.revision = 2;
  r.indicator_color = {255, 0, 0};
  CHECK(h.apply(r, mockapp::now));
  mockapp::complete_usb = false;
  tickApp(h, 60);
  CHECK(!h.ready() && mocklighting::last_frame.indicator.g == 63 &&
        mocklighting::last_frame.indicator.r == 0);
  mockapp::complete_usb = true;
  tickApp(h, 60);
  CHECK(h.ready() && mocklighting::last_frame.indicator.r == 63 &&
        mocklighting::last_frame.indicator.g == 0);
  auto c = *h.deviceConfig();
  c.revision = 3;
  c.startup_usb_mode = 1; // 保存/请求人格都不改变当前普通指示色。
  CHECK(h.applyDevice(c, mockapp::now));
  tickApp(h, 60);
  CHECK(h.usbMode() == 0 && mocklighting::last_frame.indicator.r == 63 &&
        mocklighting::last_frame.indicator.b == 0);
  r = h.appliedConfig();
  r.revision = 4;
  r.indicator_enabled = false;
  CHECK(h.apply(r, mockapp::now));
  tickApp(h, 60);
  const auto &v = mocklighting::last_frame.indicator;
  CHECK(!v.r && !v.g && !v.b);
  return 0;
}
