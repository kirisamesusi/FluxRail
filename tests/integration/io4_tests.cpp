// Runs actual codec/backend/App/Flash/protocol C++; only peripheral boundaries
// mocked.
#include "app_storage_tests.cpp"
#include <new>
extern "C" bool fluxrail_usb_wants_cdc();
extern "C" void fluxrail_usb_configure_before_connect();
extern "C" int test_io4_wire_golden_and_descriptor() {
  usb::Io4Input s;
  for (uint8_t i = 0; i < 8; ++i)
    s.adc[i] = uint16_t(0x1200 + i);
  for (uint8_t i = 0; i < 4; ++i)
    s.spinner[i] = uint16_t(0x3400 + i);
  s.coin[0] = 0x4567;
  s.coin[1] = 0x89ab;
  s.switches[0] = 0xcdef;
  s.switches[1] = 0x1357;
  s.system = 0x30;
  s.usb = 0x81;
  uint8_t bytes[63];
  usb::encodeIo4(s, bytes);
  const uint8_t golden[] = {
      0,    18,   1,    18,   2,    18,   3,    18,   4,    18,  5, 18,
      6,    18,   7,    18,   0,    52,   1,    52,   2,    52,  3, 52,
      0x67, 0x45, 0xab, 0x89, 0xef, 0xcd, 0x57, 0x13, 0x30, 0x81};
  for (uint8_t i = 0; i < 34; ++i)
    CHECK(bytes[i] == golden[i]);
  for (uint8_t i = 34; i < 63; ++i)
    CHECK(bytes[i] == 0);
  uint32_t size = 0, count = 0, id = 0, inbits = 0, outbits = 0;
  for (uint16_t p = 0; p < usb::kIo4DescriptorBytes;) {
    uint8_t h = usb::kIo4Descriptor[p++], n = h & 3;
    if (n == 3)
      n = 4;
    CHECK(p + n <= usb::kIo4DescriptorBytes);
    uint32_t v = 0;
    for (uint8_t i = 0; i < n; ++i)
      v |= uint32_t(usb::kIo4Descriptor[p++]) << (8 * i);
    if (h == 0x75)
      size = v;
    else if (h == 0x95)
      count = v;
    else if (h == 0x85)
      id = v;
    else if ((h & 0xfc) == 0x80) {
      CHECK(id == 1);
      inbits += size * count;
    } else if ((h & 0xfc) == 0x90) {
      CHECK(id == 0x10);
      outbits += size * count;
    }
  }
  CHECK(inbits == 504 && outbits == 504 && length(usb::kIo4BoardName) == 93);
  return 0;
}
extern "C" int test_io4_out_all_lengths_ids_types_instances() {
  uint8_t bytes[128]{};
  bytes[0] = 0x10;
  bytes[1] = 3;
  usb::Io4Output o;
  for (uint16_t n = 0; n <= 128; ++n) {
    CHECK(usb::normalizeIo4Output(0, 0, 2, bytes, n, o) == (n == 64));
    CHECK(usb::normalizeIo4Output(0, 0x10, 2, bytes + 1, n, o) == (n == 63));
  }
  for (uint16_t i = 0; i < 256; ++i) {
    CHECK(usb::normalizeIo4Output(uint8_t(i), 0, 2, bytes, 64, o) == (i == 0));
    CHECK(usb::normalizeIo4Output(0, uint8_t(i), 2, bytes, 64, o) == (i == 0));
    CHECK(usb::normalizeIo4Output(0, 0, uint8_t(i), bytes, 64, o) == (i == 2));
  }
  CHECK(!usb::normalizeIo4Output(0, 0, 2, nullptr, 64, o));
  usb::Io4Output control, interrupt;
  CHECK(usb::normalizeIo4Output(0, 0x10, 2, bytes + 1, 63, control));
  CHECK(usb::normalizeIo4Output(0, 0, 2, bytes, 64, interrupt));
  CHECK(memcmp(&control, &interrupt, 63) == 0 && control.command == 3);
  bytes[0] = 0x11;
  CHECK(!usb::normalizeIo4Output(0, 0, 2, bytes, 64, o));
  return 0;
}
extern "C" int test_io4_init_timeout_clear_and_unsupported() {
  usb::Io4State s;
  usb::Io4Output o;
  CHECK(s.input.system == 2);
  o.command = 1;
  o.payload[0] = 2;
  CHECK(s.accept(o, UINT32_MAX - 50000) && s.input.system == 0x30 &&
        s.timeout_units == 2);
  s.tick(149998);
  CHECK(s.input.system == 0x30);
  s.tick(149999);
  CHECK(s.input.system == 2 && s.timeouts == 1 && !s.gpio_active);
  o.command = 2;
  o.payload[0] = 4;
  CHECK(s.accept(o, 200000) && s.sampling_count == 4 && s.input.system == 0x30);
  s.input.coin[0] = 300;
  o.command = 3;
  CHECK(s.accept(o, 200001) && s.input.system == 0 && s.input.coin[0] == 0);
  o.command = 4;
  o.payload[0] = 0x80;
  o.payload[1] = 1;
  o.payload[2] = 2;
  o.payload[3] = 4;
  CHECK(s.accept(o, 200002) && s.gpio == 0x80010204u && s.gpio_active);
  const uint8_t commands[] = {5, 0x85, 0x41};
  for (uint8_t command : commands) {
    o.command = command;
    CHECK(!s.accept(o, 200003) && s.last_output.command == 4);
  }
  CHECK(s.rejected == 3);
  s.disconnect();
  CHECK(s.input.system == 2 && s.input.adc[0] == 0x7fff &&
        s.input.switches[0] == 0x4000 && !s.gpio_active);
  return 0;
}
extern "C" int test_io4_twelve_mapping_polarity_and_absolute() {
  common::Io4Function b[12];
  common::defaultIo4Bindings(b);
  uint16_t sw[2];
  usb::mapIo4Switches(0, b, sw);
  CHECK(sw[0] == 0x4000 && sw[1] == 0x8000);
  usb::mapIo4Switches(1u << 8, b, sw);
  CHECK(sw[1] == 0);
  usb::mapIo4Switches(1u << 9, b, sw);
  CHECK(sw[0] == 0);
  usb::mapIo4Switches(0xfff, b, sw);
  CHECK(sw[0] == 0xa033 && sw[1] == 0x4001);
  for (uint8_t i = 0; i < 12; ++i)
    b[i] = common::Io4Function::Test;
  usb::mapIo4Switches(0xfff, b, sw);
  CHECK(sw[0] == 0x4200);
  usb::mapIo4Switches(1u << 11, b, sw);
  CHECK(sw[0] == 0x4200);
  usb::mapIo4Switches(0, b, sw);
  CHECK(sw[0] == 0x4000);
  uint16_t adc = 7;
  CHECK(usb::io4Absolute(0, adc) && adc == 0);
  CHECK(usb::io4Absolute(0.5f, adc) && adc == 32768);
  CHECK(usb::io4Absolute(1, adc) && adc == 65535);
  CHECK(!usb::io4Absolute(__builtin_nanf(""), adc) && adc == 65535);
  CHECK(!usb::io4Absolute(-0.01f, adc) && !usb::io4Absolute(1.01f, adc));
  for (uint32_t i = 0; i <= 65535; ++i) {
    CHECK(usb::io4Absolute(float(i) / 65535, adc) && adc == i);
  }
  return 0;
}
static void completeIo4() {
  auto &e = mockusb::endpoints[0];
  e.ready = true;
  __usb_mutex.locked = true;
  usb::usbBackend().completed(0, e.length);
  __usb_mutex.locked = false;
}
extern "C" int test_io4_backend_busy_control_get_motion_and_lights() {
  auto &h = usb::usbBackend();
  CHECK(h.configureBeforeConnect(1) && mockusb::instances == 1 && !h.hasCdc());
  CHECK(!h.configureBeforeConnect(0) && h.mode() == 1);
  common::DeviceConfig d;
  config::defaults(d);
  CHECK(h.setProfile(d.profiles[1], 1));
  CHECK(h.tryPhysicalState(0x300) && mockusb::endpoints[0].length == 64 &&
        mockusb::endpoints[0].payload[0] == 1);
  common::InputEdge e{1, 0, 1, 1, 7, true};
  CHECK(!h.edge(e));
  completeIo4();
  CHECK(h.edge(e));
  CHECK(mockusb::endpoints[0].payload[29] == 1 &&
        mockusb::endpoints[0].payload[32] == 0);
  completeIo4();
  CHECK(h.absolute(1, true, 1, 1, 1, 1));
  CHECK(!h.absolute(0.1f, false, 1, 1, 1, 1));
  CHECK(!h.absolute(0.1f, true, 2, 1, 1, 1));
  CHECK(h.tryPhysicalState(0));
  completeIo4();
  CHECK(mockusb::endpoints[0].payload[1] == 255 &&
        mockusb::endpoints[0].payload[2] == 255);
  uint8_t b[80];
  for (auto &v : b)
    v = 0xee;
  for (uint16_t n = 0; n <= 80; ++n) {
    CHECK(h.get(1, HID_REPORT_TYPE_INPUT, b, n) == (n < 63 ? n : 63));
  }
  CHECK(h.get(2, HID_REPORT_TYPE_INPUT, b, 80) == 0 &&
        h.get(1, HID_REPORT_TYPE_FEATURE, b, 80) == 0);
  uint8_t out[63]{};
  out[0] = 4;
  out[1] = 0x40; // channel1 => blue for LeftA
  __usb_mutex.locked = true;
  h.output(0x10, HID_REPORT_TYPE_OUTPUT, out, 63);
  __usb_mutex.locked = false;
  lighting::LightFrame frame;
  auto light_config = d.profiles[1].runtime.lighting;
  light_config.brightness_level = 100;
  h.applyHostLights(frame, light_config);
  CHECK(frame.keys[7].b == 127 && frame.keys[7].r == 0 && frame.keys[7].g == 0);
  uint32_t words[10], indicator;
  lighting::LedOrder order;
  lighting::packLightWords(frame, order, true, words, indicator);
  CHECK((words[1] & 0xff00) == 0x7f00);
  __usb_mutex.locked = true;
  CHECK(h.get(0x10, HID_REPORT_TYPE_OUTPUT, b, 80) == 63 && b[0] == 4);
  h.output(0x10, HID_REPORT_TYPE_OUTPUT, out, 62);
  __usb_mutex.locked = false;
  h.disconnect();
  h.applyHostLights(frame, light_config);
  CHECK(h.tryPhysicalState(0));
  completeIo4();
  e.sequence = 90;
  e.revision = 1;
  e.source_id = 11;
  CHECK(h.edge(e));
  completeIo4();
  e.sequence = 92;
  CHECK(!h.edge(e) && h.transportFaulted());
  h.clearTransportFault();
  __usb_mutex.locked = true;
  h.failed(0);
  __usb_mutex.locked = false;
  CHECK(h.transportFaulted());
  h.clearTransportFault();
  usb::Io4Stats stats;
  uint32_t rejected, timeouts;
  CHECK(h.readStats(stats, rejected, timeouts) && stats.bad_output == 1 &&
        stats.failed == 1 && stats.disconnects == 1 &&
        stats.invalid_motion == 2);
  CHECK(mockusb::unlocked_access == 0);
  return 0;
}
extern "C" int test_io4_app_one_endpoint_maintenance_and_disconnect() {
  MemoryFlash flash;
  app_flash = &flash;
  common::DeviceConfig c;
  config::defaults(c);
  c.startup_usb_mode = 1;
  c.active_id = 2;
  config::ConfigStore s(flash);
  CHECK(s.save(c) == config::SaveResult::Ok);
  app::App h;
  CHECK(h.prepareUsbBoot(false) == 1);
  CHECK(usb::usbBackend().configureBeforeConnect(1));
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.ready() && h.usbMode() == 1 && h.savedUsbMode() == 1 &&
        mockusb::instances == 1);
  CHECK(h.save(mockapp::now));
  mockapp::complete_usb = false;
  tickApp(h, 10);
  CHECK(h.saving() && !h.ready());
  mockapp::complete_usb = true;
  tickApp(h);
  CHECK(h.ready() && !h.saving() && h.saveResult() == config::SaveResult::Ok);
  mockusb::mounted = false;
  mockapp::raw = 0x180;
  tickApp(h, 10);
  mockusb::mounted = true;
  tickApp(h);
  CHECK(h.ready() && mockusb::endpoints[0].length == 64);
  CHECK(mockusb::endpoints[0].payload[29] == 1 &&
        mockusb::endpoints[0].payload[32] == 0);
  return 0;
}
extern "C" int test_io4_boot_saved_snapshot_and_physical_recovery() {
  MemoryFlash flash;
  app_flash = &flash;
  common::DeviceConfig c;
  config::defaults(c);
  c.startup_usb_mode = 1;
  c.active_id = 2;
  c.revision = 11;
  config::ConfigStore s(flash);
  CHECK(s.save(c) == config::SaveResult::Ok);
  ::new (static_cast<void *>(&app::application())) app::App();
  mockapp::raw = 0;
  CHECK(!fluxrail_usb_wants_cdc());
  fluxrail_usb_configure_before_connect();
  CHECK(usb::usbBackend().mode() == 1 && mockusb::instances == 1 &&
        length(TinyUSBDevice.product) == 92);
  // setup reuses the exact previously validated snapshot even if backing
  // bytes subsequently become unreadable; no second selection race.
  flash.bad_read = true;
  app::application().beginCore0();
  CHECK(app::application().deviceConfig()->revision == 11 &&
        app::application().savedRevision() == 11);
  return 0;
}
extern "C" int test_io4_boot_forced_normal_keeps_saved_identity() {
  MemoryFlash flash;
  app_flash = &flash;
  common::DeviceConfig c;
  config::defaults(c);
  c.startup_usb_mode = 1;
  c.active_id = 2;
  c.revision = 11;
  config::ConfigStore s(flash);
  CHECK(s.save(c) == config::SaveResult::Ok);
  ::new (static_cast<void *>(&app::application())) app::App();
  mockapp::raw = 1u << 9;
  CHECK(fluxrail_usb_wants_cdc());
  fluxrail_usb_configure_before_connect();
  auto &h = app::application();
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(h.usbMode() == 0 && h.forcedRecovery() && h.savedUsbMode() == 1 &&
        h.deviceConfig()->startup_usb_mode == 1 &&
        h.deviceConfig()->active_id == 2);
  c = *h.deviceConfig();
  c.startup_usb_mode = 0;
  c.revision = 12;
  CHECK(h.applyDevice(c, mockapp::now));
  tickApp(h);
  CHECK(!h.reboot(0, mockapp::now));
  CHECK(h.save(mockapp::now));
  tickApp(h);
  CHECK(h.reboot(0, mockapp::now));
  tickApp(h, 30);
  CHECK(h.rebootReady());
  h.rebootResponseComplete(true, mockapp::now);
  CHECK(TinyUSBDevice.detached);
  mockapp::now += 250000;
  tickApp(h, 1);
  CHECK(rp2040.reboots == 1);
  return 0;
}
struct BusPort : Port {
  bool drained = false;
  bool txComplete() override { return drained; }
};
static void pumpApp(protocol::ConfigProtocol &p, BusPort &port, app::App &h,
                    uint32_t n = 100) {
  for (uint32_t i = 0; i < n; ++i) {
    tickApp(h, 1);
    p.service(mockapp::now, port, h);
  }
}
extern "C" int test_io4_cdc_mode_save_then_response_bus_completion_reboot() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  protocol::ConfigProtocol p;
  BusPort port;
  request(port, 1, "set_startup_usb_mode",
          "{\"expected_revision\":1,\"mode\":\"IO4\"}");
  pumpApp(p, port, h);
  CHECK(h.deviceConfig()->startup_usb_mode == 1 && h.usbMode() == 0 &&
        flash.operations == 0 && contains(port.tx, "\"ok\":true"));
  port.clearOutput();
  request(port, 2, "reboot_to_saved_mode",
          "{\"expected_revision\":2,\"target\":\"IO4\"}");
  pumpApp(p, port, h);
  CHECK(contains(port.tx, "MODE_NOT_SAVED") && !TinyUSBDevice.detached);
  port.clearOutput();
  request(port, 3, "save_config", "{\"expected_revision\":2}");
  pumpApp(p, port, h);
  CHECK(h.savedUsbMode() == 1 && h.savedRevision() == 2 &&
        contains(port.tx, "\"ok\":true"));
  port.clearOutput();
  request(port, 4, "reboot_to_saved_mode",
          "{\"expected_revision\":2,\"target\":\"IO4\"}");
  pumpApp(p, port, h);
  CHECK(h.rebootReady() && contains(port.tx, "\"reboot_ready\":true") &&
        !TinyUSBDevice.detached && rp2040.reboots == 0);
  port.drained = true;
  pumpApp(p, port, h, 1);
  CHECK(TinyUSBDevice.detached && rp2040.reboots == 0);
  tickApp(h, 498);
  CHECK(rp2040.reboots == 0);
  tickApp(h, 2);
  CHECK(rp2040.reboots >= 1);
  return 0;
}
extern "C" int test_io4_reboot_failed_save_disconnect_and_response_timeout() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  CHECK(!h.reboot(0, mockapp::now));
  CHECK(h.save(mockapp::now));
  tickApp(h);
  CHECK(h.reboot(0, mockapp::now));
  tickApp(h, 30);
  CHECK(h.rebootReady());
  h.rebootResponseComplete(false, mockapp::now);
  tickApp(h);
  CHECK(h.ready() && h.rebootFailed() && !TinyUSBDevice.detached);
  CHECK(h.reboot(0, mockapp::now));
  mockusb::mounted = false;
  tickApp(h, 1);
  CHECK(h.rebootFailed() && !TinyUSBDevice.detached);
  mockusb::mounted = true;
  tickApp(h);
  CHECK(h.ready());
  CHECK(h.reboot(0, mockapp::now));
  tickApp(h, 30);
  mockapp::now += 3100000;
  tickApp(h, 1);
  tickApp(h);
  CHECK(h.ready() && h.rebootFailed() && !TinyUSBDevice.detached &&
        rp2040.reboots == 0);
  CHECK(h.save(mockapp::now));
  flash.corrupt_write = true;
  tickApp(h);
  CHECK(h.saveResult() != config::SaveResult::Ok && !h.reboot(0, mockapp::now));
  return 0;
}
extern "C" int test_io4_cdc_tx_complete_is_distinct_from_copy() {
  usb::CdcTransport t;
  mockcdc::bus_idle = false;
  CHECK(t.write("abc", 3) == 3 && !t.txComplete());
  mockcdc::bus_idle = true;
  CHECK(t.txComplete());
  mockcdc::connected = false;
  CHECK(!t.txComplete());
  return 0;
}

