#pragma once
#include "../config/FlashRp2040.h"
#include "../hall/HallAdcRp2040.h"
#include "../knobs/Mt6701SsiRp2040.h"
#include "../lighting/Ws2812OutputRp2040.h"
#include "../protocol/ConfigProtocol.h"
#include "../realtime/InputWorker.h"
#include "../usb/CdcTransport.h"
#include "../usb/UsbBackend.h"
#include "Acquisition.h"
#include "ConfigCoordinator.h"
#include "MotionRouter.h"
#include <atomic>

namespace fluxrail {
namespace app {
struct AppStats {
  uint32_t stale_edges = 0, invalid_edges = 0, core0_max_service_us = 0,
           recoveries = 0;
  bool led_failed = false, usb_startup_failed = false;
};
class App final : public protocol::ConfigHost {
public:
  App();
  // 库首次连接前调用一次；完整只读load复用此App内静态工作区。
  uint8_t prepareUsbBoot(bool force_normal);
  void beginCore0();
  void serviceCore0();
  void beginCore1();
  void serviceCore1();
  // 仅核心1：模块3过滤后的P1/P2边沿，从相同公共生产者赋连续序号。
  // 必须在beginCore1后调用；本层不采样PUSH GPIO/不实现旋转防误触。
  bool submitFilteredPush(uint8_t source_id, bool down, uint32_t now_us) {
    return input_.submitPush(source_id, down, now_us);
  }
  // 仅核心0：true表示接受异步申请；必须等appliedRevision到目标值才算完成。
  bool applyRuntimeConfig(const common::RuntimeConfig &config, uint32_t now_us);
  uint32_t appliedRevision() const {
    return coordinator_.ready() ? coordinator_.appliedConfig().revision : 0;
  }
  const AppStats &stats() const { return stats_; }
  const common::RuntimeConfig &appliedConfig() const override {
    return coordinator_.appliedConfig();
  }
  uint32_t requestedRevision() const override { return requested_.revision; }
  bool ready() const override { return coordinator_.ready(); }
  bool busy() const override {
    return coordinator_.busy() || save_pending_ ||
           reboot_phase_ != RebootPhase::Idle || calibration_running_ ||
           calibration_phase_ != CalibrationPhase::Idle ||
           calibration_commit_pending_;
  }
  bool failed() const override { return coordinator_.failed(); }
  uint16_t physicalMask() const override {
    return static_cast<uint16_t>(
        bridge_.physical_mask.load(std::memory_order_acquire));
  }
  const char *serial() const override { return serial_; }
  bool apply(const common::RuntimeConfig &config, uint32_t now_us) override {
    return applyRuntimeConfig(config, now_us);
  }
  const common::DeviceConfig *deviceConfig() const override { return &device_; }
  bool applyDevice(const common::DeviceConfig &config,
                   uint32_t now_us) override;
  uint32_t savedRevision() const override { return store_.savedRevision(); }
  bool flashAvailable() const override { return store_.available(); }
  uint8_t validSlots() const override { return store_.validSlots(); }
  bool save(uint32_t now_us) override;
  bool saving() const override { return save_pending_; }
  config::SaveResult saveResult() const override { return save_result_; }
  const char *maintenancePhase() const override;
  uint8_t usbMode() const override { return usb::usbBackend().mode(); }
  uint8_t savedUsbMode() const override { return store_.savedMode(); }
  bool forcedRecovery() const override { return forced_normal_; }
  bool reboot(uint8_t target, uint32_t now) override;
  bool rebootReady() const override;
  bool rebootFailed() const override { return reboot_failed_; }
  void rebootResponseComplete(bool delivered, uint32_t now) override;
  const common::MotionSnapshot *motionSnapshot() const override {
    return &motion_snapshot_;
  }
  bool motionAvailable() const override { return true; }
  bool calibrating() const override { return calibration_running_; }
  bool calibrationCommand(const common::CalibrationCommand &,
                          uint32_t) override;
  bool calibrationPending() const override {
    return calibration_phase_ != CalibrationPhase::Idle ||
           calibration_commit_pending_ || calibration_finish_wait_;
  }
  bool calibrationSucceeded() const override { return calibration_ok_; }
  bool bootloader(uint32_t now) override;
  const RoutingStats &routingStats() const { return router_.stats(); }
  const protocol::ProtocolStats &protocolStats() const {
    return protocol_.stats();
  }

private:
  realtime::RealtimeBridge bridge_;
  keyboard::ActionMapper mapper_;
  realtime::InputWorker input_{bridge_};
  ConfigCoordinator coordinator_{bridge_, mapper_};
  Acquisition acquisition_{input_};
  MotionRouter router_;
  hall::HallAdcRp2040 adc_;
  knobs::Mt6701SsiRp2040 ssi_;
  common::MotionSnapshot motion_snapshot_{};
  enum class CalibrationPhase : uint8_t { Idle, Pausing, WaitReply, Restoring };
  CalibrationPhase calibration_phase_ = CalibrationPhase::Idle;
  common::CalibrationCommand calibration_command_{};
  common::CalibrationResult calibration_result_{};
  bool calibration_running_ = false, calibration_ok_ = false,
       calibration_commit_pending_ = false, calibration_finish_wait_ = false;
  uint32_t calibration_started_us_ = 0, route_epoch_ = 0,
           calibration_job_id_ = 0;
  void serviceCalibration(uint32_t now);
  void serviceMotion(uint32_t now, usb::UsbBackend &hid, bool streaming);
  lighting::Ws2812OutputRp2040 leds_;
  common::RuntimeConfig requested_{};
  common::DeviceConfig device_{}, candidate_{};
  config::FlashRp2040 flash_;
  config::ConfigStore store_{flash_};
  enum class RebootPhase : uint8_t { Idle, Pausing, WaitResponse, Detached };
  RebootPhase reboot_phase_ = RebootPhase::Idle;
  bool reboot_failed_ = false, boot_prepared_ = false, forced_normal_ = false,
       hid_profile_pending_ = true, bootloader_pending_ = false;
  uint32_t reboot_started_us_ = 0, detached_us_ = 0;
  void serviceReboot(uint32_t now, bool connected);
  bool device_pending_ = false, save_pending_ = false, save_written_ = false;
  config::SaveResult save_result_ = config::SaveResult::None;
  std::atomic<bool> core1_started_{false};
  uint32_t last_scan_us_ = 0, last_led_us_ = 0;
  bool was_connected_ = false, watchdog_started_ = false;
  AppStats stats_{};
  protocol::ConfigProtocol protocol_;
  usb::CdcTransport cdc_;
  char serial_[17]{};
};
App &application();
} // namespace app
} // namespace fluxrail
