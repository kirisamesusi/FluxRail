#include "ProfileManager.h"
#include "../keyboard/KeyboardTypes.h"
#include "../lighting/KeyLighting.h"
#include <math.h>
#include <new>
namespace fluxrail {
namespace config {
uint32_t nextRevision(uint32_t r) { return r == UINT32_MAX ? 1 : r + 1; }
// 模板名称：UTF-8，1..31字节（槽位32字节含结尾0）。只接受1..3字节形式（基本多文
// 种平面）；ASCII仅允许可打印字符且不含引号/反斜杠。拒绝非最短形式、代理对、C1控制
// 符、零宽/双向控制符与非字符，避免界面名称被伪装。旧的纯ASCII名称保持有效；
// "最多10个字符"是Web新建/改名输入规则，固件不据此拒绝已保存的旧名称。
static bool nameCodePoint(uint32_t cp) {
  if (cp < 0x20 || cp == 0x7f || cp == '"' || cp == '\\')
    return false;
  if (cp >= 0x80 && cp <= 0x9f) // C1 controls
    return false;
  if (cp >= 0x200b && cp <= 0x200f) // zero width / direction marks
    return false;
  if (cp >= 0x2028 && cp <= 0x202e) // separators, bidi embedding/override
    return false;
  if (cp >= 0x2066 && cp <= 0x2069) // bidi isolates
    return false;
  if (cp == 0xfeff || cp >= 0xfff9) // BOM, annotation marks, noncharacters
    return false;
  return true;
}
bool validName(const char *n) {
  uint8_t i = 0;
  while (i < common::kProfileNameBytes) {
    const uint8_t c = static_cast<uint8_t>(n[i]);
    if (!c)
      return i > 0;
    uint32_t cp = c;
    uint8_t extra = 0;
    if (c < 0x80)
      extra = 0;
    else if (c >= 0xc2 && c <= 0xdf) { // C0/C1 would be overlong
      cp = c & 0x1f;
      extra = 1;
    } else if (c >= 0xe0 && c <= 0xef) {
      cp = c & 0x0f;
      extra = 2;
    } else
      return false; // continuation byte, overlong lead or 4-byte form (no emoji)
    if (i + extra >= common::kProfileNameBytes)
      return false;
    for (uint8_t k = 1; k <= extra; ++k) {
      const uint8_t d = static_cast<uint8_t>(n[i + k]);
      if ((d & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (d & 0x3f);
    }
    if ((extra == 2 && cp < 0x800) || (cp >= 0xd800 && cp <= 0xdfff))
      return false; // overlong three-byte form / surrogate
    if (!nameCodePoint(cp))
      return false;
    i += extra + 1;
  }
  return false;
}
static void nameCopy(char *d, const char *s) {
  for (uint8_t i = 0; i < 32; ++i) {
    d[i] = *s;
    if (*s)
      ++s;
  }
}
static void preset(common::Profile &p, uint32_t id) {
  p = common::Profile{};
  common::defaultIo4Bindings(p.io4_bindings);
  p.id = id;
  const char *names[] = {"SDVX", "ONGEKI", "IN FALSUS", "MULTI K"};
  nameCopy(p.name, names[id - 1]);
  // D06工程默认：四预置可目视区分；indicator_color沿用现有公共字段，
  // 复制/自定义保留来源色，已有Flash/导入/迁移不因默认变化而改色。
  // 只有首次默认与显式restorePreset使用本表；GP11最终仍是实际25%。
  const common::Rgb colors[] = {
      {0, 255, 255}, {255, 96, 0}, {128, 0, 255}, {0, 255, 0}};
  p.runtime.indicator_color = colors[id - 1];
  p.runtime.keyboard = keyboard::defaultKeyboardConfig();
  p.knobs[1].axis = common::MotionAxis::Y;
  p.hall.enabled = id == 2;
  p.knobs[0].enabled = p.knobs[1].enabled = id == 1;
}
void defaults(common::DeviceConfig &c) {
  ::new (static_cast<void *>(&c)) common::DeviceConfig{};
  for (uint8_t i = 0; i < 4; ++i)
    preset(c.profiles[i], i + 1);
}
common::Profile *find(common::DeviceConfig &c, uint32_t id) {
  for (uint8_t i = 0; i < c.profile_count && i < 12; ++i)
    if (c.profiles[i].id == id)
      return &c.profiles[i];
  return nullptr;
}
const common::Profile *find(const common::DeviceConfig &c, uint32_t id) {
  for (uint8_t i = 0; i < c.profile_count && i < 12; ++i)
    if (c.profiles[i].id == id)
      return &c.profiles[i];
  return nullptr;
}
bool validProfile(const common::Profile &p) {
  const auto &r = p.runtime;
  if (!p.id || !validName(p.name) || !keyboard::validConfig(r.keyboard) ||
      !lighting::validLightingConfig(r.lighting) ||
      r.scan.press_debounce_us > 20000 || r.scan.release_debounce_us > 20000 ||
      !hall::validateSettings(p.hall) || p.routing.idle_us > 100000)
    return false;
  for (const auto &k : p.knobs)
    if (!knobs::validate(k))
      return false;
  for (auto f : p.io4_bindings)
    if (!common::validIo4Function(f))
      return false;
  return true;
}
bool validDevice(const common::DeviceConfig &c) {
  if (c.schema != common::kDeviceSchema || !c.revision || c.profile_count < 4 ||
      c.profile_count > 12 || c.startup_usb_mode > 1 || c.next_id < 5 ||
      !find(c, c.active_id))
    return false;
  for (uint32_t id = 1; id <= 4; ++id)
    if (!find(c, id))
      return false;
  for (uint8_t i = 0; i < c.profile_count; ++i) {
    if (!validProfile(c.profiles[i]) || c.profiles[i].id >= c.next_id)
      return false;
    for (uint8_t j = 0; j < i; ++j)
      if (c.profiles[i].id == c.profiles[j].id)
        return false;
  }
  return !c.calibration.valid || hall::validateCalibration(c.calibration);
}
common::RuntimeConfig activeRuntime(const common::DeviceConfig &c) {
  auto p = find(c, c.active_id);
  auto r = p ? p->runtime : common::RuntimeConfig{};
  r.revision = c.revision;
  return r;
}
bool createProfile(common::DeviceConfig &c, uint32_t copy_id,
                   const char *name) {
  const auto *p = find(c, copy_id);
  if (!p || c.profile_count == 12 || c.next_id == UINT32_MAX ||
      !validName(name))
    return false;
  c.profiles[c.profile_count] = *p;
  auto &n = c.profiles[c.profile_count++];
  n.id = c.next_id++;
  nameCopy(n.name, name);
  return true;
}
bool renameProfile(common::DeviceConfig &c, uint32_t id, const char *name) {
  auto p = find(c, id);
  if (!p || !validName(name))
    return false;
  nameCopy(p->name, name);
  return true;
}
bool deleteProfile(common::DeviceConfig &c, uint32_t id) {
  if (id <= 4 || !find(c, id))
    return false;
  for (uint8_t i = 0; i < c.profile_count; ++i)
    if (c.profiles[i].id == id) {
      for (uint8_t j = i; j + 1 < c.profile_count; ++j)
        c.profiles[j] = c.profiles[j + 1];
      --c.profile_count;
      c.profiles[c.profile_count] = common::Profile{};
      break;
    }
  if (c.active_id == id)
    c.active_id = 4;
  return true;
}
bool restorePreset(common::DeviceConfig &c, uint32_t id) {
  auto p = find(c, id);
  if (id < 1 || id > 4 || !p)
    return false;
  preset(*p, id);
  return true;
}
} // namespace config
} // namespace fluxrail
