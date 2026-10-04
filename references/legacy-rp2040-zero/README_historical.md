# RP2040 Rhythm Controller

## Edge trial 1 (2026-09-15)

See [EDGE_TRIAL_1.md](EDGE_TRIAL_1.md) for the Waveshare RP2040-Zero test firmware,
build settings, calibration procedure, validation and rollback instructions.
The actual ambient LED pin in this variant's BoardConfig.h is GP10; the older GP8 assumptions below are historical.

This workspace contains:

- `firmware/RP2040RhythmController`: Arduino firmware for RP2040-Zero
- `firmware/RP2040RhythmControllerEdgeCalibrated`: new main firmware variant with the edge-calibrated hall algorithm
- `web/index.html`: Web Serial configurator
- `web/serve.py`: tiny local server for the configurator

## Firmware features

- 8 buttons on GPIO `{14, 13, 12, 3, 2, 1, 15, 4}`
- low-latency debounce suitable for hold + rapid repeat patterns
- 8 key LEDs on GPIO 9
- 8 ambient LEDs on `GPIO 8` by default
- 4 hall sensors on `A0-A3` by default
- keyboard and hall-slide HID output
- serial configuration protocol for the web tool
- config persistence in emulated EEPROM
- 5-second self-calibration routine for hall sensors
- Web-controlled UF2 bootloader entry for reflashing without pressing BOOTSEL
- edge-calibrated hall variant with auto polarity, endpoint scan, sensor gain sampling, center offset capture, and idle baseline tracking

## Important assumptions

Two hardware details were not included in the request, so this project currently assumes:

1. The 8 ambient RGB LEDs are chained on `GPIO 8`.
2. The 4 SS49E hall sensors are connected to `A0`, `A1`, `A2`, `A3`.

If your PCB uses different routing, edit:

- `firmware/RP2040RhythmController/BoardConfig.h`

## Arduino setup

Recommended environment:

- Arduino IDE 2.x
- RP2040 core by Earle Philhower

Open:

- `firmware/RP2040RhythmController/RP2040RhythmController.ino`
- or `firmware/RP2040RhythmControllerEdgeCalibrated/RP2040RhythmControllerEdgeCalibrated.ino` for the new edge-calibrated variant

Libraries used:

- `Adafruit TinyUSB`
- `Adafruit NeoPixel`
- `EEPROM` from the RP2040 core

Suggested Arduino settings:

- Board: your RP2040-Zero variant
- USB Stack: TinyUSB
- CPU Speed: default is fine

## Web configurator

Web Serial requires `http://localhost` or `https`.

Start the local server:

```powershell
cd C:\Users\40472\Documents\Codex\2026-04-27\arduino-rp2040-zero-1-8-rp2040\web
python serve.py
```

Then open:

- `http://127.0.0.1:8000`

## Serial command protocol

The browser talks to the controller over newline-delimited text commands:

- `GET`
- `SET KEYMAP <index> <hidCode>`
- `SET SLIDE 0|1`
- `SET KEYLED_EFFECT PRESS|SOLID|RAINBOW`
- `SET KEYLED_BRIGHTNESS <0-255>`
- `SET KEYLED_COLOR <r> <g> <b>`
- `SET AMBIENT_EFFECT RAINBOW|FLOW|MARQUEE`
- `SET AMBIENT_BRIGHTNESS <0-255>`
- `SET AMBIENT_COLOR <r> <g> <b>`
- `SET SYNC 0|1`
- `SET CALIBRATE`
- `SET HALL_POLARITY AUTO|RISE|DROP|FLIP`
- `SET HALL_ENDPOINT_SCAN`
- `SET HALL_SENSOR_GAIN`
- `SET HALL_CENTER`
- `SET HALL_CAL_SAVE`
- `SET HALL_CAL_DEFAULTS`
- `BOOTLOADER` / `BOOT` / `UF2`

The main firmware can jump to UF2 bootloader mode from the web configurator or
the serial commands above, so you can reflash without opening the enclosure to
press BOOTSEL. In the diagnostic firmware, sending `B` over serial does the same
thing; it also keeps the physical `BT1 + BT8` hold shortcut for bench testing.
The edge-calibrated main firmware also accepts the test-style single-letter
commands (`Z/A/O/E/G/C/S/D/T/P/B`) when they are sent as a complete line, while
keeping the newline-delimited `GET` / `SET ...` web protocol intact.

## Notes

- All settings are applied immediately.
- The current firmware sends 8 keyboard buttons and an optional hall-derived horizontal mouse axis.
- Duplicate key mappings are allowed. For example, K1 and K2 can both be mapped to `A`.

If you want, the next step can be refining the exact switch ordering and the hall-to-axis curve after you confirm the real PCB pinout.
