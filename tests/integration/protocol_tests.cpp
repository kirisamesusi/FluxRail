#include "app/ConfigCoordinator.h"
#include "common/FirmwareInfo.h"
#include "protocol/ConfigProtocol.h"
#include "realtime/InputWorker.h"
#include "usb/CdcTransport.h"
#include <Adafruit_TinyUSB.h>
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
extern "C" int test_protocol_json_and_runtime_atomic_validation() {
  protocol::Json j;
  uint32_t n = 777;
  const char *bad[] = {"",
                       "{}{}",
                       "{",
                       "[1,]",
                       "{\"x\":01}",
                       "{\"x\":truefalse}",
                       "{\"x\":-}",
                       "{\"x\":1.}",
                       "{\"x\":1e+}",
                       "{\"x\":\"\\q\"}",
                       "{\"x\":\"\\u00xx\"}",
                       "{\"x\":\"\n\"}"};
  for (auto s : bad)
    CHECK(!j.parse(s, length(s)));
  const char *max = "4294967295";
  CHECK(j.parse(max, length(max)) && j.uintValue(0, n) && n == UINT32_MAX);
  const char *big = "4294967296";
  CHECK(j.parse(big, length(big)) && !j.uintValue(0, n) && n == UINT32_MAX);
  const char *frac = "1.0";
  CHECK(j.parse(frac, 3) && !j.uintValue(0, n));
  const char *dupe = "{\"id\":1,\"id\":2}";
  CHECK(j.parse(dupe, length(dupe)) &&
        j.field(0, "id") == protocol::Json::kMissing);
  char nested[60];
  for (uint8_t i = 0; i < 14; ++i)
    nested[i] = '[';
  nested[14] = '0';
  for (uint8_t i = 0; i < 14; ++i)
    nested[15 + i] = ']';
  CHECK(!j.parse(nested, 29));
  char many[900];
  for (uint16_t i = 0; i < 400; ++i) {
    many[2 * i + 1] = '0';
    many[2 * i + 2] = ',';
  }
  many[0] = '[';
  many[800] = ']';
  CHECK(!j.parse(many, 801));
  common::RuntimeConfig c;
  c.keyboard = keyboard::defaultKeyboardConfig();
  c.keyboard.bindings[10] = {common::ActionType::MouseButton, 0, 0, 4};
  c.lighting.per_key = true;
  c.lighting.key_colors[9] = {1, 2, 3};
  char bytes[4096];
  protocol::JsonWriter w(bytes, sizeof(bytes));
  protocol::encodeRuntime(w, c);
  CHECK(w.ok());
  CHECK(j.parse(bytes, w.size()));
  common::RuntimeConfig result;
  CHECK(protocol::decodeRuntime(j, 0, result));
  CHECK(result.keyboard.bindings[10].button == 4 &&
        result.keyboard.bindings[7].usage == 4 &&
        result.lighting.key_colors[9].b == 3);
  for (uint16_t size = 0; size < w.size(); ++size)
    CHECK(!j.parse(bytes, size));
  c.lighting.brightness_level = 101;
  protocol::JsonWriter invalid(bytes, sizeof(bytes));
  protocol::encodeRuntime(invalid, c);
  CHECK(j.parse(bytes, invalid.size()));
  result.revision = 93;
  result.lighting.brightness_level = 71;
  CHECK(!protocol::decodeRuntime(j, 0, result) && result.revision == 93 &&
        result.lighting.brightness_level == 71);
  protocol::JsonWriter tiny(bytes, 2);
  tiny.number(UINT32_MAX);
  CHECK(!tiny.ok() && tiny.size() == 2);
  return 0;
}
extern "C" int test_protocol_handshake_partial_and_backpressure() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  CHECK(h.ready());
  port.append("{\"protocol_version\":1,\"id\":1,\"op\":\"hello\",\"payload\":");
  drain(p, port, h, 3);
  CHECK(port.out == 0);
  port.append("{}}\n");
  port.write_limit = 0;
  drain(p, port, h, 10);
  CHECK(port.out == 0);
  const auto first = port.pos;
  request(port, 2, "get_config");
  drain(p, port, h, 10);
  CHECK(port.pos == first);
  port.write_limit = 7;
  drain(p, port, h, 600);
  CHECK(contains(port.tx, "\"id\":1,\"ok\":true"));
  CHECK(contains(port.tx, "\"firmware_version\":\"" FLUXRAIL_FIRMWARE_VERSION "\""));
  CHECK(contains(port.tx, "\"firmware_id\":\"FLUXRAIL-FW-V1\"") &&
        contains(port.tx, "\"firmware_board\":\"fluxrail-rp2040-4mib\"") &&
        contains(port.tx, "\"firmware_metadata_format\":1"));
  CHECK(contains(port.tx, "0123456789ABCDEF") &&
        contains(port.tx, "\"flash_save\":false") &&
        contains(port.tx, "\"io4\":false"));
  CHECK(contains(port.tx, "\"id\":2,\"ok\":true") &&
        contains(port.tx, "\"scope\":\"runtime_module1\""));
  CHECK(port.max_read_tick <= 128 && port.max_write_tick <= 64 &&
        h.apply_calls == 0);
  port.clearOutput();
  request(port, 3, "get_status");
  drain(p, port, h);
  CHECK(contains(port.tx, "\"physical_mask\":129"));
  return 0;
}
extern "C" int test_protocol_errors_and_line_resynchronization() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  request(port, 1, "hello", "{}", 2);
  request(port, 2, "save_config");
  request(port, 3, "hello", "{\"x\":0}");
  port.append("{\"protocol_version\":1,\"id\":4,\"id\":5,\"op\":\"restore_"
              "defaults\",\"payload\":{\"expected_revision\":1}}\n");
  for (uint16_t i = 0; i < 4097; ++i)
    port.rx[port.n++] = ' ';
  port.append("{\"protocol_version\":1,\"id\":6,\"op\":\"restore_defaults\","
              "\"payload\":{\"expected_revision\":1}}\n");
  request(port, 7, "get_config");
  drain(p, port, h, 400);
  CHECK(contains(port.tx, "PROTOCOL_MISMATCH") &&
        contains(port.tx, "UNSUPPORTED_OPERATION") &&
        contains(port.tx, "INVALID_PAYLOAD") &&
        contains(port.tx, "INVALID_REQUEST") &&
        contains(port.tx, "LINE_TOO_LONG"));
  CHECK(contains(port.tx, "\"id\":7,\"ok\":true") && h.apply_calls == 0 &&
        p.stats().overlong_lines == 1);
  port.clearOutput();
  // Exactly 4096 bytes before LF is valid, including trailing whitespace.
  request(port, 8, "hello");
  --port.n;
  const auto begin = port.pos;
  while (port.n - begin < 4096)
    port.rx[port.n++] = ' ';
  port.rx[port.n++] = '\n';
  drain(p, port, h, 150);
  CHECK(contains(port.tx, "\"id\":8,\"ok\":true") &&
        p.stats().overlong_lines == 1);
  return 0;
}
extern "C" int test_protocol_dual_core_apply_and_revision_conflict() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  auto c = h.appliedConfig();
  c.keyboard.bindings[7] = {common::ActionType::Keyboard, 0x2c, 2, 0};
  c.indicator_color = {1, 2, 3};
  h.sink.blocked = true;
  setRequest(port, c, 11, 1);
  drain(p, port, h, 40);
  CHECK(h.apply_calls == 1 && h.busy() && !h.ready() && port.out == 0);
  CHECK(h.appliedConfig().revision == 1 &&
        h.appliedConfig().indicator_color.r == 255);
  request(port, 12, "get_config");
  const auto pending_position = port.pos;
  drain(p, port, h, 20);
  CHECK(port.pos == pending_position && port.out == 0);
  h.sink.blocked = false;
  drain(p, port, h, 150);
  CHECK(h.ready() && h.appliedConfig().revision == 2 &&
        h.appliedConfig().keyboard.bindings[7].usage == 0x2c);
  CHECK(contains(port.tx, "\"id\":11,\"ok\":true,\"revision\":2") &&
        contains(port.tx, "\"id\":12,\"ok\":true"));
  CHECK(contains(port.tx, "\"saved_revision\":null,\"dirty\":true"));
  port.clearOutput();
  setRequest(port, c, 13, 1);
  drain(p, port, h);
  CHECK(contains(port.tx, "REVISION_CONFLICT") && h.apply_calls == 1);
  port.clearOutput();
  c.keyboard.retrigger_gap_us = 100001;
  setRequest(port, c, 14, 2);
  drain(p, port, h);
  CHECK(contains(port.tx, "INVALID_CONFIG") && h.apply_calls == 1);
  port.clearOutput();
  request(port, 15, "restore_defaults", "{\"expected_revision\":2}");
  drain(p, port, h);
  CHECK(h.apply_calls == 2 && h.appliedConfig().revision == 3 &&
        h.appliedConfig().keyboard.bindings[7].usage == 4);
  CHECK(contains(port.tx, "\"id\":15,\"ok\":true"));
  return 0;
}
extern "C" int test_protocol_disconnect_busy_timeout_wrap() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  port.append("{\"protocol_version\":1,");
  service(p, port, h);
  port.online = false;
  service(p, port, h);
  port.online = true;
  request(port, 1, "get_config");
  drain(p, port, h);
  CHECK(!contains(port.tx, "INVALID_JSON") &&
        contains(port.tx, "\"id\":1,\"ok\":true"));
  port.clearOutput();
  port.observed = false;
  request(port, 2, "hello");
  const auto pos = port.pos;
  drain(p, port, h, 3);
  CHECK(port.pos == pos);
  port.observed = true;
  drain(p, port, h);
  port.clearOutput();
  h.sink.blocked = true;
  auto c = h.appliedConfig();
  setRequest(port, c, 3, 1);
  drain(p, port, h, 30);
  CHECK(h.busy() && port.out == 0);
  port.online = false;
  service(p, port, h);
  h.sink.blocked = false;
  for (uint8_t i = 0; i < 30; ++i)
    h.tick();
  port.online = true;
  request(port, 4, "get_config");
  drain(p, port, h);
  CHECK(h.appliedConfig().revision == 2 && !contains(port.tx, "\"id\":3") &&
        contains(port.tx, "\"id\":4,\"ok\":true,\"revision\":2"));
  port.clearOutput();
  h.accept = false;
  setRequest(port, c, 5, 2);
  drain(p, port, h);
  CHECK(contains(port.tx, "BUSY"));
  h.accept = true;
  port.clearOutput();
  h.now = UINT32_MAX - 10000;
  h.sink.blocked = true;
  setRequest(port, c, 6, 2);
  drain(p, port, h, 30);
  CHECK(port.out == 0);
  h.now += 2000001;
  service(p, port, h);
  drain(p, port, h, 30);
  CHECK(contains(port.tx, "APPLY_FAILED"));
  return 0;
}

