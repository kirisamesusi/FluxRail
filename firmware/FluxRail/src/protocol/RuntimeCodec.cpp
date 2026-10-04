#include "RuntimeCodec.h"
#include "../keyboard/KeyboardTypes.h"
#include "../lighting/KeyLighting.h"
namespace fluxrail {
namespace protocol {
namespace {
bool integer(const Json &j, uint16_t obj, const char *key, uint32_t &v,
             uint32_t max) {
  return j.uintValue(j.field(obj, key), v) && v <= max;
}
bool byte(const Json &j, uint16_t obj, const char *key, uint8_t &v) {
  uint32_t n;
  if (!integer(j, obj, key, n, 255))
    return false;
  v = static_cast<uint8_t>(n);
  return true;
}
bool boolean(const Json &j, uint16_t obj, const char *key, bool &v) {
  return j.boolValue(j.field(obj, key), v);
}
bool color(const Json &j, uint16_t i, common::Rgb &c) {
  if (i == Json::kMissing || j[i].kind != JsonKind::Array || j.count(i) != 3)
    return false;
  uint16_t p = i + 1;
  uint8_t *values[] = {&c.r, &c.g, &c.b};
  for (auto v : values) {
    uint32_t n;
    if (!j.uintValue(p, n) || n > 255)
      return false;
    *v = static_cast<uint8_t>(n);
    p = j[p].next;
  }
  return true;
}
void rgb(JsonWriter &w, const common::Rgb &c) {
  w.literal("[");
  w.number(c.r);
  w.literal(",");
  w.number(c.g);
  w.literal(",");
  w.number(c.b);
  w.literal("]");
}
const char *const modes[] = {"static", "breathing", "color_cycle",
                             "rainbow_wave", "press"};
} // namespace
bool decodeRuntime(const Json &j, uint16_t obj, common::RuntimeConfig &out) {
  const char *const root[] = {"keyboard", "scan", "lighting", "indicator"};
  if (!j.shape(obj, root, 4))
    return false;
  common::RuntimeConfig c = out;
  const auto k = j.field(obj, "keyboard"), s = j.field(obj, "scan"),
             l = j.field(obj, "lighting"), i = j.field(obj, "indicator");
  const char *const kk[] = {"bindings", "retrigger_gap_us"};
  const char *const sk[] = {"press_debounce_us", "release_debounce_us"};
  const char *const lk[] = {"enabled",   "mode",         "brightness_level",
                            "per_key",   "global_color", "key_colors",
                            "period_ms", "reverse"};
  const char *const ik[] = {"enabled", "color"};
  if (!j.shape(k, kk, 2) || !j.shape(s, sk, 2) || !j.shape(l, lk, 8) ||
      !j.shape(i, ik, 2))
    return false;
  if (!integer(j, k, "retrigger_gap_us", c.keyboard.retrigger_gap_us, 100000) ||
      !integer(j, s, "press_debounce_us", c.scan.press_debounce_us, 20000) ||
      !integer(j, s, "release_debounce_us", c.scan.release_debounce_us, 20000))
    return false;
  const auto bindings = j.field(k, "bindings");
  if (j[bindings].kind != JsonKind::Array ||
      j.count(bindings) != common::kSourceCount)
    return false;
  uint16_t p = bindings + 1;
  for (uint8_t source = 0; source < common::kSourceCount; ++source) {
    const char *const bk[] = {"source_id", "action"};
    uint32_t id;
    if (!j.shape(p, bk, 2) || !integer(j, p, "source_id", id, 11) ||
        id != source)
      return false;
    const auto a = j.field(p, "action"), t = j.field(a, "type");
    common::Binding b;
    const char *const disabled[] = {"type"};
    const char *const key[] = {"type", "usage", "modifiers"};
    const char *const mouse[] = {"type", "button"};
    if (j.text(t, "disabled")) {
      if (!j.shape(a, disabled, 1))
        return false;
    } else if (j.text(t, "keyboard")) {
      b.type = common::ActionType::Keyboard;
      if (!j.shape(a, key, 3) || !byte(j, a, "usage", b.usage) ||
          !byte(j, a, "modifiers", b.modifiers))
        return false;
    } else if (j.text(t, "mouse_button")) {
      b.type = common::ActionType::MouseButton;
      if (!j.shape(a, mouse, 2) || !byte(j, a, "button", b.button))
        return false;
    } else
      return false;
    c.keyboard.bindings[source] = b;
    p = j[p].next;
  }
  if (!boolean(j, l, "enabled", c.lighting.enabled) ||
      !boolean(j, l, "per_key", c.lighting.per_key) ||
      !boolean(j, l, "reverse", c.lighting.reverse) ||
      !byte(j, l, "brightness_level", c.lighting.brightness_level) ||
      !integer(j, l, "period_ms", c.lighting.period_ms, 60000) ||
      !color(j, j.field(l, "global_color"), c.lighting.global_color))
    return false;
  bool found = false;
  for (uint8_t m = 0; m < 5; ++m)
    if (j.text(j.field(l, "mode"), modes[m])) {
      c.lighting.mode = static_cast<common::LightMode>(m);
      found = true;
    }
  if (!found)
    return false;
  const auto colors = j.field(l, "key_colors");
  if (j[colors].kind != JsonKind::Array || j.count(colors) != common::kKeyCount)
    return false;
  p = colors + 1;
  for (auto &c0 : c.lighting.key_colors) {
    if (!color(j, p, c0))
      return false;
    p = j[p].next;
  }
  if (!boolean(j, i, "enabled", c.indicator_enabled) ||
      !color(j, j.field(i, "color"), c.indicator_color) ||
      !keyboard::validConfig(c.keyboard) ||
      !lighting::validLightingConfig(c.lighting))
    return false;
  out = c;
  return true; // Invalid input never publishes a partial configuration.
}
void encodeRuntime(JsonWriter &w, const common::RuntimeConfig &c) {
  w.literal("{\"keyboard\":{\"bindings\":[");
  for (uint8_t source = 0; source < common::kSourceCount; ++source) {
    if (source)
      w.literal(",");
    const auto &b = c.keyboard.bindings[source];
    w.literal("{\"source_id\":");
    w.number(source);
    w.literal(",\"action\":{\"type\":\"");
    if (b.type == common::ActionType::Keyboard) {
      w.literal("keyboard\",\"usage\":");
      w.number(b.usage);
      w.literal(",\"modifiers\":");
      w.number(b.modifiers);
    } else if (b.type == common::ActionType::MouseButton) {
      w.literal("mouse_button\",\"button\":");
      w.number(b.button);
    } else
      w.literal("disabled\"");
    w.literal("}}");
  }
  w.literal("],\"retrigger_gap_us\":");
  w.number(c.keyboard.retrigger_gap_us);
  w.literal("},\"scan\":{\"press_debounce_us\":");
  w.number(c.scan.press_debounce_us);
  w.literal(",\"release_debounce_us\":");
  w.number(c.scan.release_debounce_us);
  w.literal("},\"lighting\":{\"enabled\":");
  w.boolean(c.lighting.enabled);
  w.literal(",\"mode\":\"");
  w.literal(modes[static_cast<uint8_t>(c.lighting.mode)]);
  w.literal("\",\"brightness_level\":");
  w.number(c.lighting.brightness_level);
  w.literal(",\"per_key\":");
  w.boolean(c.lighting.per_key);
  w.literal(",\"global_color\":");
  rgb(w, c.lighting.global_color);
  w.literal(",\"key_colors\":[");
  for (uint8_t n = 0; n < common::kKeyCount; ++n) {
    if (n)
      w.literal(",");
    rgb(w, c.lighting.key_colors[n]);
  }
  w.literal("],\"period_ms\":");
  w.number(c.lighting.period_ms);
  w.literal(",\"reverse\":");
  w.boolean(c.lighting.reverse);
  w.literal("},\"indicator\":{\"enabled\":");
  w.boolean(c.indicator_enabled);
  w.literal(",\"color\":");
  rgb(w, c.indicator_color);
  w.literal("}}");
}
} // namespace protocol
} // namespace fluxrail
