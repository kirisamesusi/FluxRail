#pragma once
#include "../common/MotionTypes.h"
#include "../config/ConfigStore.h"
#include "RuntimeCodec.h"
namespace fluxrail {
namespace protocol {
class ConfigHost {
public:
  virtual const common::RuntimeConfig &appliedConfig() const = 0;
  virtual uint32_t requestedRevision() const = 0;
  virtual bool ready() const = 0;
  virtual bool busy() const = 0;
  virtual bool failed() const = 0;
  virtual uint16_t physicalMask() const = 0;
  virtual const char *
  serial() const = 0; // Real board identifier: 16 hex digits.
  virtual bool apply(const common::RuntimeConfig &config, uint32_t now_us) = 0;
  virtual const common::DeviceConfig *deviceConfig() const { return nullptr; }
  virtual bool applyDevice(const common::DeviceConfig &, uint32_t) {
    return false;
  }
  virtual uint32_t savedRevision() const { return 0; }
  virtual bool flashAvailable() const { return false; }
  virtual uint8_t validSlots() const { return 0; }
  virtual bool save(uint32_t) { return false; }
  virtual bool saving() const { return false; }
  virtual config::SaveResult saveResult() const {
    return config::SaveResult::None;
  }
  virtual uint8_t usbMode() const { return 0; }
  virtual uint8_t savedUsbMode() const { return 0; }
  virtual bool forcedRecovery() const { return false; }
  virtual bool reboot(uint8_t, uint32_t) { return false; }
  virtual bool rebootReady() const { return false; }
  virtual bool rebootFailed() const { return false; }
  virtual void rebootResponseComplete(bool, uint32_t) {}
  virtual const char *maintenancePhase() const { return "idle"; }
  virtual const common::MotionSnapshot *motionSnapshot() const {
    return nullptr;
  }
  virtual bool motionAvailable() const { return false; }
  virtual bool calibrating() const { return false; }
  virtual bool calibrationCommand(const common::CalibrationCommand &,
                                  uint32_t) {
    return false;
  }
  virtual bool calibrationPending() const { return false; }
  virtual bool calibrationSucceeded() const { return false; }
  virtual bool bootloader(uint32_t) { return false; }
  virtual ~ConfigHost() = default;
};
class CdcPort {
public:
  virtual bool
  connectionState(bool &online) = 0; // false: mutex busy, state unobserved.
  virtual bool txComplete() {
    return false;
  } // FIFO empty AND actual endpoint complete.
  virtual int32_t readByte() = 0; // -1: no data or temporary busy.
  virtual uint32_t write(const char *data,
                         uint32_t length) = 0; // copied count; never wait.
  virtual ~CdcPort() = default;
};
struct ProtocolStats {
  uint32_t requests = 0, rejected = 0, overlong_lines = 0, session_resets = 0,
           apply_timeouts = 0;
};
class ConfigProtocol {
public:
  static constexpr uint16_t kMaxLine = 4096;
  static constexpr uint32_t kApplyTimeoutUs = 3000000;
  void service(uint32_t now_us, CdcPort &port, ConfigHost &host);
  const ProtocolStats &stats() const { return stats_; }

private:
  char rx_[kMaxLine]{}, tx_[kMaxLine + 1]{};
  Json json_;
  uint16_t rx_size_ = 0, tx_size_ = 0, tx_sent_ = 0;
  uint32_t id_ = 0, pending_revision_ = 0, apply_started_us_ = 0;
  bool online_ = false, dropping_ = false, pending_ = false;
  ProtocolStats stats_{};
  bool pending_save_ = false, pending_device_ = false;
  bool pending_reboot_ = false, reboot_response_ = false;
  bool pending_calibration_ = false;
  void motionReply(const ConfigHost &host, bool calibration_only);
  bool dispatchMotion(uint16_t op, uint16_t payload, uint32_t now,
                      ConfigHost &host);
  void modeReply(const ConfigHost &host);
  common::DeviceConfig candidate_{};
  config::DeviceCodec device_codec_;
  uint8_t transfer_[7680]{};
  uint32_t transfer_size_ = 0, transfer_expected_ = 0, transfer_crc_ = 0,
           transfer_revision_ = 0;
  void deviceReply(const ConfigHost &host);
  bool dispatchDevice(uint16_t op, uint16_t payload, uint32_t now,
                      ConfigHost &host);
  void resetSession();
  void dispatch(uint32_t now_us, ConfigHost &host);
  void prefix(JsonWriter &writer, bool ok, const ConfigHost &host);
  void finish(JsonWriter &writer);
  void error(const char *code, const char *message, const ConfigHost &host);
  void configReply(const ConfigHost &host);
};
} // namespace protocol
} // namespace fluxrail
