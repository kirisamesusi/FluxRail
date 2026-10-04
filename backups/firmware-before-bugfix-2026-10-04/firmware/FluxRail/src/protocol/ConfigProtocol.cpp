#include "ConfigProtocol.h"
#include "../common/FirmwareInfo.h"
#include "../keyboard/KeyboardTypes.h"
namespace fluxrail {
namespace protocol {
void ConfigProtocol::resetSession() {
  rx_size_ = tx_size_ = tx_sent_ = 0;
  id_ = 0;
  dropping_ = pending_ = pending_save_ = pending_device_ = false;
  pending_reboot_ = reboot_response_ = pending_calibration_ = false;
  transfer_size_ = transfer_expected_ = transfer_crc_ = 0;
  ++stats_.session_resets;
  // Accepted application remains owned by App; losing CDC must not cancel it.
}
void ConfigProtocol::prefix(JsonWriter &w, bool ok, const ConfigHost &h) {
  w.literal("{\"protocol_version\":1,\"id\":");
  if (id_)
    w.number(id_);
  else
    w.literal("null");
  w.literal(",\"ok\":");
  w.boolean(ok);
  w.literal(",\"revision\":");
  w.number(h.appliedConfig().revision);
  w.literal(",\"applied_revision\":");
  if (h.ready())
    w.number(h.appliedConfig().revision);
  else
    w.literal("null");
  w.literal(",\"saved_revision\":");
  if (h.savedRevision())
    w.number(h.savedRevision());
  else
    w.literal("null");
  w.literal(",\"dirty\":");
  w.boolean(!h.savedRevision() ||
            h.savedRevision() != h.appliedConfig().revision);
}
void ConfigProtocol::finish(JsonWriter &w) {
  w.literal("}\n");
  if (w.ok()) {
    tx_size_ = static_cast<uint16_t>(w.size());
    tx_sent_ = 0;
  } else { // Constant error fits even if a future capability exceeds the line
           // budget.
    JsonWriter fallback(tx_, sizeof(tx_));
    fallback.literal("{\"protocol_version\":1,\"id\":");
    if (id_)
      fallback.number(id_);
    else
      fallback.literal("null");
    fallback.literal(",\"ok\":false,\"error\":{\"code\":\"RESPONSE_TOO_LARGE\","
                     "\"message\":\"Response exceeds line limit\"}}\n");
    tx_size_ = static_cast<uint16_t>(fallback.size());
    tx_sent_ = 0;
  }
}
void ConfigProtocol::error(const char *code, const char *message,
                           const ConfigHost &h) {
  ++stats_.rejected;
  JsonWriter w(tx_, kMaxLine);
  prefix(w, false, h);
  w.literal(",\"error\":{\"code\":\"");
  w.literal(code);
  w.literal("\",\"message\":\"");
  w.literal(message);
  w.literal("\"}");
  finish(w);
}
void ConfigProtocol::configReply(const ConfigHost &h) {
  JsonWriter w(tx_, kMaxLine);
  prefix(w, true, h);
  w.literal(",\"result\":{\"scope\":\"runtime_module1\",\"config_ready\":");
  w.boolean(h.ready());
  w.literal(",\"config\":");
  encodeRuntime(w, h.appliedConfig());
  w.literal("}");
  finish(w);
}
void ConfigProtocol::dispatch(uint32_t now, ConfigHost &h) {
  ++stats_.requests;
  id_ = 0;
  if (!json_.parse(rx_, rx_size_)) {
    error("INVALID_JSON", "Malformed JSON or parser limit exceeded", h);
    return;
  }
  // Extract a usable correlation id even when a later envelope field is
  // invalid.
  json_.uintValue(json_.field(0, "id"), id_);
  const char *const envelope[] = {"protocol_version", "id", "op", "payload"};
  const auto p = json_.field(0, "payload"), op = json_.field(0, "op");
  uint32_t version;
  if (!json_.shape(0, envelope, 4) || !id_ ||
      !json_.uintValue(json_.field(0, "protocol_version"), version) ||
      p == Json::kMissing || json_[p].kind != JsonKind::Object ||
      op == Json::kMissing || json_[op].kind != JsonKind::String) {
    error("INVALID_REQUEST", "Expected version id op and object payload", h);
    return;
  }
  if (version != 1) {
    error("PROTOCOL_MISMATCH", "Only protocol version 1 is supported", h);
    return;
  }
  if (json_.text(op, "hello") || json_.text(op, "get_config") ||
      json_.text(op, "get_status")) {
    if (json_.count(p) != 0) {
      error("INVALID_PAYLOAD", "This operation requires empty payload", h);
      return;
    }
    if (json_.text(op, "get_config")) {
      configReply(h);
      return;
    }
    JsonWriter w(tx_, kMaxLine);
    prefix(w, true, h);
    if (json_.text(op, "hello")) {
      const auto &fw = firmware::info();
      w.literal(",\"result\":{\"product\":\"FluxRail\",\"board\":\"FluxRail RP2040 "
                "4MiB\",\"firmware_version\":\"");
      w.literal(fw.version);
      w.literal("\",\"firmware_id\":\"");
      w.literal(fw.magic);
      w.literal("\",\"firmware_board\":\"");
      w.literal(fw.board);
      w.literal("\",\"firmware_metadata_format\":");
      w.number(fw.format);
      w.literal(",\"serial\":\"");
      w.literal(h.serial());
      if (h.deviceConfig()) {
        w.literal("\",\"usb_mode\":\"");
        w.literal(h.usbMode() ? "IO4" : "NORMAL_NKRO");
        w.literal("\",\"scope\":\"device_"
                  "schema2\",\"capabilities\":{\"nkro\":true,\"mouse_buttons\":"
                  "true,\"lighting\":true,\"cdc_config\":true,\"ram_preview\":"
                  "true,\"profiles\":true,\"flash_save\":");
        w.boolean(h.flashAvailable());
        w.literal(",\"global_calibration_storage\":true,\"snapshot_transfer\":"
                  "true,\"hall\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"knobs\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"push_hardware\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"absolute_x\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"motion_telemetry\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"hall_calibration\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"bootloader_command\":");
        w.boolean(h.motionAvailable());
        w.literal(",\"usb_host_verified\":false,\"io4\":true,\"io4_game_"
                  "verified\":false,\"usb_cold_switch\":true,\"max_profiles\":"
                  "12,\"binding_sources\":12,\"max_line_bytes\":4096,\"max_"
                  "tokens\":384,\"max_depth\":12}}");
      } else
        w.literal(
            "\",\"usb_mode\":\"NORMAL_NKRO\",\"scope\":\"runtime_module1\","
            "\"capabilities\":{\"nkro\":true,\"mouse_buttons\":true,"
            "\"lighting\":true,\"cdc_config\":true,\"ram_preview\":true,"
            "\"flash_save\":false,\"profiles\":false,\"hall\":false,"
            "\"knobs\":false,\"push_hardware\":false,\"absolute_x\":false,"
            "\"io4\":false,\"bootloader_command\":false,\"max_profiles\":0,"
            "\"binding_sources\":12,\"physical_bt_inputs\":10,\"max_line_"
            "bytes\":4096,\"max_tokens\":384,\"max_depth\":12}}");
    } else {
      w.literal(",\"result\":{\"config_ready\":");
      w.boolean(h.ready());
      w.literal(",\"applying\":");
      w.boolean(h.busy());
      w.literal(",\"apply_failed\":");
      w.boolean(h.failed());
      w.literal(",\"requested_revision\":");
      w.number(h.requestedRevision());
      w.literal(",\"maintenance\":\"");
      w.literal(h.maintenancePhase());
      w.literal("\",\"save_result\":");
      w.number(uint8_t(h.saveResult()));
      w.literal(",\"valid_slots\":");
      w.number(h.validSlots());
      w.literal(",\"physical_mask\":");
      w.number(h.physicalMask());
      w.literal(",\"protocol_requests\":");
      w.number(stats_.requests);
      w.literal(",\"protocol_rejected\":");
      w.number(stats_.rejected);
      w.literal(",\"overlong_lines\":");
      w.number(stats_.overlong_lines);
      w.literal("}");
    }
    finish(w);
    return;
  }
  if (dispatchMotion(op, p, now, h))
    return;
  if (h.deviceConfig() && dispatchDevice(op, p, now, h))
    return;
  if (!json_.text(op, "set_config") && !json_.text(op, "restore_defaults")) {
    error("UNSUPPORTED_OPERATION",
          "Operation is not implemented in this prototype", h);
    return;
  }
  const bool restore = json_.text(op, "restore_defaults");
  const char *const setkeys[] = {"expected_revision", "config"};
  const char *const restorekeys[] = {"expected_revision"};
  uint32_t expected;
  if (!json_.shape(p, restore ? restorekeys : setkeys, restore ? 1 : 2) ||
      !json_.uintValue(json_.field(p, "expected_revision"), expected)) {
    error("INVALID_PAYLOAD", "Expected revision and complete runtime config",
          h);
    return;
  }
  if (!h.ready() || h.busy()) {
    error("BUSY", "Read status and retry after recovery completes", h);
    return;
  }
  if (expected != h.appliedConfig().revision) {
    error("REVISION_CONFLICT", "Read current config before editing", h);
    return;
  }
  common::RuntimeConfig c = h.appliedConfig();
  if (restore) {
    c = common::RuntimeConfig{};
    c.keyboard = keyboard::defaultKeyboardConfig();
  } else if (!decodeRuntime(json_, json_.field(p, "config"), c)) {
    error("INVALID_CONFIG",
          "Incomplete unknown duplicate or out of range config field", h);
    return;
  }
  c.revision = expected + 1;
  if (!c.revision)
    c.revision = 1;
  if (!h.apply(c, now)) {
    error("BUSY", "Application did not accept the configuration", h);
    return;
  }
  pending_device_ = false;
  pending_ = true;
  pending_revision_ = c.revision;
  apply_started_us_ = now;
  // No success response until both cores and release/rebuild have completed.
}
void ConfigProtocol::motionReply(const ConfigHost &h, bool calibration_only) {
  JsonWriter w(tx_, kMaxLine);
  prefix(w, true, h);
  const auto *s = h.motionSnapshot();
  w.literal(",\"result\":{\"sample_ready\":");
  w.boolean(s && s->revision != 0);
  if (s) {
    w.literal(",\"timestamp_us\":");
    w.number(s->timestamp_us);
    w.literal(",\"sample_revision\":");
    w.number(s->revision);
    w.literal(",\"epoch\":");
    w.number(s->epoch);
    w.literal(",\"calibration\":{\"job_id\":");
    w.number(s->calibration.job_id);
    w.literal(",\"stage\":");
    w.number(uint8_t(s->calibration.stage));
    w.literal(",\"error\":");
    w.number(uint8_t(s->calibration.error));
    w.literal(",\"collected\":");
    w.number(s->calibration.collected);
    w.literal(",\"required\":");
    w.number(s->calibration.required);
    w.literal(",\"next_reference\":");
    w.number(s->calibration.next_reference);
    w.literal(",\"nodes\":");
    w.number(s->calibration.nodes);
    w.literal(",\"sweep_segments\":");
    w.number(s->calibration.sweep_segments);
    w.literal(",\"sweep_left\":");
    w.boolean(s->calibration.sweep_left);
    w.literal(",\"sweep_right\":");
    w.boolean(s->calibration.sweep_right);
    w.literal("}");
    if (!calibration_only) {
      w.literal(",\"adc_started\":");
      w.boolean(s->adc_started);
      w.literal(",\"ssi_started\":");
      w.boolean(s->ssi_started);
      w.literal(",\"raw_adc\":[");
      for (uint8_t i = 0; i < 3; ++i) {
        if (i)
          w.literal(",");
        w.number(s->raw.raw[i]);
      }
      w.literal("]");
      w.literal(",\"hall\":{\"valid\":");
      w.boolean(s->hall.valid);
      w.literal(",\"active\":");
      w.boolean(s->hall.active);
      w.literal(",\"quality\":");
      w.number(uint8_t(s->hall.quality));
      w.literal(",\"mm\":");
      w.real(s->hall.mm);
      w.literal(",\"p\":");
      w.real(s->hall.p);
      w.literal(",\"output\":");
      w.real(s->hall.output);
      w.literal(",\"samples\":");
      w.number(s->hall_stats.samples);
      w.literal(",\"queue_full\":");
      w.number(s->hall_stats.queue_full);
      w.literal(",\"adc_errors\":");
      w.number(s->adc_stats.errors);
      w.literal("},\"knobs\":[");
      for (uint8_t i = 0; i < 2; ++i) {
        if (i)
          w.literal(",");
        const auto &k = s->knobs[i];
        const auto &p = s->push[i];
        w.literal("{\"knob_id\":");
        w.number(i);
        w.literal(",\"valid\":");
        w.boolean(k.valid);
        w.literal(",\"active\":");
        w.boolean(k.active);
        w.literal(",\"enabled\":");
        w.boolean(k.enabled);
        w.literal(",\"axis\":");
        w.number(uint8_t(k.axis));
        w.literal(",\"raw_angle\":");
        w.number(k.raw_angle);
        w.literal(",\"raw_turns\":");
        w.real(k.raw_turns);
        w.literal(",\"scaled_turns\":");
        w.real(k.scaled_turns);
        w.literal(",\"quality\":");
        w.number(uint8_t(k.quality));
        w.literal(",\"crc_errors\":");
        w.number(s->knob_stats[i].crc_errors);
        w.literal(",\"transport_errors\":");
        w.number(s->knob_stats[i].transport_errors);
        w.literal(",\"raw_push\":");
        w.boolean(p.raw_gpio);
        w.literal(",\"ssi_push\":");
        w.boolean(p.ssi_push);
        w.literal(",\"filtered_push\":");
        w.boolean(p.filtered_down);
        w.literal("}");
      }
      w.literal("],\"core1_max_service_us\":");
      w.number(s->core1_max_service_us);
      w.literal(",\"telemetry_dropped\":");
      w.number(s->telemetry_dropped);
    }
  }
  w.literal(",\"running\":");
  w.boolean(h.calibrating());
  w.literal(",\"adopting\":");
  w.boolean(h.calibrationPending());
  if (h.deviceConfig()) {
    w.literal(",\"calibration_generation\":");
    w.number(h.deviceConfig()->calibration_generation);
    w.literal(",\"calibration_valid\":");
    w.boolean(h.deviceConfig()->calibration.valid);
  }
  w.literal("}");
  finish(w);
}
bool ConfigProtocol::dispatchMotion(uint16_t op, uint16_t p, uint32_t now,
                                    ConfigHost &h) {
  const bool motion = json_.text(op, "get_motion"),
             status = json_.text(op, "get_calibration"),
             boot = json_.text(op, "enter_bootloader");
  const bool start = json_.text(op, "calibration_start"),
             ref = json_.text(op, "calibration_reference"),
             node = json_.text(op, "calibration_node"),
             sweep = json_.text(op, "calibration_begin_sweep"),
             finish_op = json_.text(op, "calibration_finish"),
             cancel = json_.text(op, "calibration_cancel");
  if (!(motion || status || boot || start || ref || node || sweep ||
        finish_op || cancel))
    return false;
  if (!h.motionAvailable()) {
    error("UNSUPPORTED_OPERATION", "Runtime motion integration unavailable", h);
    return true;
  }
  if (motion || status) {
    if (json_.count(p))
      error("INVALID_PAYLOAD", "Expected empty payload", h);
    else
      motionReply(h, status);
    return true;
  }
  uint32_t expected = 0;
  if (!json_.uintValue(json_.field(p, "expected_revision"), expected) ||
      expected != h.appliedConfig().revision) {
    error("REVISION_CONFLICT", "Read current revision", h);
    return true;
  }
  if (boot) {
    const char *const keys[] = {"expected_revision"};
    if (!json_.shape(p, keys, 1) || !h.bootloader(now)) {
      error("BUSY", "Bootloader maintenance rejected", h);
      return true;
    }
    pending_reboot_ = true;
    apply_started_us_ = now;
    return true;
  }
  const char *const sk[] = {"expected_revision", "job_id", "magnet_removed",
                            "left_mm", "right_mm"};
  const char *const rk[] = {"expected_revision", "job_id", "channel"};
  const char *const nk[] = {"expected_revision", "job_id", "mm"};
  const char *const jk[] = {"expected_revision", "job_id"};
  common::CalibrationCommand c;
  c.token = id_;
  bool ok = json_.shape(p,
                        start  ? sk
                        : ref  ? rk
                        : node ? nk
                               : jk,
                        start           ? 5
                        : (ref || node) ? 3
                                        : 2) &&
            json_.uintValue(json_.field(p, "job_id"), c.job_id) && c.job_id;
  uint32_t channel = 0;
  if (start) {
    c.op = common::CalibrationOp::Start;
    ok = ok &&
         json_.boolValue(json_.field(p, "magnet_removed"), c.magnet_removed) &&
         json_.floatValue(json_.field(p, "left_mm"), c.left_mm) &&
         json_.floatValue(json_.field(p, "right_mm"), c.right_mm);
  } else if (ref) {
    c.op = common::CalibrationOp::Reference;
    ok = ok && json_.uintValue(json_.field(p, "channel"), channel) &&
         channel < 3;
    c.channel = uint8_t(channel);
  } else if (node) {
    c.op = common::CalibrationOp::Node;
    ok = ok && json_.floatValue(json_.field(p, "mm"), c.mm);
  } else
    c.op = sweep       ? common::CalibrationOp::BeginSweep
           : finish_op ? common::CalibrationOp::Finish
                       : common::CalibrationOp::Cancel;
  if (!ok) {
    error("INVALID_PAYLOAD", "Invalid calibration command fields", h);
    return true;
  }
  if (!h.calibrationCommand(c, now)) {
    error("BUSY", "Calibration command not accepted", h);
    return true;
  }
  pending_calibration_ = true;
  apply_started_us_ = now;
  return true;
}
void ConfigProtocol::service(uint32_t now, CdcPort &port, ConfigHost &h) {
  bool online = online_;
  if (!port.connectionState(online))
    return;
  if (online != online_) {
    online_ = online;
    if (pending_reboot_ || reboot_response_)
      h.rebootResponseComplete(false, now);
    resetSession();
  }
  if (!online_)
    return;
  if (tx_size_) {
    const uint32_t remaining = tx_size_ - tx_sent_,
                   budget = remaining < 64 ? remaining : 64;
    const uint32_t sent = port.write(tx_ + tx_sent_, budget);
    if (sent <= budget)
      tx_sent_ += static_cast<uint16_t>(sent);
    if (tx_sent_ == tx_size_)
      tx_size_ = tx_sent_ = 0;
    return; // Backpressure never executes the next buffered request.
  }
  if (reboot_response_) {
    if (h.rebootFailed() || now - apply_started_us_ >= 3000000) {
      h.rebootResponseComplete(false, now);
      reboot_response_ = false;
    } else if (port.txComplete()) {
      h.rebootResponseComplete(true, now);
      // Keep input locked until detach; App owns the remaining bounded delay.
    }
    return;
  }
  if (pending_reboot_) {
    if (h.rebootFailed()) {
      pending_reboot_ = false;
      error("REBOOT_FAILED", "Read status before retrying", h);
    } else if (h.rebootReady()) {
      pending_reboot_ = false;
      modeReply(h);
      reboot_response_ = true;
    } else if (now - apply_started_us_ >= 3000000) {
      pending_reboot_ = false;
      h.rebootResponseComplete(false, now);
      error("REBOOT_TIMEOUT", "No switch read status before retrying", h);
    }
    return;
  }
  if (pending_calibration_) {
    if (!h.calibrationPending()) {
      pending_calibration_ = false;
      if (h.calibrationSucceeded())
        motionReply(h, true);
      else
        error("CALIBRATION_REJECTED",
              "Stage or parameters rejected query calibration", h);
    } else if (now - apply_started_us_ >= kApplyTimeoutUs) {
      pending_calibration_ = false;
      error("CALIBRATION_TIMEOUT",
            "Outcome unknown query calibration before retry", h);
    }
    return;
  }
  if (pending_save_) {
    if (!h.saving()) {
      pending_save_ = false;
      if (h.saveResult() == config::SaveResult::Ok && h.ready())
        deviceReply(h);
      else
        error("SAVE_FAILED",
              "Storage or input recovery failed read status before retrying",
              h);
    } else if (now - apply_started_us_ >= 10000000) {
      pending_save_ = false;
      error("SAVE_TIMEOUT", "Outcome unknown query status before retrying", h);
    }
    return;
  }
  if (pending_) {
    if (h.ready()) {
      pending_ = false;
      if (h.appliedConfig().revision == pending_revision_) {
        if (pending_device_)
          deviceReply(h);
        else
          configReply(h);
        pending_device_ = false;
      } else
        error("APPLY_SUPERSEDED", "Read back the confirmed configuration", h);
    } else if (h.failed()) {
      pending_ = false;
      error("APPLY_FAILED", "Read status before retrying", h);
    } else if (now - apply_started_us_ >= kApplyTimeoutUs) {
      pending_ = false;
      ++stats_.apply_timeouts;
      error("APPLY_TIMEOUT",
            "Outcome unknown read status and config before retrying", h);
    }
    return;
  }
  for (uint16_t budget = 0; budget < 128; ++budget) {
    const int32_t input = port.readByte();
    if (input < 0)
      return;
    const char c = static_cast<char>(input);
    if (c == '\n') {
      if (dropping_) {
        dropping_ = false;
        rx_size_ = 0;
        id_ = 0;
        error("LINE_TOO_LONG", "Discarded line exceeds 4096 bytes", h);
        return;
      }
      if (!rx_size_)
        continue; // Empty keepalive lines are harmless.
      dispatch(now, h);
      rx_size_ = 0;
      return;
    }
    if (dropping_)
      continue;
    if (rx_size_ == kMaxLine) {
      dropping_ = true;
      ++stats_.overlong_lines;
      rx_size_ = 0;
    } else
      rx_[rx_size_++] = c;
  }
}
} // namespace protocol
} // namespace fluxrail