extern "C" int test_protocol_timeout_and_revision_wrap() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  auto c = h.appliedConfig();
  c.revision = UINT32_MAX;
  CHECK(h.coordinator.start(c, h.now));
  h.requested = c;
  for (uint8_t i = 0; i < 30; ++i)
    h.tick();
  CHECK(h.appliedConfig().revision == UINT32_MAX);
  setRequest(port, c, UINT32_MAX, UINT32_MAX);
  drain(p, port, h);
  CHECK(h.appliedConfig().revision == 1);
  CHECK(contains(port.tx, "\"id\":4294967295,\"ok\":true,\"revision\":1"));
  port.clearOutput();
  h.now = UINT32_MAX - 5000;
  setRequest(port, c, 1, 1);
  for (uint8_t i = 0; i < 30; ++i)
    service(p, port, h, false);
  CHECK(h.apply_calls == 2 && h.busy() && port.out == 0);
  h.now += protocol::ConfigProtocol::kApplyTimeoutUs;
  service(p, port, h, false);
  for (uint8_t i = 0; i < 30; ++i)
    service(p, port, h, false);
  CHECK(contains(port.tx, "APPLY_TIMEOUT") && p.stats().apply_timeouts == 1 &&
        h.busy());
  return 0;
}
extern "C" int test_cdc_transport_guard_dtr_and_partial_write() {
  usb::CdcTransport port;
  bool online = false;
  CHECK(port.connectionState(online) && online);
  mockcdc::rx[0] = 'A';
  mockcdc::rx_n = 1;
  CHECK(port.readByte() == 'A' && port.readByte() == -1);
  mockcdc::capacity = 5;
  mockcdc::copy_limit = 2;
  CHECK(port.write("hello", 5) == 2 && mockcdc::tx_n == 2 &&
        mockcdc::tx[0] == 'h' && mockcdc::tx[1] == 'e' &&
        mockcdc::flushes == 1);
  mockcdc::capacity = 0;
  CHECK(port.write("x", 1) == 0 && mockcdc::flushes == 1);
  __usb_mutex.locked = true;
  CHECK(!port.connectionState(online) && online);
  CHECK(port.readByte() == -1 && port.write("x", 1) == 0);
  __usb_mutex.locked = false;
  mockcdc::rx_n = 2;
  mockcdc::connected = false;
  CHECK(port.connectionState(online) && !online);
  CHECK(mockcdc::rx_pos == 2 && mockcdc::tx_n == 0 && port.readByte() == -1 &&
        port.write("x", 1) == 0);
  mockcdc::connected = true;
  mockusb::suspended = true;
  CHECK(port.connectionState(online) && !online);
  mockusb::suspended = false;
  mockusb::mounted = false;
  CHECK(port.connectionState(online) && !online);
  CHECK(mockusb::unlocked_access == 0 && !__usb_mutex.locked);
  return 0;
}