hall::Calibration fixture() {
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

static void put32Io4(uint8_t *p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i)
    p[i] = uint8_t(v >> (8 * i));
}
extern "C" int test_io4_schema1_full_migration_and_mixed_slots() {
  common::DeviceConfig c;
  config::defaults(c);
  // 旧已存白色/自定义色必须原样迁移，不能被新的预置色覆盖。
  c.profiles[0].runtime.indicator_color = {255, 255, 255};
  c.profiles[1].runtime.indicator_color = {17, 33, 129};
  c.calibration = fixture();
  c.calibration_generation = 17;
  c.revision = 39;
  for (uint8_t i = 0; i < 8; ++i)
    CHECK(config::createProfile(c, 2, "custom"));
  c.active_id = 12;
  c.next_id = 29;
  c.profiles[11].knobs[1].gain = 3.75;
  c.profiles[5].runtime.keyboard.bindings[11] = {
      common::ActionType::MouseButton, 0, 0, 4};
  uint8_t modern[7680], old[7680];
  auto n = config::DeviceCodec::encode(c, modern, sizeof(modern));
  CHECK(n > 0);
  memcpy(old, modern, 18);
  put32Io4(old, 1);
  uint32_t out = 18;
  for (uint8_t i = 0; i < 12; ++i) {
    memcpy(old + out, modern + 18 + i * 341, 329);
    out += 329;
  }
  memcpy(old + out, modern + 18 + 12 * 341, n - (18 + 12 * 341));
  auto oldn = n - 144;
  common::DeviceConfig migrated;
  config::DeviceCodec codec;
  CHECK(codec.decode(old, oldn, migrated));
  CHECK(migrated.schema == 2 && migrated.revision == 39 &&
        migrated.active_id == 12 && migrated.next_id == 29 &&
        migrated.calibration_generation == 17 && migrated.calibration.valid &&
        migrated.calibration.polarity[1] == -1 &&
        migrated.profiles[11].knobs[1].gain == 3.75);
  uint8_t again[7680];
  CHECK(config::DeviceCodec::encode(migrated, again, sizeof(again)) == n &&
        memcmp(again, modern, n) == 0);
  put32Io4(old, 3);
  CHECK(!codec.decode(old, oldn, migrated));
  put32Io4(old, 1);
  old[17] = 1;
  CHECK(!codec.decode(old, oldn, migrated));
  old[17] = 0;
  MemoryFlash flash;
  app_flash = &flash;
  auto a = flash.bytes[0];
  memset(a, 255, 8192);
  put32Io4(a, 0x314c5246);
  put32Io4(a + 4, 1);
  put32Io4(a + 8, oldn);
  put32Io4(a + 12, 5);
  put32Io4(a + 16, c.revision);
  memcpy(a + 256, old, oldn);
  put32Io4(a + 20, config::crc32(old, oldn));
  put32Io4(a + 24, config::crc32(a, 24));
  put32Io4(a + 28, 0);
  auto commit = a + 7936;
  put32Io4(commit, 0x54494d43);
  put32Io4(commit + 4, 5);
  put32Io4(commit + 8, config::crc32(old, oldn));
  put32Io4(commit + 12, config::crc32(a, 24));
  put32Io4(commit + 16, config::crc32(commit, 16));
  config::ConfigStore store(flash);
  CHECK(store.load(migrated) && store.savedMode() == 0 &&
        flash.operations == 0);
  c.revision = 40;
  c.startup_usb_mode = 1;
  CHECK(store.save(c) == config::SaveResult::Ok && store.savedMode() == 1);
  config::ConfigStore boot(flash);
  CHECK(boot.load(migrated) && migrated.revision == 40 &&
        migrated.startup_usb_mode == 1);
  flash.bytes[1][400] ^= 1;
  CHECK(boot.load(migrated) && migrated.revision == 39 &&
        migrated.startup_usb_mode == 0 &&
        migrated.calibration_generation == 17);
  return 0;
}
extern "C" int test_io4_mode_save_interrupted_preserves_previous_identity() {
  MemoryFlash base;
  common::DeviceConfig c;
  config::defaults(c);
  config::ConfigStore s(base);
  CHECK(s.save(c) == config::SaveResult::Ok);
  c.revision = 2;
  c.startup_usb_mode = 1;
  for (int cut = 0; cut < 10; ++cut) {
    MemoryFlash f;
    memcpy(f.bytes, base.bytes, sizeof(f.bytes));
    config::ConfigStore w(f);
    common::DeviceConfig prev;
    CHECK(w.load(prev));
    f.cutoff = cut;
    f.partial = 128;
    auto r = w.save(c);
    config::ConfigStore boot(f);
    common::DeviceConfig loaded;
    CHECK(boot.load(loaded));
    CHECK((loaded.revision == 1 && loaded.startup_usb_mode == 0) ||
          (loaded.revision == 2 && loaded.startup_usb_mode == 1));
    if (r != config::SaveResult::Ok)
      CHECK(w.savedMode() == 0);
  }
  return 0;
}
extern "C" int test_io4_protocol_function_binding_bounds_and_mode_readback() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App h;
  h.beginCore0();
  h.beginCore1();
  tickApp(h);
  protocol::ConfigProtocol p;
  BusPort port;
  request(port, 1, "set_io4_bindings",
          "{\"expected_revision\":1,\"profile_id\":4,\"bindings\":[11,12,0,0,0,"
          "0,0,0,9,10,1,6]}");
  pumpApp(p, port, h);
  CHECK(h.deviceConfig()->revision == 2 &&
        h.deviceConfig()->profiles[3].io4_bindings[10] ==
            common::Io4Function::LeftA &&
        flash.operations == 0);
  port.clearOutput();
  request(port, 2, "set_io4_bindings",
          "{\"expected_revision\":2,\"profile_id\":4,\"bindings\":[13,0,0,0,0,"
          "0,0,0,0,0,0,0]}");
  pumpApp(p, port, h);
  CHECK(contains(port.tx, "INVALID_PAYLOAD") &&
        h.deviceConfig()->revision == 2);
  port.clearOutput();
  request(port, 3, "get_usb_mode");
  pumpApp(p, port, h);
  CHECK(contains(port.tx, "\"current\":\"NORMAL_NKRO\"") &&
        contains(port.tx, "\"saved_startup\":null"));
  return 0;
}

