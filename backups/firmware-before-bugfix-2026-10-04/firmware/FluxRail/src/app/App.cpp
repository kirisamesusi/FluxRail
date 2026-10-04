#include "App.h"
#include "../board/BoardConfig.h"
#include "../keyboard/KeyScannerRp2040.h"
#include "../usb/NormalHid.h"
#include <Arduino.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/unique_id.h>

namespace fluxrail {
namespace app {
namespace {
App instance;
} // namespace
App &application() { return instance; }
App::App() {
  config::defaults(device_);
  requested_ = config::activeRuntime(device_);
  input_.setAcquisitionHook(&acquisition_);
  coordinator_.setAcquisitionHook(&acquisition_);
  coordinator_.setAcquisition(common::acquisitionConfig(device_));
}
uint8_t App::prepareUsbBoot(bool force_normal) {
  if (!boot_prepared_) {
    store_.load(device_);
    boot_prepared_ = true;
    forced_normal_ = force_normal;
    requested_ = config::activeRuntime(device_);
  }
  return forced_normal_ ? 0 : device_.startup_usb_mode;
}
void App::beginCore0() {
  if (!boot_prepared_)
    prepareUsbBoot(false);
  requested_ = config::activeRuntime(device_);
  pico_get_unique_board_id_string(serial_, sizeof(serial_));
  coordinator_.setAcquisition(common::acquisitionConfig(device_));
  stats_.usb_startup_failed = !usb::usbBackend().startupOk();
  // 也覆盖第二核始终未启动的情况；此后只有看到新鲜采样心跳才能喂狗。
  watchdog_enable(2000, true);
  watchdog_started_ = true;
  lighting::LedOrder order;
  for (uint8_t i = 0; i < common::kKeyCount; ++i)
    order.chain_to_bt[i] = board::kLedChainToBt[i];
  stats_.led_failed = !leds_.begin(order, board::kLedGrb);
  last_led_us_ = micros();
}
void App::beginCore1() {
  keyboard::beginKeyPinsRp2040();
  last_scan_us_ = micros();
  input_.begin(keyboard::readKeyDownMaskRp2040(), last_scan_us_);
  const bool adc_ok = adc_.begin(), ssi_ok = ssi_.begin();
  acquisition_.hardwareStarted(adc_ok, ssi_ok);
  core1_started_.store(true, std::memory_order_release);
}
void App::serviceCore1() {
  const uint32_t now = micros();
  if (now - last_scan_us_ < board::kScanPeriodUs)
    return;
  last_scan_us_ = now; // 超期不密集补旧扫描，下一次读取当前完整电平。
  input_.service(keyboard::readKeyDownMaskRp2040(), now);
  // 唯一核心1：每轮两SSI都服务；HallSampler自行保持1ms周期，不补旧样本。
  acquisition_.service(micros(), adc_, ssi_);
  acquisition_.recordServiceCost(micros() - now);
}
bool App::applyRuntimeConfig(const common::RuntimeConfig &c, uint32_t now) {
  if (busy() || !ready())
    return false;
  candidate_ = device_;
  config::find(candidate_, candidate_.active_id)->runtime = c;
  candidate_.revision = c.revision;
  return applyDevice(candidate_, now);
}
bool App::applyDevice(const common::DeviceConfig &c, uint32_t now) {
  if (busy() || !ready() || !config::validDevice(c) ||
      c.revision != config::nextRevision(device_.revision))
    return false;
  const auto r = config::activeRuntime(c);
  coordinator_.setAcquisition(common::acquisitionConfig(c));
  if (!coordinator_.start(r, now))
    return false;
  candidate_ = c;
  device_pending_ = true;
  hid_profile_pending_ = true;
  requested_ = r;
  return true;
}
bool App::save(uint32_t now) {
  if (busy() || !ready() || device_pending_ || !store_.available() ||
      !coordinator_.startMaintenance(requested_, now))
    return false;
  usb::usbBackend().clearMotionAndLights();
  save_pending_ = true;
  save_written_ = false;
  save_result_ = config::SaveResult::None;
  return true;
}
bool App::reboot(uint8_t target, uint32_t now) {
  if (busy() || !ready() || device_pending_ || target > 1 ||
      !store_.savedRevision() || store_.savedRevision() != device_.revision ||
      target != store_.savedMode() ||
      (save_result_ != config::SaveResult::None &&
       save_result_ != config::SaveResult::Ok) ||
      !coordinator_.startMaintenance(requested_, now))
    return false;
  usb::usbBackend().clearMotionAndLights();
  reboot_failed_ = false;
  bootloader_pending_ = false;
  reboot_started_us_ = now;
  reboot_phase_ = RebootPhase::Pausing;
  return true;
}
bool App::rebootReady() const {
  return reboot_phase_ == RebootPhase::WaitResponse;
}
void App::rebootResponseComplete(bool delivered, uint32_t now) {
  if (reboot_phase_ == RebootPhase::Idle ||
      reboot_phase_ == RebootPhase::Detached)
    return;
  if (!delivered || reboot_phase_ != RebootPhase::WaitResponse) {
    reboot_failed_ = true;
    reboot_phase_ = RebootPhase::Idle;
    coordinator_.start(requested_, now, true);
    return;
  }
  if (!usb::usbBackend().quiescent())
    return;
  if (usb::usbBackend().detachIfQuiescent()) {
    reboot_phase_ = RebootPhase::Detached;
    detached_us_ = now;
  } else {
    reboot_failed_ = true;
    reboot_phase_ = RebootPhase::Idle;
    coordinator_.start(requested_, now, true);
  }
}
void App::serviceReboot(uint32_t now, bool connected) {
  if (reboot_phase_ == RebootPhase::Idle)
    return;
  if (reboot_phase_ == RebootPhase::Detached) {
    if (now - detached_us_ >= 250000) {
      if (bootloader_pending_)
        reset_usb_boot(0, 0);
      else
        rp2040.reboot();
    }
    return;
  }
  if (!connected || coordinator_.failed() ||
      now - reboot_started_us_ >= 3000000) {
    rebootResponseComplete(false, now);
    return;
  }
  if (reboot_phase_ == RebootPhase::Pausing &&
      coordinator_.maintenanceReady() && usb::usbBackend().quiescent())
    reboot_phase_ = RebootPhase::WaitResponse;
}
bool App::bootloader(uint32_t now) {
  // UF2不要求保存RAM；与身份重启共享真正释放/应答完成/detach门槛。
  if (busy() || !ready() || !coordinator_.startMaintenance(requested_, now))
    return false;
  usb::usbBackend().clearMotionAndLights();
  bootloader_pending_ = true;
  reboot_failed_ = false;
  reboot_started_us_ = now;
  reboot_phase_ = RebootPhase::Pausing;
  return true;
}
bool App::calibrationCommand(const common::CalibrationCommand &c,
                             uint32_t now) {
  if (!c.token || !c.job_id || calibrationPending() || save_pending_ ||
      reboot_phase_ != RebootPhase::Idle || !ready())
    return false;
  if (c.op == common::CalibrationOp::Start) {
    if (busy() || c.job_id == calibration_job_id_ || !c.magnet_removed ||
        !(c.left_mm >= -200 && c.right_mm <= 200 &&
          c.right_mm - c.left_mm >= 1) ||
        !coordinator_.startMaintenance(requested_, now))
      return false;
    calibration_phase_ = CalibrationPhase::Pausing;
  } else {
    if (!calibration_running_ || c.job_id != calibration_job_id_ ||
        !acquisition_.command(c))
      return false;
    calibration_phase_ = CalibrationPhase::WaitReply;
  }
  calibration_command_ = c;
  calibration_ok_ = false;
  calibration_started_us_ = now;
  return true;
}
void App::serviceCalibration(uint32_t now) {
  const auto stage = motion_snapshot_.calibration.stage;
  if (motion_snapshot_.calibration.job_id == calibration_job_id_ &&
      (stage == hall::CalibrationStage::Failed ||
       stage == hall::CalibrationStage::Cancelled))
    calibration_running_ = false;
  if (calibration_phase_ == CalibrationPhase::Pausing &&
      coordinator_.maintenanceReady() &&
      acquisition_.command(calibration_command_))
    calibration_phase_ = CalibrationPhase::WaitReply;
  common::CalibrationReply reply;
  if (calibration_phase_ == CalibrationPhase::WaitReply &&
      acquisition_.takeReply(reply) &&
      reply.token == calibration_command_.token) {
    calibration_ok_ = reply.ok;
    if (calibration_command_.op == common::CalibrationOp::Start) {
      calibration_running_ = reply.ok;
      if (reply.ok)
        calibration_job_id_ = calibration_command_.job_id;
      calibration_phase_ = CalibrationPhase::Restoring;
      if (!coordinator_.finishMaintenance(now)) {
        calibration_ok_ = false;
        calibration_phase_ = CalibrationPhase::Idle;
      }
    } else {
      if (calibration_command_.op == common::CalibrationOp::Cancel)
        calibration_running_ = false;
      if (calibration_command_.op == common::CalibrationOp::Finish && reply.ok)
        calibration_finish_wait_ = true;
      calibration_phase_ = CalibrationPhase::Idle;
    }
  }
  if (calibration_phase_ == CalibrationPhase::Restoring && coordinator_.ready())
    calibration_phase_ = CalibrationPhase::Idle;
  if (calibration_phase_ == CalibrationPhase::Idle &&
      acquisition_.takeResult(calibration_result_)) {
    // 仅已采用的合格候选写入唯一DeviceConfig。先RAM完整ACK，之后显式save才写Flash。
    calibration_running_ = false;
    candidate_ = device_;
    candidate_.calibration = calibration_result_.calibration;
    candidate_.calibration_generation =
        config::nextRevision(device_.calibration_generation);
    candidate_.revision = config::nextRevision(device_.revision);
    if (applyDevice(candidate_, now))
      calibration_commit_pending_ = true;
    else {
      calibration_ok_ = false;
      calibration_finish_wait_ = false;
    }
  }
  if (calibrationPending() && now - calibration_started_us_ >= 3000000) {
    calibration_ok_ = false;
    calibration_finish_wait_ = false;
    if (coordinator_.maintenanceReady())
      coordinator_.finishMaintenance(now);
    calibration_phase_ = CalibrationPhase::Idle;
  }
}
void App::serviceMotion(uint32_t now, usb::UsbBackend &hid, bool streaming) {
  if (!streaming) {
    router_.clear();
    route_epoch_ = 0;
    return;
  }
  if (route_epoch_ != coordinator_.epoch()) {
    const auto *p = config::find(device_, device_.active_id);
    if (!p)
      return;
    router_.configure(p->routing, device_.revision, coordinator_.epoch(),
                      hid.mode() == 1);
    route_epoch_ = coordinator_.epoch();
  }
  // Hall覆盖为最新绝对坐标；旋钮保分时FIFO，队列背压时继续由模块自身缓冲。
  for (uint8_t n = 0; n < 8; ++n) {
    hall::Estimate h;
    if (!acquisition_.takeHall(h))
      break;
    router_.hall(h, now);
  }
  for (uint8_t id = 0; id < 2; ++id) {
    const auto *p = config::find(device_, device_.active_id);
    for (uint8_t n = 0; p && n < 8 && router_.canAccept(p->knobs[id].axis);
         ++n) {
      knobs::Motion k;
      if (!acquisition_.takeKnob(id, k))
        break;
      router_.knob(k, now);
    }
  }
  struct Sink final : MotionSink {
    usb::UsbBackend &usb;
    uint32_t revision, epoch;
    Sink(usb::UsbBackend &u, uint32_t r, uint32_t e)
        : usb(u), revision(r), epoch(e) {}
    bool relative(int16_t x, int16_t y) override { return usb.relative(x, y); }
    bool absolute(float out, uint32_t r, uint32_t e) override {
      return usb.absolute(out, true, r, e, revision, epoch);
    }
  } sink(hid, device_.revision, coordinator_.epoch());
  router_.service(sink);
}
const char *App::maintenancePhase() const {
  if (reboot_phase_ != RebootPhase::Idle)
    return "rebooting";
  if (calibration_phase_ != CalibrationPhase::Idle)
    return "calibration_start";
  if (!save_pending_)
    return "idle";
  if (save_written_)
    return "restoring";
  return coordinator_.maintenanceReady() ? "writing" : "pausing";
}
void App::serviceCore0() {
  const uint32_t entered = micros();
  // 本机端口已提供IRQ+同一mutex保护任务入口；不直接重入tud_task。
  TinyUSB_Device_Task();
  auto &hid = usb::usbBackend();
  bool connected = was_connected_;
  const bool link_observed = hid.connectionState(connected);
  const uint32_t now = micros();
  if (core1_started_.load(std::memory_order_acquire)) {
    for (uint8_t budget = 0; budget < 2; ++budget) {
      common::MotionSnapshot snapshot;
      if (!acquisition_.takeSnapshot(snapshot))
        break;
      motion_snapshot_ = snapshot;
    }
    serviceCalibration(now);
    if (!save_pending_ && reboot_phase_ == RebootPhase::Idle && link_observed &&
        connected != was_connected_) {
      // 断连立即要求核心1停旧流；重连总是新epoch，不补离线动作。
      if (coordinator_.start(requested_, now, true))
        ++stats_.recoveries;
      if (!connected)
        hid.disconnect();
      was_connected_ = connected;
    }
    const uint32_t fault =
        bridge_.producer_fault_epoch.load(std::memory_order_acquire);
    if (!save_pending_ && reboot_phase_ == RebootPhase::Idle &&
        hid.transportFaulted()) {
      if (coordinator_.start(requested_, now, true)) {
        hid.clearTransportFault();
        ++stats_.recoveries;
      }
    } else if (!save_pending_ && reboot_phase_ == RebootPhase::Idle &&
               (((acquisition_.recoveryNeeded() || router_.faulted()) &&
                 !coordinator_.busy()) ||
                (fault != 0 && fault == coordinator_.epoch() &&
                 !coordinator_.busy()) ||
                (connected && !coordinator_.busy() &&
                 (coordinator_.failed() || !coordinator_.ready())))) {
      if (coordinator_.start(requested_, now))
        ++stats_.recoveries;
    }
    if (hid_profile_pending_) {
      const auto &snapshot = device_pending_ ? candidate_ : device_;
      const auto *profile = config::find(snapshot, snapshot.active_id);
      if (profile && hid.setProfile(*profile, requested_.revision))
        hid_profile_pending_ = false;
    }
    if (!hid_profile_pending_)
      coordinator_.service(now, hid);
    if (device_pending_ && coordinator_.ready() &&
        coordinator_.appliedConfig().revision == candidate_.revision) {
      device_ = candidate_;
      device_pending_ = false;
      if (calibration_commit_pending_) {
        calibration_commit_pending_ = false;
        calibration_finish_wait_ = false;
      }
    }
    if (save_pending_) {
      leds_.service(now);
      if (coordinator_.maintenanceReady() && !save_written_ && !leds_.busy() &&
          hid.quiescent()) {
        // faulted只说明DMA超期，不证明DMA已停止。仍busy时不得绕过
        // 维护门槛；沿用Coordinator超时失败/恢复，Flash保持原有效槽。
        save_result_ = store_.save(device_);
        save_written_ = true;
        if (!coordinator_.finishMaintenance(micros()))
          save_result_ = config::SaveResult::VerifyFailed;
      }
      if (save_written_ && coordinator_.ready())
        save_pending_ = false;
      if (coordinator_.failed()) {
        save_pending_ = false;
        save_result_ = config::SaveResult::VerifyFailed;
      }
    }
    if (connected && coordinator_.ready() && !save_pending_ &&
        reboot_phase_ == RebootPhase::Idle) {
      // 至多移交八条边沿；本地mapper背压则保留公共队列头，先服务USB。
      for (uint8_t i = 0; i < 8; ++i) {
        common::InputEdge edge;
        if (!bridge_.edges.tryPeek(edge))
          break;
        if (edge.epoch != coordinator_.epoch()) {
          bridge_.edges.pop();
          ++stats_.stale_edges;
          continue;
        }
        if (hid.usesPhysical()) {
          if (!hid.edge(edge))
            break;
          bridge_.edges.pop();
          continue;
        }
        const auto result =
            mapper_.enqueue(edge.source_id, edge.down, edge.sequence,
                            edge.timestamp_us, edge.revision);
        if (result == keyboard::IngressResult::Backpressure)
          break;
        if (result == keyboard::IngressResult::NeedsResync) {
          coordinator_.start(requested_, now);
          break;
        }
        bridge_.edges.pop();
        if (result != keyboard::IngressResult::Accepted)
          ++stats_.invalid_edges;
      }
      if (!coordinator_.busy() && !hid.usesPhysical())
        mapper_.service(now, hid);
    }
  }
  serviceMotion(now, hid,
                connected && coordinator_.ready() && !save_pending_ &&
                    reboot_phase_ == RebootPhase::Idle &&
                    calibration_phase_ == CalibrationPhase::Idle);
  // CDC is serviced after reliable input/USB work, with fixed per-loop byte
  // budgets.
  if (hid.hasCdc())
    protocol_.service(now, cdc_, *this);
  serviceReboot(now, connected);
  hid.service(now, coordinator_.ready() && !save_pending_ &&
                       reboot_phase_ == RebootPhase::Idle);
  leds_.service(now);
  if (leds_.faulted())
    stats_.led_failed = true;
  if (!save_pending_ && reboot_phase_ == RebootPhase::Idle &&
      !stats_.led_failed && !leds_.faulted() &&
      now - last_led_us_ >= board::kLedPeriodUs) {
    // 正在申请的颜色不能提前表示已经生效；保留上一次确认完成的配置。
    // 尚无成功申请时appliedConfig仍是默认灯配置；也不提前显示首次申请色。
    const auto &c = coordinator_.appliedConfig();
    auto frame = lighting::renderLights(
        c.lighting,
        static_cast<uint16_t>(
            bridge_.physical_mask.load(std::memory_order_acquire)),
        millis(), c.indicator_enabled,
        lighting::modeIndicatorColor(
            c.indicator_color, coordinator_.hasApplied() && hid.mode() == 1));
    hid.applyHostLights(frame, c.lighting);
    if (leds_.trySubmit(frame, now))
      last_led_us_ = now;
  }
  // watchdog只由核心0喂，但必须确认采样核新鲜；网页/USB未连接不等于核心故障。
  const uint32_t heartbeat =
      bridge_.sample_heartbeat_us.load(std::memory_order_acquire);
  if (watchdog_started_ && core1_started_.load(std::memory_order_acquire) &&
      micros() - heartbeat < board::kInputCoreTimeoutUs)
    watchdog_update();
  const uint32_t cost = micros() - entered;
  if (cost > stats_.core0_max_service_us)
    stats_.core0_max_service_us = cost;
}
} // namespace app
} // namespace fluxrail

// 唯一启动入口：先完整load再决定CDC，随后只注册已选HID人格。
extern "C" bool fluxrail_usb_wants_cdc() {
  fluxrail::keyboard::beginKeyPinsRp2040();
  // GPIO pull-up settling uses a bounded core delay; no CDC or core1
  // dependency.
  delayMicroseconds(50);
  const bool force = (fluxrail::keyboard::readKeyDownMaskRp2040() &
                      (1u << fluxrail::board::kUsbRecoverySource)) != 0;
  const uint8_t mode = fluxrail::app::application().prepareUsbBoot(force);
  return mode == 0 || FLUXRAIL_IO4_CONFIG_CDC;
}
extern "C" void fluxrail_usb_configure_before_connect() {
  const uint8_t mode = fluxrail::app::application().prepareUsbBoot(false);
  fluxrail::usb::usbBackend().configureBeforeConnect(mode);
}