extern "C" int test_runtime_rejects_invalid_actions_and_lighting() {
  protocol::Json j;
  char bytes[4096];
  common::RuntimeConfig c;
  c.keyboard = keyboard::defaultKeyboardConfig();
  common::RuntimeConfig out;
  out.revision = 37;
  out.keyboard.retrigger_gap_us = 99;
  for (uint8_t variant = 0; variant < 9; ++variant) {
    auto bad = c;
    if (variant == 0)
      bad.keyboard.bindings[0] = {common::ActionType::Keyboard, 0, 0, 0};
    if (variant == 1)
      bad.keyboard.bindings[0] = {common::ActionType::MouseButton, 0, 0, 3};
    if (variant == 2)
      bad.keyboard.retrigger_gap_us = 100001;
    if (variant == 3)
      bad.scan.press_debounce_us = 20001;
    if (variant == 4)
      bad.scan.release_debounce_us = 20001;
    if (variant == 5)
      bad.lighting.brightness_level = 101;
    if (variant == 6)
      bad.lighting.period_ms = 99;
    if (variant == 7)
      bad.lighting.period_ms = 60001;
    if (variant == 8)
      bad.keyboard.bindings[11] = {common::ActionType::Keyboard, 0xdf, 0, 0};
    protocol::JsonWriter w(bytes, sizeof(bytes));
    protocol::encodeRuntime(w, bad);
    CHECK(w.ok() && j.parse(bytes, w.size()));
    CHECK(!protocol::decodeRuntime(j, 0, out));
    CHECK(out.revision == 37 && out.keyboard.retrigger_gap_us == 99);
  }
  // Duplicate source identifiers are rejected even if every action is valid.
  protocol::JsonWriter w(bytes, sizeof(bytes));
  protocol::encodeRuntime(w, c);
  const auto size = w.size();
  uint16_t position = 0;
  // Use known canonical field location, without requiring a null-terminated
  // writer.
  const char *source = "\"source_id\":0";
  for (uint16_t x = 0; static_cast<size_t>(x) + 13 < size; ++x) {
    uint16_t z = 0;
    while (source[z] && bytes[x + z] == source[z])
      ++z;
    if (!source[z]) {
      position = x + 12;
      break;
    }
  }
  CHECK(position > 0);
  bytes[position] = '1';
  CHECK(j.parse(bytes, size) && !protocol::decodeRuntime(j, 0, out));
  return 0;
}
extern "C" int test_protocol_envelope_rejects_noncanonical_fields() {
  Host h;
  Port port;
  protocol::ConfigProtocol p;
  const char *cases[] = {
      "{\"protocol_version\":1,\"id\":0,\"op\":\"hello\",\"payload\":{}}\n",
      "{\"protocol_version\":1,\"id\":4294967296,\"op\":\"hello\",\"payload\":{"
      "}}\n",
      "{\"protocol_version\":1,\"id\":1.0,\"op\":\"hello\",\"payload\":{}}\n",
      "{\"protocol_version\":1,\"id\":1,\"op\":0,\"payload\":{}}\n",
      "{\"protocol_version\":1,\"id\":2,\"op\":\"hello\",\"payload\":{},"
      "\"extra\":1}\n",
      "{\"protocol_version\":1,\"id\":3,\"op\":\"restore_defaults\","
      "\"payload\":{\"expected_revision\":1,\"expected_revision\":1}}\n",
      "{\"protocol_version\":1,\"id\":4,\"op\":\"restore_defaults\","
      "\"payload\":{\"expected_revision\":-1}}\n",
      "{\"protocol_version\":1,\"id\":5,\"op\":\"set_config\",\"payload\":{"
      "\"expected_revision\":1,\"config\":{}}}\n"};
  for (auto s : cases) {
    port.clearOutput();
    port.append(s);
    drain(p, port, h);
    CHECK(contains(port.tx, "\"ok\":false"));
    CHECK(h.apply_calls == 0 && h.appliedConfig().revision == 1);
  }
  return 0;
}
