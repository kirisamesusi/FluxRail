#pragma once
#include "../common/DeviceConfig.h"
#include "../lighting/KeyLighting.h"
#include "Io4Codec.h"
#include "NormalHid.h"
#ifndef FLUXRAIL_IO4_CONFIG_CDC
#define FLUXRAIL_IO4_CONFIG_CDC 0
#endif
namespace fluxrail {
namespace usb {
struct Io4Stats {
  uint32_t submitted = 0, completed = 0, failed = 0, busy = 0, bad_output = 0,
           disconnects = 0, invalid_motion = 0;
};
// 所有方法核心0；回调由同一库所有者按当前身份分派。
class UsbBackend final : public keyboard::ActionSink {
public:
  bool configureBeforeConnect(uint8_t mode);
  uint8_t mode() const { return mode_; }
  bool hasCdc() const { return mode_ == 0 || FLUXRAIL_IO4_CONFIG_CDC; }
  bool startupOk() const {
    return mode_ ? initialized_ : normalHid().startupOk();
  }
  bool connectionState(bool &state) const;
  bool quiescent();
  bool detachIfQuiescent();
  bool transportFaulted() const {
    return mode_ ? faulted_.load(std::memory_order_acquire)
                 : normalHid().transportFaulted();
  }
  void clearTransportFault() {
    if (mode_)
      faulted_.store(false, std::memory_order_release);
    else
      normalHid().clearTransportFault();
  }
  bool tryKeyboard(const common::KeyboardState &s) override {
    return !mode_ && normalHid().tryKeyboard(s);
  }
  bool tryMouseButtons(uint8_t b) override {
    return !mode_ && normalHid().tryMouseButtons(b);
  }
  bool usesPhysical() const override { return mode_ == 1; }
  bool tryPhysicalState(uint16_t mask) override;
  bool outputsComplete() override { return quiescent(); }
  bool setProfile(const common::Profile &p, uint32_t revision);
  bool edge(const common::InputEdge &e);
  bool relative(int16_t x, int16_t y) {
    return !mode_ && normalHid().tryRelative(x, y);
  }
  // 已配置绝对 output，仅核心0；真实采集调度尚未接入，不接受假采集。
  bool absolute(float output, bool valid, uint32_t revision, uint32_t epoch,
                uint32_t expected_revision, uint32_t expected_epoch,
                int16_t y_counts = 0);
  void service(uint32_t now, bool streaming);
  void disconnect();
  void clearMotionAndLights();
  // IO4只覆盖功能灯颜色；已生效模板开关/亮度始终生效，不改保存配色。
  void applyHostLights(lighting::LightFrame &frame,
                       const common::LightingConfig &lighting);
  bool readStats(Io4Stats &stats, uint32_t &rejected, uint32_t &timeouts);
  uint16_t get(uint8_t id, hid_report_type_t type, uint8_t *bytes,
               uint16_t requested);
  void output(uint8_t id, hid_report_type_t type, const uint8_t *bytes,
              uint16_t size);
  void completed(uint8_t instance, uint16_t bytes);
  void failed(uint8_t instance);

private:
  Adafruit_USBD_HID io4_{kIo4Descriptor, kIo4DescriptorBytes,
                         HID_ITF_PROTOCOL_NONE, 1, true};
  Io4State state_{};
  common::Io4Function bindings_[12]{};
  std::atomic<bool> faulted_{false};
  Io4Stats stats_{};
  uint8_t mode_ = 0;
  bool initialized_ = false, have_sequence_ = false;
  bool configuration_attempted_ = false, reset_pending_ = false,
       clear_pending_ = false;
  void applyPending();
  uint16_t mask_ = 0;
  uint32_t last_sequence_ = 0, revision_ = 0, last_sent_us_ = 0;
  bool send(uint16_t mask);
};
UsbBackend &usbBackend();
} // namespace usb
} // namespace fluxrail