extern "C" int
test_io4_host_lights_follow_user_brightness_disable_and_physical_mapping() {
  auto &h = usb::usbBackend();
  CHECK(h.configureBeforeConnect(1));
  common::DeviceConfig d;
  config::defaults(d);
  auto &p = d.profiles[1];
  // 重复功能映射的两个物理键都亮；PUSH没有灯，非ABC保留本地帧。
  p.io4_bindings[0] = common::Io4Function::LeftA;
  p.io4_bindings[11] = common::Io4Function::LeftA;
  CHECK(h.setProfile(p, 1));
  uint8_t out[63]{};
  out[0] = 4;
  out[1] = 0x40; // LeftA蓝色。
  h.output(0x10, HID_REPORT_TYPE_OUTPUT, out, 63);
  auto c = p.runtime.lighting;
  c.mode = common::LightMode::Static;
  for (uint8_t level = 0; level <= 100; ++level) {
    c.brightness_level = level;
    auto f = lighting::renderLights(c, 0, 0, true, {255, 0, 255});
    const auto local = f.keys[8];
    h.applyHostLights(f, c);
    const auto expected = uint8_t(255u * level / 200u);
    CHECK(f.keys[7].b == expected && !f.keys[7].r && !f.keys[7].g);
    CHECK(f.keys[0].b == expected && !f.keys[0].r && !f.keys[0].g);
    CHECK(f.keys[8].r == local.r && f.keys[8].g == local.g &&
          f.keys[8].b == local.b);
    CHECK(f.indicator.r == 63 && f.indicator.b == 63);
    uint32_t words[10], indicator;
    lighting::packLightWords(f, {}, true, words, indicator);
    CHECK((words[1] & 0xff00) == (uint32_t(expected) << 8));
  }
  c.enabled = false;
  auto f = lighting::renderLights(c, 0, 0, true, {255, 0, 255});
  h.applyHostLights(f, c);
  for (const auto &v : f.keys)
    CHECK(!v.r && !v.g && !v.b);
  CHECK(f.indicator.r == 63 && f.indicator.b == 63);
  c.enabled = true;
  c.brightness_level = 101;
  h.applyHostLights(f, c);
  CHECK(!f.keys[7].r && !f.keys[7].g && !f.keys[7].b);
  c.brightness_level = 100;
  __usb_mutex.locked = true;
  h.applyHostLights(f, c);
  __usb_mutex.locked = false;
  CHECK(!f.keys[7].b); // mutex忙时本地帧不被半份覆盖。
  h.applyHostLights(f, c);
  CHECK(f.keys[7].b == 127);
  CHECK(h.setProfile(p, 2));
  f = {};
  h.applyHostLights(f, c);
  CHECK(!f.keys[7].b); // 热配置撤销旧主机覆盖。
  return 0;
}
extern "C" int test_io4_indicator_persona_color_and_independent_disable() {
  MemoryFlash flash;
  app_flash = &flash;
  common::DeviceConfig c;
  config::defaults(c);
  c.startup_usb_mode = 1;
  c.active_id = 2;
  config::ConfigStore store(flash);
  CHECK(store.save(c) == config::SaveResult::Ok);
  app::App h;
  CHECK(h.prepareUsbBoot(false) == 1);
  CHECK(usb::usbBackend().configureBeforeConnect(1));
  h.beginCore0();
  h.beginCore1();
  tickApp(h, 60);
  CHECK(h.ready() && h.appliedConfig().indicator_color.g == 96);
  CHECK(mocklighting::last_frame.indicator.r == 63 &&
        mocklighting::last_frame.indicator.g == 0 &&
        mocklighting::last_frame.indicator.b == 63);
  c = *h.deviceConfig();
  c.revision = 2;
  c.profiles[1].runtime.indicator_enabled = false;
  CHECK(h.applyDevice(c, mockapp::now));
  tickApp(h, 60);
  const auto &v = mocklighting::last_frame.indicator;
  CHECK(h.ready() && !v.r && !v.g && !v.b);
  return 0;
}

