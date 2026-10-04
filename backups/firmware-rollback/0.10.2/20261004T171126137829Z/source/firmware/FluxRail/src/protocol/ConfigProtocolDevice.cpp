#include "ConfigProtocol.h"
#include "ProfileCodec.h"
namespace fluxrail {
namespace protocol {
void ConfigProtocol::deviceReply(const ConfigHost &h) {
  JsonWriter w(tx_, kMaxLine);
  prefix(w, true, h);
  const auto &c = *h.deviceConfig();
  w.literal(",\"result\":{\"schema\":");
  w.number(c.schema);
  w.literal(",\"active_profile\":");
  w.number(c.active_id);
  w.literal(",\"profile_count\":");
  w.number(c.profile_count);
  w.literal(",\"next_profile_id\":");
  w.number(c.next_id);
  w.literal(",\"startup_usb_mode\":\"");
  w.literal(c.startup_usb_mode ? "IO4" : "NORMAL_NKRO");
  w.literal("\",\"calibration_valid\":");
  w.boolean(c.calibration.valid);
  w.literal(",\"calibration_generation\":");
  w.number(c.calibration_generation);
  w.literal("}");
  finish(w);
}
void ConfigProtocol::modeReply(const ConfigHost &h) {
  JsonWriter w(tx_, kMaxLine);
  prefix(w, true, h);
  w.literal(",\"result\":{\"current\":\"");
  w.literal(h.usbMode() ? "IO4" : "NORMAL_NKRO");
  w.literal("\",\"requested_startup\":\"");
  w.literal(h.deviceConfig()->startup_usb_mode ? "IO4" : "NORMAL_NKRO");
  w.literal("\",\"saved_startup\":");
  if (h.savedRevision()) {
    w.literal("\"");
    w.literal(h.savedUsbMode() ? "IO4" : "NORMAL_NKRO");
    w.literal("\"");
  } else
    w.literal("null");
  w.literal(",\"forced_recovery\":");
  w.boolean(h.forcedRecovery());
  w.literal(",\"reboot_ready\":");
  w.boolean(h.rebootReady());
  w.literal("}");
  finish(w);
}
bool ConfigProtocol::dispatchDevice(uint16_t op, uint16_t p, uint32_t now,
                                    ConfigHost &h) {
  const auto &current = *h.deviceConfig();
  const bool mode_get = json_.text(op, "get_usb_mode"),
             mode_set = json_.text(op, "set_startup_usb_mode"),
             reboot = json_.text(op, "reboot_to_saved_mode"),
             bindings = json_.text(op, "set_io4_bindings");
  if (mode_get) {
    if (json_.count(p) != 0)
      error("INVALID_PAYLOAD", "Expected empty payload", h);
    else
      modeReply(h);
    return true;
  }
  if (mode_set || reboot || bindings) {
    if (!h.ready() || h.busy()) {
      error("BUSY", "Wait for confirmed snapshot", h);
      return true;
    }
    uint32_t expected = 0;
    if (!json_.uintValue(json_.field(p, "expected_revision"), expected) ||
        expected != current.revision) {
      error("REVISION_CONFLICT", "Read current revision", h);
      return true;
    }
    const char *const mk[] = {"expected_revision", reboot ? "target" : "mode"};
    const char *const bk[] = {"expected_revision", "profile_id", "bindings"};
    if (!json_.shape(p, bindings ? bk : mk, bindings ? 3 : 2)) {
      error("INVALID_PAYLOAD", "Unexpected fields", h);
      return true;
    }
    uint8_t target = 0;
    if (!bindings) {
      auto token = json_.field(p, reboot ? "target" : "mode");
      if (json_.text(token, "IO4"))
        target = 1;
      else if (!json_.text(token, "NORMAL_NKRO")) {
        error("INVALID_PAYLOAD", "Unknown USB mode", h);
        return true;
      }
    }
    if (reboot) {
      if (!h.savedRevision() || h.savedRevision() != current.revision ||
          target != h.savedUsbMode()) {
        error("MODE_NOT_SAVED", "Explicit save and readback required", h);
        return true;
      }
      if (!h.reboot(target, now)) {
        error("REBOOT_REJECTED", "Maintenance or save recovery failed", h);
        return true;
      }
      pending_reboot_ = true;
      apply_started_us_ = now;
      return true;
    }
    candidate_ = current;
    if (mode_set)
      candidate_.startup_usb_mode = target;
    else {
      uint32_t id = 0;
      auto a = json_.field(p, "bindings");
      auto *pr = json_.uintValue(json_.field(p, "profile_id"), id)
                     ? config::find(candidate_, id)
                     : nullptr;
      if (!pr || a == Json::kMissing || json_[a].kind != JsonKind::Array ||
          json_.count(a) != 12) {
        error("INVALID_PAYLOAD", "Expected twelve function IDs", h);
        return true;
      }
      for (uint16_t i = 0, t = a + 1; i < 12; ++i, t = json_[t].next) {
        uint32_t f = 0;
        if (!json_.uintValue(t, f) || f > 12) {
          error("INVALID_PAYLOAD", "Function ID outside 0..12", h);
          return true;
        }
        pr->io4_bindings[i] = common::Io4Function(f);
      }
    }
    candidate_.revision = config::nextRevision(current.revision);
    if (!h.applyDevice(candidate_, now)) {
      error("BUSY", "Snapshot not accepted", h);
      return true;
    }
    pending_ = pending_device_ = true;
    pending_revision_ = candidate_.revision;
    apply_started_us_ = now;
    return true;
  }
  const bool list = json_.text(op, "list_profiles"),
             get = json_.text(op, "get_profile"),
             meta = json_.text(op, "get_device"),
             exp = json_.text(op, "export_config");
  if (list || get || meta || exp) {
    uint32_t id = 0, offset = 0;
    const char *const idkeys[] = {"profile_id"};
    const char *const offkeys[] = {"offset"};
    if ((list || meta) && json_.count(p) != 0) {
      error("INVALID_PAYLOAD", "Expected empty payload", h);
      return true;
    }
    if (get && (!json_.shape(p, idkeys, 1) ||
                !json_.uintValue(json_.field(p, "profile_id"), id) ||
                !config::find(current, id))) {
      error("INVALID_PROFILE", "Profile does not exist", h);
      return true;
    }
    if (exp && (!json_.shape(p, offkeys, 1) ||
                !json_.uintValue(json_.field(p, "offset"), offset))) {
      error("INVALID_PAYLOAD", "Expected export offset", h);
      return true;
    }
    if (meta) {
      deviceReply(h);
      return true;
    }
    JsonWriter w(tx_, kMaxLine);
    prefix(w, true, h);
    w.literal(",\"result\":{");
    if (list) {
      w.literal("\"active_profile\":");
      w.number(current.active_id);
      w.literal(",\"max_profiles\":12,\"profiles\":[");
      for (uint8_t i = 0; i < current.profile_count; ++i) {
        if (i)
          w.literal(",");
        const auto &pr = current.profiles[i];
        w.literal("{\"profile_id\":");
        w.number(pr.id);
        w.literal(",\"name\":\"");
        w.literal(pr.name);
        w.literal("\",\"preset\":");
        w.boolean(pr.id <= 4);
        w.literal("}");
      }
      w.literal("]");
    } else if (get) {
      const auto &pr = *config::find(current, id);
      w.literal("\"profile_id\":");
      w.number(id);
      w.literal(",\"name\":\"");
      w.literal(pr.name);
      w.literal("\",\"config\":");
      encodeRuntime(w, pr.runtime);
      w.literal(",\"motion\":");
      encodeMotion(w, pr);
      w.literal(",\"io4_bindings\":[");
      for (uint8_t i = 0; i < 12; ++i) {
        if (i)
          w.literal(",");
        w.number(uint8_t(pr.io4_bindings[i]));
      }
      w.literal("]");
    } else {
      const auto n =
          config::DeviceCodec::encode(current, transfer_, sizeof(transfer_));
      if (!n || offset > n) {
        error("INVALID_PAYLOAD", "Offset outside snapshot", h);
        return true;
      }
      // export invalidates an unfinished import; no cross-request mixed buffer.
      transfer_size_ = transfer_expected_ = 0;
      w.literal("\"schema\":2,\"encoding\":\"hex-le-schema2\",\"length\":");
      w.number(n);
      w.literal(",\"crc32\":");
      w.number(config::crc32(transfer_, n));
      w.literal(",\"offset\":");
      w.number(offset);
      w.literal(",\"data\":\"");
      const uint32_t end = offset + 512 < n ? offset + 512 : uint32_t(n);
      const char hex[] = "0123456789abcdef";
      for (uint32_t i = offset; i < end; ++i) {
        char s[3] = {hex[transfer_[i] >> 4], hex[transfer_[i] & 15], 0};
        w.literal(s);
      }
      w.literal("\",\"next_offset\":");
      w.number(end);
    }
    w.literal("}");
    finish(w);
    return true;
  }
  const bool save = json_.text(op, "save_config"),
             create = json_.text(op, "create_profile") ||
                      json_.text(op, "copy_profile"),
             rename = json_.text(op, "rename_profile"),
             del = json_.text(op, "delete_profile"),
             active = json_.text(op, "set_active_profile"),
             restore = json_.text(op, "restore_preset"),
             set = json_.text(op, "set_profile"),
             begin = json_.text(op, "begin_import"),
             chunk = json_.text(op, "import_chunk"),
             commit = json_.text(op, "commit_import");
  if (!(save || create || rename || del || active || restore || set || begin ||
        chunk || commit))
    return false;
  if (!h.ready() || h.busy()) {
    error("BUSY", "Wait for application or maintenance", h);
    return true;
  }
  uint32_t expected = 0;
  if (!json_.uintValue(json_.field(p, "expected_revision"), expected) ||
      expected != current.revision) {
    error("REVISION_CONFLICT", "Expected current revision", h);
    return true;
  }
  const char *const ek[] = {"expected_revision"};
  const char *const ik[] = {"expected_revision", "profile_id"};
  const char *const nk[] = {"expected_revision", "profile_id", "name"};
  const char *const sk[] = {"expected_revision", "profile_id", "config",
                            "motion"};
  const char *const bk[] = {"expected_revision", "length", "crc32"};
  const char *const ck[] = {"expected_revision", "offset", "data"};
  const char *const *keys = (save || commit)     ? ek
                            : begin              ? bk
                            : chunk              ? ck
                            : (create || rename) ? nk
                            : set                ? sk
                                                 : ik;
  const uint16_t count = (save || commit)                       ? 1
                         : set                                  ? 4
                         : (create || rename || begin || chunk) ? 3
                                                                : 2;
  if (!json_.shape(p, keys, count)) {
    error("INVALID_PAYLOAD", "Unexpected missing or duplicate field", h);
    return true;
  }
  if (save) {
    if (!h.flashAvailable()) {
      error("STORAGE_UNAVAILABLE", "Use fixed reserved Flash build", h);
      return true;
    }
    if (!h.save(now)) {
      error("BUSY", "Maintenance not accepted", h);
      return true;
    }
    pending_save_ = true;
    apply_started_us_ = now;
    return true;
  }
  if (begin) {
    uint32_t len, crc;
    if (!json_.uintValue(json_.field(p, "length"), len) || len == 0 ||
        len > sizeof(transfer_) ||
        !json_.uintValue(json_.field(p, "crc32"), crc)) {
      error("INVALID_PAYLOAD", "Snapshot length or crc32 invalid", h);
      return true;
    }
    transfer_size_ = 0;
    transfer_revision_ = current.revision;
    transfer_expected_ = len;
    transfer_crc_ = crc;
    deviceReply(h);
    return true;
  }
  if (chunk) {
    uint32_t offset;
    char hex[1025];
    if (!transfer_expected_ || transfer_revision_ != current.revision ||
        !json_.uintValue(json_.field(p, "offset"), offset) ||
        offset != transfer_size_ ||
        !json_.stringValue(json_.field(p, "data"), hex, sizeof(hex))) {
      error("INVALID_TRANSFER", "Expected sequential hex chunk", h);
      return true;
    }
    uint32_t len = 0;
    while (hex[len])
      ++len;
    if (!len || len % 2 || len / 2 > transfer_expected_ - transfer_size_) {
      error("INVALID_TRANSFER", "Chunk length invalid", h);
      return true;
    }
    for (uint32_t i = 0; i < len; ++i)
      if (!((hex[i] >= '0' && hex[i] <= '9') ||
            (hex[i] >= 'a' && hex[i] <= 'f'))) {
        error("INVALID_TRANSFER", "Only lowercase hex supported", h);
        return true;
      }
    for (uint32_t i = 0; i < len; i += 2) {
      auto digit = [](char x) -> uint8_t {
        return x <= '9' ? uint8_t(x - '0') : uint8_t(x - 'a' + 10);
      };
      transfer_[transfer_size_++] =
          uint8_t(digit(hex[i]) * 16 + digit(hex[i + 1]));
    }
    deviceReply(h);
    return true;
  }
  candidate_ = current;
  if (commit) {
    if (!transfer_expected_ || transfer_revision_ != current.revision ||
        transfer_size_ != transfer_expected_ ||
        config::crc32(transfer_, transfer_size_) != transfer_crc_ ||
        !device_codec_.decode(transfer_, transfer_size_, candidate_)) {
      error("INVALID_TRANSFER", "Incomplete invalid or corrupt snapshot", h);
      return true;
    }
    transfer_size_ = transfer_expected_ = 0;
    if (candidate_.next_id < current.next_id)
      candidate_.next_id = current.next_id;
    candidate_.calibration_generation =
        config::nextRevision(current.calibration_generation);
  } else {
    uint32_t id = 0;
    char name[32]{};
    if (!json_.uintValue(json_.field(p, "profile_id"), id)) {
      error("INVALID_PROFILE", "Expected profile_id", h);
      return true;
    }
    bool ok = false;
    if (create || rename) {
      if (json_.stringValue(json_.field(p, "name"), name, sizeof(name)))
        ok = create ? config::createProfile(candidate_, id, name)
                    : config::renameProfile(candidate_, id, name);
    } else if (del)
      ok = config::deleteProfile(candidate_, id);
    else if (active) {
      ok = config::find(candidate_, id) != nullptr;
      if (ok)
        candidate_.active_id = id;
    } else if (restore)
      ok = config::restorePreset(candidate_, id);
    else if (set) {
      auto pr = config::find(candidate_, id);
      ok = pr && decodeRuntime(json_, json_.field(p, "config"), pr->runtime) &&
           decodeMotion(json_, json_.field(p, "motion"), *pr);
    }
    if (!ok) {
      error("INVALID_PROFILE",
            "Invalid profile capacity name preset or settings", h);
      return true;
    }
  }
  candidate_.revision = config::nextRevision(current.revision);
  if (!config::validDevice(candidate_) || !h.applyDevice(candidate_, now)) {
    error("BUSY", "Complete snapshot was not accepted", h);
    return true;
  }
  pending_ = pending_device_ = true;
  pending_revision_ = candidate_.revision;
  apply_started_us_ = now;
  return true;
}
} // namespace protocol
} // namespace fluxrail
