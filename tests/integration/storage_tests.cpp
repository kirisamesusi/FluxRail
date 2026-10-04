#include "app/ConfigCoordinator.h"
#include "config/ConfigStore.h"
#include "protocol/ConfigProtocol.h"
#include "protocol/ProfileCodec.h"
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
static hall::Calibration fixture() {
  hall::Calibration c{};
  c.valid = true;
  c.left_mm = -41;
  c.right_mm = 39;
  c.node_count = 7;
  const float positions[7] = {
      -41, hall::kSensorMm[0],        hall::kSensorMm[0] * 0.5f,
      0,   hall::kSensorMm[2] * 0.5f, hall::kSensorMm[2],
      39};
  c.polarity[1] = -1;
  for (uint8_t i = 0; i < 3; ++i) {
    c.baseline[i] = 2048;
    c.baseline_noise[i] = 0.3f;
  }
  for (uint8_t j = 0; j < 7; ++j) {
    c.nodes[j].mm = positions[j];
    for (uint8_t i = 0; i < 3; ++i) {
      c.nodes[j].response[i] =
          100 + 900 * hall::clamp(1 - hall::magnitude(positions[j] -
                                                      hall::kSensorMm[i]) /
                                          33.75f,
                                  0, 1);
      c.nodes[j].noise[i] = 0.5f;
    }
  }
  c.noise_mm = 0.02f;
  c.sweep_left = true;
  c.sweep_right = true;
  c.sweep_segments = 63;
  return c;
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
static void put32(uint8_t *p, uint32_t n) {
  for (uint8_t i = 0; i < 4; ++i)
    p[i] = uint8_t(n >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
static void repair(uint8_t *p) {
  auto n = get32(p + 8);
  put32(p + 20, config::crc32(p + 256, n));
  put32(p + 24, config::crc32(p, 24));
  auto m = p + 7936;
  put32(m + 4, get32(p + 12));
  put32(m + 8, get32(p + 20));
  put32(m + 12, get32(p + 24));
  put32(m + 16, config::crc32(m, 16));
}
extern "C" int test_storage_schema_golden_and_17_node_calibration() {
  common::DeviceConfig c;
  config::defaults(c);
  uint8_t data[7680];
  auto n = config::DeviceCodec::encode(c, data, sizeof(data));
  CHECK(n == 1387 && config::crc32(data, n) == 1053904252u);
  CHECK(config::crc32(reinterpret_cast<const uint8_t *>("123456789"), 9) ==
        0xcbf43926u);
  const auto old = fixture();
  auto cal = old;
  cal.node_count = 17;
  uint8_t count = 0;
  const uint8_t steps[] = {2, 3, 3, 3, 3, 2};
  for (uint8_t j = 0; j < 6; ++j)
    for (uint8_t k = 0; k < steps[j]; ++k) {
      auto &node = cal.nodes[count++];
      float f = float(k) / steps[j];
      node.mm = old.nodes[j].mm + f * (old.nodes[j + 1].mm - old.nodes[j].mm);
      for (uint8_t i = 0; i < 3; ++i) {
        node.response[i] =
            old.nodes[j].response[i] +
            f * (old.nodes[j + 1].response[i] - old.nodes[j].response[i]);
        node.noise[i] = 0.5f;
      }
    }
  cal.nodes[count++] = old.nodes[6];
  cal.sweep_segments = 65535;
  CHECK(count == 17 && hall::validateCalibration(cal));
  c.calibration = cal;
  c.calibration_generation = 3;
  n = config::DeviceCodec::encode(c, data, sizeof(data));
  CHECK(n > 0);
  config::DeviceCodec decoder;
  common::DeviceConfig out;
  CHECK(decoder.decode(data, n, out) && out.calibration.node_count == 17 &&
        out.calibration_generation == 3);
  c.calibration.nodes[9].response[1] = __builtin_nanf("");
  CHECK(config::DeviceCodec::encode(c, data, sizeof(data)) == 0);
  return 0;
}
extern "C" int test_storage_profiles_full_snapshot_codec() {
  common::DeviceConfig c;
  config::defaults(c);
  CHECK(config::validDevice(c));
  CHECK(c.active_id == 4 && c.profiles[1].hall.enabled &&
        !c.profiles[1].knobs[0].enabled && !c.profiles[1].knobs[1].enabled);
  c.calibration = fixture();
  c.calibration_generation = 9;
  CHECK(hall::validateCalibration(c.calibration));
  for (uint8_t i = 0; i < 8; ++i)
    CHECK(config::createProfile(c, 1, "custom"));
  CHECK(c.profile_count == 12 && !config::createProfile(c, 1, "full"));
  c.active_id = 12;
  CHECK(config::deleteProfile(c, 12) && c.active_id == 4);
  CHECK(config::createProfile(c, 2, "again") && c.profiles[11].id == 13);
  CHECK(!config::deleteProfile(c, 1) && !config::renameProfile(c, 1, "") &&
        !config::renameProfile(c, 1, "bad\\name"));
  CHECK(config::restorePreset(c, 2) && c.calibration_generation == 9 &&
        c.calibration.valid);
  c.profiles[8].knobs[0].gain = 2.25f;
  c.profiles[7].runtime.keyboard.bindings[11] = {
      common::ActionType::MouseButton, 0, 0, 4};
  uint8_t data[7680], again[7680];
  auto n = config::DeviceCodec::encode(c, data, sizeof(data));
  CHECK(n > 0 && n < 7680);
  config::DeviceCodec codec;
  common::DeviceConfig out;
  CHECK(codec.decode(data, n, out));
  auto m = config::DeviceCodec::encode(out, again, sizeof(again));
  CHECK(n == m && memcmp(data, again, n) == 0 &&
        out.calibration.polarity[1] == -1 &&
        out.profiles[8].knobs[0].gain == 2.25f);
  CHECK(!codec.decode(data, n + 1, out));
  c.profiles[0].hall.center_gain = __builtin_nanf("");
  CHECK(!config::validDevice(c));
  return 0;
}
static int cutRange(int first, int last) {
  MemoryFlash base;
  config::ConfigStore store(base);
  common::DeviceConfig c;
  config::defaults(c);
  for (uint8_t i = 0; i < 8; ++i)
    CHECK(config::createProfile(c, 1, "custom"));
  c.calibration = fixture();
  CHECK(store.save(c) == config::SaveResult::Ok);
  c.revision = 2;
  c.profiles[3].runtime.lighting.brightness_level = 73;
  // Cut before/during each sector erase, header, every payload page, and
  // commit.
  const int partials[] = {0, 1, 128, 255};
  uint8_t encoded[7680];
  const auto encoded_size =
      config::DeviceCodec::encode(c, encoded, sizeof(encoded));
  const int stages = int((encoded_size + 256 + 255) / 256) + 3;
  for (int cut = first; cut <= stages && cut <= last; ++cut)
    for (int partial : partials) {
      MemoryFlash flash;
      memcpy(flash.bytes, base.bytes, sizeof(flash.bytes));
      config::ConfigStore writer(flash);
      common::DeviceConfig old;
      CHECK(writer.load(old) && old.revision == 1);
      flash.cutoff = cut;
      flash.partial = partial;
      const auto r = writer.save(c);
      config::ConfigStore restart(flash);
      common::DeviceConfig loaded;
      CHECK(restart.load(loaded));
      CHECK(loaded.revision == 1 || loaded.revision == 2);
      CHECK(loaded.profiles[3].runtime.lighting.brightness_level ==
            (loaded.revision == 1 ? 50 : 73));
      CHECK(memcmp(flash.bytes[0], base.bytes[0], 8192) == 0);
      if (r == config::SaveResult::Ok)
        CHECK(loaded.revision == 2);
    }
  // Initial save interruption has no valid slot and never invents saved state.
  MemoryFlash empty;
  empty.cutoff = 3;
  empty.partial = 128;
  config::ConfigStore first_store(empty);
  CHECK(first_store.save(c) != config::SaveResult::Ok);
  config::ConfigStore restart(empty);
  common::DeviceConfig loaded;
  CHECK(!restart.load(loaded) && restart.savedRevision() == 0);
  return 0;
}
extern "C" int test_storage_cut_erase_and_payload_early() {
  return cutRange(0, 11);
}
extern "C" int test_storage_cut_payload_late_and_commit() {
  return cutRange(12, 100);
}
extern "C" int test_storage_corruption_semantic_fallback_generation_wrap() {
  MemoryFlash base;
  config::ConfigStore store(base);
  common::DeviceConfig c;
  config::defaults(c);
  CHECK(store.save(c) == config::SaveResult::Ok);
  c.revision = 2;
  CHECK(store.save(c) == config::SaveResult::Ok);
  const uint32_t spots[] = {0,    4,   8,   12,   16,   20,   24,
                            28,   256, 300, 7936, 7940, 7944, 7948,
                            7952, 32,  255, 7900, 8000};
  for (auto spot : spots) {
    MemoryFlash f;
    memcpy(f.bytes, base.bytes, sizeof(f.bytes));
    f.bytes[1][spot] ^= 1;
    config::ConfigStore read(f);
    common::DeviceConfig out;
    CHECK(read.load(out) && out.revision == 1 && read.validSlots() == 1);
  }
  MemoryFlash semantic;
  memcpy(semantic.bytes, base.bytes, sizeof(semantic.bytes));
  put32(semantic.bytes[1] + 256 + 8, 999);
  repair(semantic.bytes[1]);
  config::ConfigStore sr(semantic);
  common::DeviceConfig out;
  CHECK(sr.load(out) && out.revision == 1);
  MemoryFlash wrap;
  memcpy(wrap.bytes, base.bytes, sizeof(wrap.bytes));
  put32(wrap.bytes[0] + 12, UINT32_MAX);
  repair(wrap.bytes[0]);
  put32(wrap.bytes[1] + 12, 1);
  repair(wrap.bytes[1]);
  config::ConfigStore wr(wrap);
  CHECK(wr.load(out) && out.revision == 2 && wr.generation() == 1);
  c.revision = 3;
  CHECK(wr.save(c) == config::SaveResult::Ok && wr.generation() == 2);
  config::ConfigStore rr(wrap);
  CHECK(rr.load(out) && out.revision == 3);
  memset(wrap.bytes, 0, sizeof(wrap.bytes));
  CHECK(!rr.load(out) && rr.savedRevision() == 0);
  return 0;
}
extern "C" int test_storage_readback_failure_preserves_previous() {
  MemoryFlash f;
  config::ConfigStore s(f);
  common::DeviceConfig c;
  config::defaults(c);
  CHECK(s.save(c) == config::SaveResult::Ok);
  c.revision = 2;
  f.corrupt_write = true;
  CHECK(s.save(c) == config::SaveResult::VerifyFailed &&
        s.savedRevision() == 1);
  f.corrupt_write = false;
  config::ConfigStore restart(f);
  CHECK(restart.load(c) && c.revision == 1);
  c.revision = 2;
  f.bad_read = true;
  CHECK(s.save(c) == config::SaveResult::ReadFailed && s.savedRevision() == 1);
  f.bad_read = false;
  f.enabled = false;
  CHECK(s.save(c) == config::SaveResult::Unavailable);
  return 0;
}
struct DeviceHost : Host {
  common::DeviceConfig device, candidate;
  MemoryFlash flash;
  config::ConfigStore store{flash};
  bool pending = false, save_pending = false, written = false;
  config::SaveResult result = config::SaveResult::None;
  DeviceHost() { config::defaults(device); }
  const common::DeviceConfig *deviceConfig() const override { return &device; }
  bool applyDevice(const common::DeviceConfig &c, uint32_t time) override {
    if (busy() || !config::validDevice(c) ||
        !Host::apply(config::activeRuntime(c), time))
      return false;
    candidate = c;
    pending = true;
    return true;
  }
  bool apply(const common::RuntimeConfig &r, uint32_t time) override {
    candidate = device;
    config::find(candidate, candidate.active_id)->runtime = r;
    candidate.revision = r.revision;
    return applyDevice(candidate, time);
  }
  uint32_t savedRevision() const override { return store.savedRevision(); }
  bool flashAvailable() const override { return true; }
  uint8_t validSlots() const override { return store.validSlots(); }
  bool busy() const override { return Host::busy() || save_pending; }
  bool saving() const override { return save_pending; }
  config::SaveResult saveResult() const override { return result; }
  bool save(uint32_t time) override {
    if (!coordinator.startMaintenance(requested, time))
      return false;
    save_pending = true;
    written = false;
    result = config::SaveResult::None;
    return true;
  }
  void tickDevice() {
    Host::tick();
    if (pending && ready()) {
      device = candidate;
      pending = false;
    }
    if (save_pending) {
      if (coordinator.maintenanceReady() && !written) {
        result = store.save(device);
        written = true;
        coordinator.finishMaintenance(now);
      }
      if (written && ready())
        save_pending = false;
    }
  }
};
void pump(protocol::ConfigProtocol &p, Port &port, DeviceHost &h,
          uint32_t loops = 180) {
  for (uint32_t i = 0; i < loops; ++i) {
    h.tickDevice();
    p.service(h.now, port, h);
  }
}
extern "C" int test_storage_protocol_templates_and_save_ack() {
  DeviceHost h;
  Port port;
  protocol::ConfigProtocol p;
  request(port, 1, "hello");
  pump(p, port, h);
  CHECK(contains(port.tx, "device_schema2") &&
        contains(port.tx, "\"flash_save\":true") &&
        contains(port.tx, "\"hall\":false"));
  port.clearOutput();
  request(port, 2, "copy_profile",
          "{\"expected_revision\":1,\"profile_id\":2,\"name\":\"copy\"}");
  pump(p, port, h);
  CHECK(h.device.profile_count == 5 && h.device.revision == 2 &&
        contains(port.tx, "\"ok\":true"));
  port.clearOutput();
  request(port, 3, "set_active_profile",
          "{\"expected_revision\":2,\"profile_id\":5}");
  pump(p, port, h);
  CHECK(h.device.active_id == 5 && h.appliedConfig().revision == 3);
  port.clearOutput();
  h.sink.blocked = true;
  request(port, 4, "save_config", "{\"expected_revision\":3}");
  pump(p, port, h, 20);
  CHECK(h.saving() && h.savedRevision() == 0 && port.out == 0);
  // CDC disconnect only loses reply; accepted save completes under host
  // ownership.
  port.online = false;
  p.service(h.now, port, h);
  h.sink.blocked = false;
  pump(p, port, h, 40);
  CHECK(!h.saving() && h.savedRevision() == 3 && h.ready());
  port.online = true;
  request(port, 5, "get_device");
  pump(p, port, h);
  CHECK(contains(port.tx, "\"saved_revision\":3,\"dirty\":false") &&
        !contains(port.tx, "\"id\":4"));
  common::DeviceConfig boot;
  config::ConfigStore reader(h.flash);
  CHECK(reader.load(boot) && boot.active_id == 5 && boot.profile_count == 5);
  port.clearOutput();
  request(port, 6, "delete_profile",
          "{\"expected_revision\":3,\"profile_id\":5}");
  pump(p, port, h);
  CHECK(h.device.active_id == 4 && h.device.profile_count == 4 &&
        h.device.revision == 4 && h.savedRevision() == 3);
  return 0;
}
extern "C" int test_storage_motion_codec_and_import_atomicity() {
  common::Profile pr;
  common::DeviceConfig c;
  config::defaults(c);
  pr = c.profiles[1];
  char bytes[2048];
  protocol::JsonWriter w(bytes, sizeof(bytes));
  protocol::encodeMotion(w, pr);
  protocol::Json j;
  CHECK(w.ok() && j.parse(bytes, w.size()));
  CHECK(protocol::decodeMotion(j, 0, pr) && pr.hall.enabled &&
        !pr.knobs[0].enabled);
  pr.knobs[0].gain = 0;
  protocol::JsonWriter bad(bytes, sizeof(bytes));
  protocol::encodeMotion(bad, pr);
  CHECK(j.parse(bytes, bad.size()) && !protocol::decodeMotion(j, 0, pr));
  DeviceHost h;
  Port port;
  protocol::ConfigProtocol p;
  uint8_t data[7680];
  c.calibration = fixture();
  c.active_id = 2;
  auto n = config::DeviceCodec::encode(c, data, sizeof(data));
  auto crc = config::crc32(data, n);
  char payload[1400];
  protocol::JsonWriter begin(payload, sizeof(payload));
  begin.literal("{\"expected_revision\":1,\"length\":");
  begin.number(n);
  begin.literal(",\"crc32\":");
  begin.number(crc);
  begin.literal("}");
  payload[begin.size()] = 0;
  request(port, 1, "begin_import", payload);
  pump(p, port, h);
  port.clearOutput();
  request(port, 2, "commit_import", "{\"expected_revision\":1}");
  pump(p, port, h);
  CHECK(contains(port.tx, "INVALID_TRANSFER") && h.device.revision == 1);
  port.clearOutput();
  for (uint32_t off = 0; off < n;) {
    protocol::JsonWriter chunk(payload, sizeof(payload));
    chunk.literal("{\"expected_revision\":1,\"offset\":");
    chunk.number(off);
    chunk.literal(",\"data\":\"");
    uint32_t end = off + 512 < n ? off + 512 : n;
    const char hex[] = "0123456789abcdef";
    for (; off < end; ++off) {
      char s[3] = {hex[data[off] >> 4], hex[data[off] & 15], 0};
      chunk.literal(s);
    }
    chunk.literal("\"}");
    payload[chunk.size()] = 0;
    port.n = port.pos = 0;
    request(port, 3, "import_chunk", payload);
    pump(p, port, h);
    CHECK(contains(port.tx, "\"ok\":true") && h.device.revision == 1);
    port.clearOutput();
  }
  port.n = port.pos = 0;
  request(port, 4, "commit_import", "{\"expected_revision\":1}");
  pump(p, port, h);
  CHECK(h.device.revision == 2 && h.device.active_id == 2 &&
        h.device.calibration.valid && h.device.calibration.polarity[1] == -1 &&
        h.savedRevision() == 0);
  return 0;
}
extern "C" int test_storage_maintenance_release_resume_current_mask() {
  Host h;
  h.worker.service(0x81, h.now += 500);
  h.worker.service(0x81, h.now += 2000);
  CHECK(h.coordinator.startMaintenance(h.requested, h.now));
  for (uint8_t i = 0; i < 40; ++i) {
    h.worker.service(0x81, h.now += 500);
    h.coordinator.service(h.now, h.sink);
  }
  CHECK(h.coordinator.maintenanceReady() && !h.ready());
  // Input continues physical observations while publication is paused.
  for (uint8_t i = 0; i < 8; ++i) {
    h.worker.service(0x2, h.now += 500);
    h.coordinator.service(h.now, h.sink);
  }
  CHECK(h.coordinator.finishMaintenance(h.now));
  for (uint8_t i = 0; i < 40; ++i) {
    h.worker.service(0x2, h.now += 500);
    h.coordinator.service(h.now, h.sink);
  }
  CHECK(h.ready());
  common::InputEdge e;
  uint32_t count = 0;
  while (h.bridge.edges.tryPop(e)) {
    CHECK(e.epoch == h.coordinator.epoch());
    ++count;
  }
  CHECK(count == 3);
  return 0;
}

static int truncateRange(uint32_t first, uint32_t last) {
  common::DeviceConfig c;
  config::defaults(c);
  for (uint8_t i = 0; i < 8; ++i)
    CHECK(config::createProfile(c, 1, "custom"));
  c.calibration = fixture();
  uint8_t data[7680];
  auto n = config::DeviceCodec::encode(c, data, sizeof(data));
  config::DeviceCodec codec;
  common::DeviceConfig out;
  CHECK(n > 0);
  for (uint32_t size = first; size < n && size < last; ++size) {
    out.revision = 77;
    CHECK(!codec.decode(data, size, out) && out.revision == 77);
  }
  return 0;
}
extern "C" int test_storage_truncation_0() { return truncateRange(0, 768); }
extern "C" int test_storage_truncation_1() { return truncateRange(768, 1536); }
extern "C" int test_storage_truncation_2() { return truncateRange(1536, 2304); }
extern "C" int test_storage_truncation_3() { return truncateRange(2304, 3072); }
extern "C" int test_storage_truncation_4() { return truncateRange(3072, 3840); }
extern "C" int test_storage_truncation_5() { return truncateRange(3840, 4608); }
extern "C" int test_storage_set_profile_full_payload_and_readback() {
  DeviceHost h;
  Port port;
  protocol::ConfigProtocol p;
  auto pr = h.device.profiles[1];
  for (auto &b : pr.runtime.keyboard.bindings)
    b = {common::ActionType::Keyboard, 4, 255, 0};
  pr.knobs[0].gain = 0.01;
  pr.knobs[0].push.enabled = true;
  pr.knobs[1].counts_per_turn_at_gain1 = 1000000;
  char payload[4096];
  protocol::JsonWriter w(payload, sizeof(payload));
  w.literal("{\"expected_revision\":1,\"profile_id\":2,\"config\":");
  protocol::encodeRuntime(w, pr.runtime);
  w.literal(",\"motion\":");
  protocol::encodeMotion(w, pr);
  w.literal("}");
  CHECK(w.ok() && w.size() < 4096);
  payload[w.size()] = 0;
  request(port, 1, "set_profile", payload);
  pump(p, port, h);
  CHECK(h.device.revision == 2 &&
        h.device.profiles[1].knobs[0].gain > 0.009999999999 &&
        h.device.profiles[1].knobs[0].gain < 0.010000000001 &&
        h.device.profiles[1].knobs[0].push.enabled);
  port.clearOutput();
  request(port, 2, "get_profile", "{\"profile_id\":2}");
  pump(p, port, h);
  protocol::Json j;
  CHECK(contains(port.tx, "\"ok\":true") && j.parse(port.tx, port.out));
  CHECK(contains(port.tx, "\"counts_per_turn_at_gain1\":1000000.000000"));
  return 0;
}

extern "C" int test_storage_import_revision_change_invalidates_candidate() {
  DeviceHost h;
  Port port;
  protocol::ConfigProtocol p;
  request(port, 1, "begin_import",
          "{\"expected_revision\":1,\"length\":2,\"crc32\":0}");
  pump(p, port, h);
  port.clearOutput();
  request(port, 2, "rename_profile",
          "{\"expected_revision\":1,\"profile_id\":4,\"name\":\"changed\"}");
  pump(p, port, h);
  CHECK(h.device.revision == 2);
  port.clearOutput();
  request(port, 3, "import_chunk",
          "{\"expected_revision\":2,\"offset\":0,\"data\":\"0000\"}");
  pump(p, port, h);
  CHECK(contains(port.tx, "INVALID_TRANSFER") && h.device.revision == 2);
  port.clearOutput();
  request(port, 4, "commit_import", "{\"expected_revision\":2}");
  pump(p, port, h);
  CHECK(contains(port.tx, "INVALID_TRANSFER") && h.device.revision == 2);
  return 0;
}

extern "C" int test_indicator_defaults_copy_restore_and_saved_color_preserved() {
  common::DeviceConfig c;
  config::defaults(c);
  const common::Rgb expected[] = {
      {0, 255, 255}, {255, 96, 0}, {128, 0, 255}, {0, 255, 0}};
  for (uint8_t i = 0; i < 4; ++i) {
    const auto &v = c.profiles[i].runtime.indicator_color;
    CHECK(v.r == expected[i].r && v.g == expected[i].g && v.b == expected[i].b);
  }
  c.profiles[1].runtime.indicator_color = {17, 33, 129};
  CHECK(config::createProfile(c, 2, "custom color"));
  CHECK(c.profiles[4].runtime.indicator_color.b == 129);
  MemoryFlash flash;
  config::ConfigStore store(flash);
  CHECK(store.save(c) == config::SaveResult::Ok);
  common::DeviceConfig loaded;
  CHECK(store.load(loaded));
  CHECK(loaded.profiles[1].runtime.indicator_color.r == 17 &&
        loaded.profiles[4].runtime.indicator_color.g == 33);
  CHECK(config::restorePreset(loaded, 2));
  CHECK(loaded.profiles[1].runtime.indicator_color.g == 96 &&
        loaded.profiles[4].runtime.indicator_color.g == 33);
  return 0;
}
