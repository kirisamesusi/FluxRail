#include "ConfigStore.h"
#include <string.h>
namespace fluxrail {
namespace config {
namespace {
constexpr uint32_t kMagic = 0x314c5246, kCommit = 0x54494d43;
uint32_t get(const uint8_t *p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
void put(uint8_t *p, uint32_t v) {
  for (uint8_t i = 0; i < 4; ++i)
    p[i] = uint8_t(v >> (8 * i));
}
bool newer(uint32_t a, uint32_t b) {
  const uint32_t d = a - b;
  return d && d < 0x80000000u;
}
} // namespace
bool ConfigStore::readSlot(uint8_t slot, uint32_t &generation,
                           common::DeviceConfig &out) {
  if (!flash_.read(slot, 0, image_, kSlotBytes))
    return false;
  const auto n = get(image_ + 8), g = get(image_ + 12), rev = get(image_ + 16),
             pc = get(image_ + 20), hc = get(image_ + 24);
  const auto *m = image_ + kCommitOffset;
  if (get(image_) != kMagic ||
      (get(image_ + 4) != 1 && get(image_ + 4) != common::kDeviceSchema) ||
      n == 0 || n > kCommitOffset - 256 || !g || !rev ||
      get(image_ + 28) != 0 || hc != crc32(image_, 24) ||
      pc != crc32(image_ + 256, n) || get(m) != kCommit || get(m + 4) != g ||
      get(m + 8) != pc || get(m + 12) != hc || get(m + 16) != crc32(m, 16))
    return false;
  for (uint32_t i = 32; i < 256; ++i)
    if (image_[i] != 255)
      return false;
  for (uint32_t i = 256 + n; i < kCommitOffset; ++i)
    if (image_[i] != 255)
      return false;
  for (uint32_t i = kCommitOffset + 20; i < kSlotBytes; ++i)
    if (image_[i] != 255)
      return false;
  if (n < 4 || get(image_ + 256) != get(image_ + 4))
    return false;
  if (!codec_.decode(image_ + 256, n, out) || out.revision != rev)
    return false;
  generation = g;
  return true;
}
bool ConfigStore::load(common::DeviceConfig &out) {
  generation_ = saved_revision_ = 0;
  saved_mode_ = 0;
  active_slot_ = -1;
  valid_slots_ = 0;
  if (!available())
    return false;
  for (uint8_t slot = 0; slot < 2; ++slot) {
    uint32_t g = 0;
    if (readSlot(slot, g, verified_)) {
      valid_slots_ |= uint8_t(1u << slot);
      if (active_slot_ < 0 || newer(g, generation_)) {
        out = verified_;
        generation_ = g;
        saved_revision_ = out.revision;
        saved_mode_ = out.startup_usb_mode;
        active_slot_ = slot;
      }
    }
  }
  return active_slot_ >= 0;
}
SaveResult ConfigStore::save(const common::DeviceConfig &c) {
  if (!available())
    return SaveResult::Unavailable;
  memset(image_, 255, sizeof(image_));
  const size_t n = DeviceCodec::encode(c, image_ + 256, kCommitOffset - 256);
  if (!n)
    return SaveResult::InvalidSnapshot;
  const uint8_t slot = active_slot_ == 0 ? 1 : 0;
  const uint32_t g = nextRevision(generation_);
  put(image_, kMagic);
  put(image_ + 4, common::kDeviceSchema);
  put(image_ + 8, n);
  put(image_ + 12, g);
  put(image_ + 16, c.revision);
  put(image_ + 20, crc32(image_ + 256, n));
  put(image_ + 24, crc32(image_, 24));
  put(image_ + 28, 0);
  auto *m = image_ + kCommitOffset;
  put(m, kCommit);
  put(m + 4, g);
  put(m + 8, get(image_ + 20));
  put(m + 12, get(image_ + 24));
  put(m + 16, crc32(m, 16));
  valid_slots_ &= uint8_t(~(1u << slot));
  if (!flash_.erase(slot))
    return SaveResult::EraseFailed;
  const uint32_t end = (256 + uint32_t(n) + 255) & ~255u;
  for (uint32_t off = 0; off < end; off += 256)
    if (!flash_.program(slot, off, image_ + off, 256))
      return SaveResult::ProgramFailed;
  // precommit按字节验证header及payload，提交前不让启动加载器看到候选。
  for (uint32_t off = 0; off < end; off += 256) {
    if (!flash_.read(slot, off, read_page_, 256))
      return SaveResult::ReadFailed;
    if (memcmp(read_page_, image_ + off, 256))
      return SaveResult::VerifyFailed;
  }
  if (!flash_.program(slot, kCommitOffset, m, 256))
    return SaveResult::ProgramFailed;
  uint32_t actual = 0;
  if (!readSlot(slot, actual, verified_) || actual != g ||
      verified_.revision != c.revision)
    return SaveResult::VerifyFailed;
  active_slot_ = slot;
  generation_ = g;
  saved_revision_ = c.revision;
  saved_mode_ = verified_.startup_usb_mode;
  valid_slots_ |= uint8_t(1u << slot);
  return SaveResult::Ok;
}
} // namespace config
} // namespace fluxrail
