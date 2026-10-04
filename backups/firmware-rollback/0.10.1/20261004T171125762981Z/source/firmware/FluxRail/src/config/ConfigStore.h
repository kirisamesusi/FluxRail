#pragma once
#include "DeviceCodec.h"
namespace fluxrail {
namespace config {
constexpr uint32_t kSlotBytes = 8192, kPageBytes = 256, kCommitOffset = 7936;
constexpr uint32_t kSlotOffset[2] = {0x3fb000, 0x3fd000};
static_assert(kSlotOffset[0] % 4096 == 0 && kSlotOffset[1] % 4096 == 0 &&
                  kSlotOffset[1] >= kSlotOffset[0] + kSlotBytes &&
                  kSlotOffset[1] + kSlotBytes <= 0x3ff000,
              "independent erase regions required");
class FlashDevice {
public:
  virtual bool available() const = 0;
  virtual bool read(uint8_t slot, uint32_t offset, uint8_t *data,
                    uint32_t length) = 0;
  virtual bool erase(uint8_t slot) = 0;
  virtual bool program(uint8_t slot, uint32_t offset, const uint8_t *data,
                       uint32_t length) = 0;
  virtual ~FlashDevice() = default;
};
enum class SaveResult : uint8_t {
  None,
  Ok,
  Unavailable,
  InvalidSnapshot,
  ReadFailed,
  EraseFailed,
  ProgramFailed,
  VerifyFailed
};
class ConfigStore {
public:
  explicit ConfigStore(FlashDevice &flash) : flash_(flash) {}
  bool load(common::DeviceConfig &out);
  // 只能在公共维护暂停/ACK/全释放完成后调用；完整写入+读回，不自动预览写。
  SaveResult save(const common::DeviceConfig &snapshot);
  uint32_t savedRevision() const { return saved_revision_; }
  uint8_t savedMode() const { return saved_mode_; }
  uint32_t generation() const { return generation_; }
  uint8_t validSlots() const { return valid_slots_; }
  bool available() const { return flash_.available(); }

private:
  FlashDevice &flash_;
  DeviceCodec codec_;
  common::DeviceConfig verified_{};
  uint8_t image_[kSlotBytes]{}, read_page_[kPageBytes]{};
  uint32_t generation_ = 0, saved_revision_ = 0;
  int8_t active_slot_ = -1;
  uint8_t valid_slots_ = 0, saved_mode_ = 0;
  bool readSlot(uint8_t slot, uint32_t &generation, common::DeviceConfig &out);
};
} // namespace config
} // namespace fluxrail
