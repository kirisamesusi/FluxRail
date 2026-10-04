#include "DeviceCodec.h"
#include <new>
#include <string.h>
namespace fluxrail {
namespace config {
static_assert(sizeof(double) == 8, "schema requires binary64");
static_assert(sizeof(float) == 4, "schema requires binary32");
uint32_t crc32(const uint8_t *p, size_t n) {
  uint32_t c = 0xffffffff;
  for (size_t i = 0; i < n; ++i) {
    c ^= p[i];
    for (uint8_t k = 0; k < 8; ++k)
      c = (c >> 1) ^ ((c & 1) ? 0xedb88320 : 0);
  }
  return c ^ 0xffffffff;
}
namespace {
struct Bytes {
  uint8_t *dst = nullptr;
  const uint8_t *src = nullptr;
  size_t pos = 0, size = 0;
  bool ok = true;
  bool reading() const { return src != nullptr; }
  void u8(uint8_t &v) {
    if (pos >= size) {
      ok = false;
      return;
    }
    if (reading())
      v = src[pos];
    else
      dst[pos] = v;
    ++pos;
  }
  void u16(uint16_t &v) {
    uint8_t a = uint8_t(v), c = uint8_t(v >> 8);
    u8(a);
    u8(c);
    if (reading())
      v = uint16_t(a) | (uint16_t(c) << 8);
  }
  void u32(uint32_t &v) {
    uint32_t out = 0;
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t x = uint8_t(v >> (8 * i));
      u8(x);
      out |= uint32_t(x) << (8 * i);
    }
    if (reading())
      v = out;
  }
  void boolean(bool &v) {
    uint8_t x = v ? 1 : 0;
    u8(x);
    if (x > 1)
      ok = false;
    if (reading())
      v = x == 1;
  }
  template <class T> void enumeration(T &v) {
    uint8_t x = uint8_t(v);
    u8(x);
    if (reading())
      v = T(x);
  }
  void real(float &v) {
    uint32_t x;
    memcpy(&x, &v, 4);
    u32(x);
    if (reading())
      memcpy(&v, &x, 4);
  }
  void real64(double &v) {
    uint64_t x;
    memcpy(&x, &v, 8);
    uint32_t lo = uint32_t(x), hi = uint32_t(x >> 32);
    u32(lo);
    u32(hi);
    if (reading()) {
      x = uint64_t(lo) | (uint64_t(hi) << 32);
      memcpy(&v, &x, 8);
    }
  }
  void color(common::Rgb &c) {
    u8(c.r);
    u8(c.g);
    u8(c.b);
  }
};
void profile(Bytes &b, common::Profile &p) {
  b.u32(p.id);
  for (auto &c : p.name) {
    uint8_t v = uint8_t(c);
    b.u8(v);
    if (b.reading())
      c = char(v);
  }
  for (auto &a : p.runtime.keyboard.bindings) {
    b.enumeration(a.type);
    b.u8(a.usage);
    b.u8(a.modifiers);
    b.u8(a.button);
  }
  auto &r = p.runtime;
  b.u32(r.keyboard.retrigger_gap_us);
  b.u32(r.scan.press_debounce_us);
  b.u32(r.scan.release_debounce_us);
  b.boolean(r.lighting.enabled);
  b.enumeration(r.lighting.mode);
  b.u8(r.lighting.brightness_level);
  b.boolean(r.lighting.per_key);
  b.u32(r.lighting.period_ms);
  b.boolean(r.lighting.reverse);
  b.boolean(r.indicator_enabled);
  b.color(r.lighting.global_color);
  for (auto &c : r.lighting.key_colors)
    b.color(c);
  b.color(r.indicator_color);
  auto &h = p.hall;
  b.boolean(h.enabled);
  b.boolean(h.invert);
  b.real(h.center_gain);
  b.real(h.range_min);
  b.real(h.range_max);
  b.u32(h.quiet_tau_us);
  b.u32(h.moving_tau_us);
  b.real(h.fast_speed_mm_s);
  b.real(h.activity_mm);
  b.u32(h.activity_hold_us);
  b.real(h.enter_signal);
  b.real(h.exit_signal);
  b.real(h.curve_tolerance);
  b.u32(h.max_gap_us);
  for (auto &k : p.knobs) {
    b.boolean(k.enabled);
    b.enumeration(k.axis);
    b.boolean(k.invert);
    b.real64(k.gain);
    b.real64(k.counts_per_turn_at_gain1);
    b.u32(k.filter_tau_us);
    b.u32(k.max_gap_us);
    b.real64(k.max_turns_per_second);
    b.u16(k.activity_ticks);
    b.u32(k.activity_hold_us);
    b.boolean(k.push.enabled);
    b.u32(k.push.press_us);
    b.u32(k.push.rotating_press_us);
    b.u32(k.push.release_us);
    b.u32(k.push.rearm_us);
    b.u32(k.push.max_hold_us);
    b.u32(k.push.max_sample_gap_us);
  }
  b.u32(p.routing.idle_us);
  b.boolean(p.routing.simultaneous_hall_first);
}
void snapshot(Bytes &b, common::DeviceConfig &c) {
  b.u32(c.schema);
  const uint32_t wire_schema = c.schema;
  if (wire_schema != 1 && wire_schema != 2) {
    b.ok = false;
    return;
  }
  b.u32(c.revision);
  b.u32(c.active_id);
  b.u32(c.next_id);
  b.u8(c.profile_count);
  b.u8(c.startup_usb_mode);
  if (c.profile_count < 4 || c.profile_count > 12) {
    b.ok = false;
    return;
  }
  if (wire_schema == 1 && c.startup_usb_mode != 0) {
    b.ok = false;
    return;
  }
  for (uint8_t i = 0; i < c.profile_count; ++i) {
    profile(b, c.profiles[i]);
    if (wire_schema == 2)
      for (auto &f : c.profiles[i].io4_bindings)
        b.enumeration(f);
    else
      common::defaultIo4Bindings(c.profiles[i].io4_bindings);
  }
  if (b.reading())
    c.schema = common::kDeviceSchema;
  b.u32(c.calibration_generation);
  auto &a = c.calibration;
  b.boolean(a.valid);
  if (!a.valid)
    return;
  b.u16(a.version);
  for (uint8_t i = 0; i < 3; ++i) {
    b.real(a.baseline[i]);
    b.real(a.baseline_noise[i]);
    b.real(a.gain[i]);
    uint8_t v = uint8_t(a.polarity[i]);
    b.u8(v);
    if (b.reading())
      a.polarity[i] = v == 255 ? -1 : (v == 1 ? 1 : 0);
  }
  b.real(a.left_mm);
  b.real(a.right_mm);
  b.u8(a.node_count);
  if (a.node_count < 7 || a.node_count > 17) {
    b.ok = false;
    return;
  }
  for (uint8_t j = 0; j < a.node_count; ++j) {
    auto &n = a.nodes[j];
    b.real(n.mm);
    for (auto &v : n.response)
      b.real(v);
    for (auto &v : n.noise)
      b.real(v);
  }
  b.u32(a.sweep_segments);
  b.boolean(a.sweep_left);
  b.boolean(a.sweep_right);
  b.real(a.noise_mm);
}
} // namespace
size_t DeviceCodec::encode(const common::DeviceConfig &c, uint8_t *p,
                           size_t n) {
  if (!validDevice(c))
    return 0;
  // 写入visitor不会修改源；仅byte reader赋值，保持完整快照不变。
  Bytes b{p, nullptr, 0, n, true};
  snapshot(b, const_cast<common::DeviceConfig &>(c));
  return b.ok ? b.pos : 0;
}
bool DeviceCodec::decode(const uint8_t *p, size_t n,
                         common::DeviceConfig &out) {
  if (!p || n < 4)
    return false;
  ::new (static_cast<void *>(&candidate_)) common::DeviceConfig{};
  Bytes b{nullptr, p, 0, n, true};
  snapshot(b, candidate_);
  if (!b.ok || b.pos != n || !validDevice(candidate_))
    return false;
  out = candidate_;
  return true;
}
} // namespace config
} // namespace fluxrail