// 普通CDC编辑->显式保存->冷启动IO4，执行真实协议/App/Flash/映射链路。
// 复位后的USB端点/对象重建是外围替身，不代表Windows枚举或实板复位验收。
extern "C" int test_io4_normal_edit_saved_mapping_survives_cold_boot() {
  MemoryFlash flash;
  app_flash = &flash;
  CHECK(usb::usbBackend().configureBeforeConnect(0));
  app::App normal;
  normal.beginCore0();
  normal.beginCore1();
  tickApp(normal);
  const auto original_keyboard =
      normal.deviceConfig()->profiles[3].runtime.keyboard;
  protocol::ConfigProtocol protocol;
  BusPort port;
  request(port, 1, "set_io4_bindings",
          "{\"expected_revision\":1,\"profile_id\":4,\"bindings\":[12,11,0,0,0,"
          "0,0,0,0,0,0,0]}");
  pumpApp(protocol, port, normal);
  CHECK(normal.ready() && normal.deviceConfig()->revision == 2 &&
        contains(port.tx, "\"ok\":true") && flash.operations == 0);
  CHECK(memcmp(&original_keyboard,
               &normal.deviceConfig()->profiles[3].runtime.keyboard,
               sizeof(original_keyboard)) == 0);
  port.clearOutput();
  request(port, 2, "set_startup_usb_mode",
          "{\"expected_revision\":2,\"mode\":\"IO4\"}");
  pumpApp(protocol, port, normal);
  CHECK(normal.usbMode() == 0 && normal.deviceConfig()->revision == 3);
  port.clearOutput();
  request(port, 3, "save_config", "{\"expected_revision\":3}");
  pumpApp(protocol, port, normal);
  CHECK(normal.savedRevision() == 3 && normal.savedUsbMode() == 1 &&
        contains(port.tx, "\"ok\":true"));
  port.clearOutput();
  request(port, 4, "reboot_to_saved_mode",
          "{\"expected_revision\":3,\"target\":\"IO4\"}");
  pumpApp(protocol, port, normal);
  CHECK(normal.rebootReady() && !TinyUSBDevice.detached);
  port.drained = true;
  pumpApp(protocol, port, normal, 1);
  tickApp(normal, 501);
  CHECK(TinyUSBDevice.detached && rp2040.reboots > 0);

  // ROM重启边界：保留同一Flash，重建后端和测试USB外围，随后正常启动新App。
  usb::usbBackend().~UsbBackend();
  ::new (static_cast<void *>(&usb::usbBackend())) usb::UsbBackend();
  mockusb::instances = 0;
  mockusb::endpoints[0] = {};
  mockusb::endpoints[1] = {};
  TinyUSBDevice = {};
  mockapp::raw = 0;
  app::App io4;
  CHECK(io4.prepareUsbBoot(false) == 1);
  CHECK(usb::usbBackend().configureBeforeConnect(1));
  io4.beginCore0();
  io4.beginCore1();
  tickApp(io4);
  CHECK(io4.ready() && io4.usbMode() == 1 && mockusb::instances == 1 &&
        io4.deviceConfig()->active_id == 4 && io4.savedRevision() == 3);
  mockapp::raw = 3; // BT1=Service(bit6)、BT2=Test(bit9)。
  tickApp(io4);
  const auto &report = mockusb::endpoints[0];
  CHECK(report.length == 64 && report.payload[0] == 1);
  CHECK(report.payload[29] == 0x40 && report.payload[30] == 0x42 &&
        report.payload[31] == 0 && report.payload[32] == 0x80);
  mockapp::raw = 0;
  tickApp(io4);
  CHECK(report.payload[29] == 0 && report.payload[30] == 0x40 &&
        report.payload[31] == 0 && report.payload[32] == 0x80);
  CHECK(mockusb::unlocked_access == 0);
  return 0;
}
