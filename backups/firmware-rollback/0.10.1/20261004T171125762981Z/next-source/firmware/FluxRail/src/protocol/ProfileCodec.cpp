#include "ProfileCodec.h"
#include "../config/ProfileManager.h"
namespace fluxrail {
namespace protocol {
void encodeMotion(JsonWriter &w, const common::Profile &p) {
  w.literal("{\"hall\":{");
  w.literal("\"enabled\":");
  w.boolean(p.hall.enabled);
  w.literal(",\"invert\":");
  w.boolean(p.hall.invert);
  w.literal(",\"center_gain\":");
  w.real(p.hall.center_gain);
  w.literal(",\"range_min\":");
  w.real(p.hall.range_min);
  w.literal(",\"range_max\":");
  w.real(p.hall.range_max);
  w.literal(",\"quiet_tau_us\":");
  w.number(p.hall.quiet_tau_us);
  w.literal(",\"moving_tau_us\":");
  w.number(p.hall.moving_tau_us);
  w.literal(",\"fast_speed_mm_s\":");
  w.real(p.hall.fast_speed_mm_s);
  w.literal(",\"activity_mm\":");
  w.real(p.hall.activity_mm);
  w.literal(",\"activity_hold_us\":");
  w.number(p.hall.activity_hold_us);
  w.literal(",\"enter_signal\":");
  w.real(p.hall.enter_signal);
  w.literal(",\"exit_signal\":");
  w.real(p.hall.exit_signal);
  w.literal(",\"curve_tolerance\":");
  w.real(p.hall.curve_tolerance);
  w.literal(",\"max_gap_us\":");
  w.number(p.hall.max_gap_us);
  w.literal("},\"knobs\":[");
  for (uint8_t i = 0; i < 2; ++i) {
    if (i)
      w.literal(",");
    const auto &k = p.knobs[i];
    w.literal("{\"enabled\":");
    w.boolean(k.enabled);
    w.literal(",\"axis\":");
    w.number(uint8_t(k.axis));
    w.literal(",\"invert\":");
    w.boolean(k.invert);
    w.literal(",\"gain\":");
    w.real(k.gain);
    w.literal(",\"counts_per_turn_at_gain1\":");
    w.real(k.counts_per_turn_at_gain1);
    w.literal(",\"filter_tau_us\":");
    w.number(k.filter_tau_us);
    w.literal(",\"max_gap_us\":");
    w.number(k.max_gap_us);
    w.literal(",\"max_turns_per_second\":");
    w.real(k.max_turns_per_second);
    w.literal(",\"activity_ticks\":");
    w.number(k.activity_ticks);
    w.literal(",\"activity_hold_us\":");
    w.number(k.activity_hold_us);
    w.literal(",\"push\":{\"enabled\":");
    w.boolean(k.push.enabled);
    w.literal(",\"press_us\":");
    w.number(k.push.press_us);
    w.literal(",\"rotating_press_us\":");
    w.number(k.push.rotating_press_us);
    w.literal(",\"release_us\":");
    w.number(k.push.release_us);
    w.literal(",\"rearm_us\":");
    w.number(k.push.rearm_us);
    w.literal(",\"max_hold_us\":");
    w.number(k.push.max_hold_us);
    w.literal(",\"max_sample_gap_us\":");
    w.number(k.push.max_sample_gap_us);
    w.literal("}}");
  }
  w.literal("],\"routing\":{\"idle_us\":");
  w.number(p.routing.idle_us);
  w.literal(",\"simultaneous_hall_first\":");
  w.boolean(p.routing.simultaneous_hall_first);
  w.literal("}}");
}
bool decodeMotion(const Json &j, uint16_t t, common::Profile &out) {
  const char *const keys[] = {"hall", "knobs", "routing"};
  if (!j.shape(t, keys, 3))
    return false;
  const auto h = j.field(t, "hall"), kn = j.field(t, "knobs"),
             r = j.field(t, "routing");
  const char *const hk[] = {
      "enabled",         "invert",           "center_gain",   "range_min",
      "range_max",       "quiet_tau_us",     "moving_tau_us", "fast_speed_mm_s",
      "activity_mm",     "activity_hold_us", "enter_signal",  "exit_signal",
      "curve_tolerance", "max_gap_us"};
  if (!j.shape(h, hk, 14))
    return false;
  if (!j.boolValue(j.field(h, "enabled"), out.hall.enabled))
    return false;
  if (!j.boolValue(j.field(h, "invert"), out.hall.invert))
    return false;
  if (!j.floatValue(j.field(h, "center_gain"), out.hall.center_gain))
    return false;
  if (!j.floatValue(j.field(h, "range_min"), out.hall.range_min))
    return false;
  if (!j.floatValue(j.field(h, "range_max"), out.hall.range_max))
    return false;
  if (!j.uintValue(j.field(h, "quiet_tau_us"), out.hall.quiet_tau_us))
    return false;
  if (!j.uintValue(j.field(h, "moving_tau_us"), out.hall.moving_tau_us))
    return false;
  if (!j.floatValue(j.field(h, "fast_speed_mm_s"), out.hall.fast_speed_mm_s))
    return false;
  if (!j.floatValue(j.field(h, "activity_mm"), out.hall.activity_mm))
    return false;
  if (!j.uintValue(j.field(h, "activity_hold_us"), out.hall.activity_hold_us))
    return false;
  if (!j.floatValue(j.field(h, "enter_signal"), out.hall.enter_signal))
    return false;
  if (!j.floatValue(j.field(h, "exit_signal"), out.hall.exit_signal))
    return false;
  if (!j.floatValue(j.field(h, "curve_tolerance"), out.hall.curve_tolerance))
    return false;
  if (!j.uintValue(j.field(h, "max_gap_us"), out.hall.max_gap_us))
    return false;
  if (kn == Json::kMissing || j[kn].kind != JsonKind::Array || j.count(kn) != 2)
    return false;
  uint16_t n = kn + 1;
  for (auto &k : out.knobs) {
    const char *const kk[] = {"enabled",
                              "axis",
                              "invert",
                              "gain",
                              "counts_per_turn_at_gain1",
                              "filter_tau_us",
                              "max_gap_us",
                              "max_turns_per_second",
                              "activity_ticks",
                              "activity_hold_us",
                              "push"};
    uint32_t axis, ticks;
    if (!j.shape(n, kk, 11) || !j.boolValue(j.field(n, "enabled"), k.enabled) ||
        !j.uintValue(j.field(n, "axis"), axis) || axis > 1 ||
        !j.boolValue(j.field(n, "invert"), k.invert) ||
        !j.doubleValue(j.field(n, "gain"), k.gain) ||
        !j.doubleValue(j.field(n, "counts_per_turn_at_gain1"),
                       k.counts_per_turn_at_gain1) ||
        !j.uintValue(j.field(n, "filter_tau_us"), k.filter_tau_us) ||
        !j.uintValue(j.field(n, "max_gap_us"), k.max_gap_us) ||
        !j.doubleValue(j.field(n, "max_turns_per_second"),
                       k.max_turns_per_second) ||
        !j.uintValue(j.field(n, "activity_ticks"), ticks) || ticks > 65535 ||
        !j.uintValue(j.field(n, "activity_hold_us"), k.activity_hold_us))
      return false;
    k.axis = common::MotionAxis(axis);
    k.activity_ticks = uint16_t(ticks);
    const auto ps = j.field(n, "push");
    const char *const pk[] = {
        "enabled",  "press_us",    "rotating_press_us", "release_us",
        "rearm_us", "max_hold_us", "max_sample_gap_us"};
    if (!j.shape(ps, pk, 7) ||
        !j.boolValue(j.field(ps, "enabled"), k.push.enabled) ||
        !j.uintValue(j.field(ps, "press_us"), k.push.press_us) ||
        !j.uintValue(j.field(ps, "rotating_press_us"),
                     k.push.rotating_press_us) ||
        !j.uintValue(j.field(ps, "release_us"), k.push.release_us) ||
        !j.uintValue(j.field(ps, "rearm_us"), k.push.rearm_us) ||
        !j.uintValue(j.field(ps, "max_hold_us"), k.push.max_hold_us) ||
        !j.uintValue(j.field(ps, "max_sample_gap_us"),
                     k.push.max_sample_gap_us))
      return false;
    n = j[n].next;
  }
  const char *const rk[] = {"idle_us", "simultaneous_hall_first"};
  return j.shape(r, rk, 2) &&
         j.uintValue(j.field(r, "idle_us"), out.routing.idle_us) &&
         j.boolValue(j.field(r, "simultaneous_hall_first"),
                     out.routing.simultaneous_hall_first) &&
         config::validProfile(out);
}
} // namespace protocol
} // namespace fluxrail
