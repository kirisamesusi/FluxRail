# Firmware rollback archive: 0.10.1

Reason: Retrospective complete rollback archive requested by user

- Previous UF2: artifacts/FluxRail.ino.uf2. This archive does not flash it.
- Previous exact code: source/firmware/FluxRail; all build-manifest code SHA verified.
- Next compile input snapshot: next-source/firmware/FluxRail.
- Changes: changes.patch, compared against previous actual build sources.
- Supporting non-code documents are copied from current checkout, not claimed historical.
- Private TinyUSB adapter archived; Arduino-Pico/tool versions are in firmware_results.json.
- Restore source by copying source/firmware/FluxRail into the project after separately
  backing up current work. Review changes.patch first. A firmware rollback requires
  explicitly choosing a UF2 matching the target board and restoring configuration separately.
- Configuration backup/board validation are independent from code/UF2 rollback.

Changed source files: firmware/FluxRail/src/common/FirmwareInfo.h, firmware/FluxRail/src/usb/Io4Codec.cpp
