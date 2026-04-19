# Bring-Up Status

Last updated: `2026-04-19`

## Goal

Use an `Arduino Nano RP2040 Connect` as:

- `PC -> USB CDC -> RP2040`
- `RP2040 -> UART -> NINA-W102`
- `NINA-W102 -> Bluetooth HID -> Switch`

The current repo is the starter implementation for that split.

## Where We Are

The hardware and transport bring-up succeeded.

Confirmed working:

- The `RP2040` side uploads from Arduino IDE.
- The custom `NINA` firmware builds with `ESP-IDF v4.4.6` and flashes through `SerialNINAPassthrough`.
- The shared bridge protocol works end to end.
- The NINA Bluetooth Classic HID stack initializes.
- The device advertises as `Nano Switch Starter`.
- Pairing to a normal host works.

Not done yet:

- Nintendo Switch-specific HID identity
- `OUTPUT 0x01` parsing
- `INPUT 0x21` subcommand replies
- `INPUT 0x30` streaming reports in Switch format
- device info and SPI-flash-backed identity behavior
- rumble, LEDs, IMU, and pairing persistence details expected by the Switch

## What Was Flashed

### RP2040

Two sketches were used at different times:

- `WiFiNINA -> Tools -> SerialNINAPassthrough`
  Used only to flash the NINA.
- `rp2040_bridge/rp2040_bridge.ino`
  Used for normal operation after the NINA was flashed.

### NINA

The NINA was flashed with the custom ESP-IDF project in:

- `nina_firmware/`

## Working Toolchain

- Arduino IDE with `Arduino Nano RP2040 Connect`
- ESP-IDF `v4.4.6`

Important NINA build settings:

- `Serial flasher config -> Before flashing -> No reset`
- `Component config -> ESP System Settings -> Channel for console output -> None`

These are also captured in `nina_firmware/sdkconfig.defaults`.

## Exact Bring-Up Outcome

The first successful bridge test from the PC was:

```powershell
python host_tools\send_state.py status COM3
```

Reply:

```text
53 42 01 03 0e 08 c4 05 07 00 30 08 00 00 00 00 53 42 01 03 0f 08 81 6a 07 00 30 08 00 00 00 00
```

This means:

- `STATUS` frames were received back from the NINA
- `flags = 0x07`
  `BRIDGE_READY | BT_READY | HID_READY`
- `input_report_mode = 0x30`
- `battery_level = 8`
- `last_error = 0`

After that, pairing to a host was confirmed to work.

## Important Fixes Made During Bring-Up

These are the key changes that were necessary to reach the current working state.

### 1. Arduino IDE include path fix

Problem:

- Arduino IDE could not compile the RP2040 sketch because it does not reliably support `..` includes outside the sketch folder.

Fix:

- `rp2040_bridge/rp2040_bridge.ino` now includes a local `switch_bridge_protocol.h`
- a copy of the shared protocol header was added under `rp2040_bridge/`

### 2. RP2040 serial port fix

Problem:

- The RP2040 bridge originally used the wrong internal serial alias for the NINA path.

Fix:

- `rp2040_bridge/rp2040_bridge.ino` now uses `SerialNina`

Why it matters:

- On `NANO_RP2040_CONNECT`, the board variant maps the NINA serial path to `SerialNina`, while `SerialHCI` is a different internal alias.

### 3. RP2040 boot control for the NINA

Problem:

- After flashing, the NINA could be left in the wrong state and the bridge would get no reply.

Fix:

- the RP2040 bridge now explicitly drives `NINA_GPIO0` and `NINA_RESETN`
- on startup it forces the NINA to boot the flashed application, not ROM download mode

### 4. Arduino `.ino` prototype issue

Problem:

- Arduino IDE auto-generated a prototype for `PumpFrames(...)` before the `FrameDecoder` class was known.

Fix:

- added a forward declaration for `FrameDecoder`

### 5. NINA build configuration

Problem:

- the first NINA builds failed because Bluetooth Classic HID features were not enabled in config
- the project also needed clearer target handling for `esp32`

Fix:

- added `nina_firmware/sdkconfig.defaults`
- added an `esp32` target guard in `nina_firmware/CMakeLists.txt`

### 6. ESP-IDF 4.4 API compatibility fixes

Problem:

- some APIs/constants in the first draft did not match ESP-IDF `v4.4.6`

Fixes:

- switched to `esp_bt_dev_set_device_name(...)`
- removed use of missing `ESP_BT_COD_MINOR_PERIPHERAL_COMBO`
- added explicit Bluetooth include paths in `nina_firmware/main/CMakeLists.txt`

### 7. Flashing behavior through SerialNINAPassthrough

Problem:

- flashing the NINA failed with `No serial data received`

Fix:

- use `--before no_reset`
- use `115200` baud for flashing through the Nano RP2040 Connect passthrough path

## Files Worth Reading First

If resuming this project later, read these in order:

1. `docs/bringup-status.md`
2. `docs/architecture.md`
3. `docs/building.md`
4. `rp2040_bridge/rp2040_bridge.ino`
5. `nina_firmware/main/bridge_uart.c`
6. `nina_firmware/main/switch_hid.c`

## Current Behavior Of The Firmware

### RP2040 bridge

The RP2040:

- exposes a USB CDC serial port to the PC
- boots the NINA into application mode
- forwards framed packets between USB serial and `SerialNina`
- periodically sends `HELLO`

### NINA firmware

The NINA:

- reads framed controller-state packets over UART
- replies to `HELLO` and `GET_STATUS`
- brings up Bluetooth Classic HID
- advertises as `Nano Switch Starter`
- exposes a generic gamepad HID placeholder

This is enough for:

- status polling
- pairing to a normal host

This is not yet enough for:

- pairing or behaving correctly as a Nintendo Switch controller

## Resume Plan

The next implementation target should be:

1. keep the current RP2040 bridge as-is unless debugging forces changes
2. replace the generic HID placeholder in `nina_firmware/main/switch_hid.c`
3. implement Switch-style `0x21` subcommand replies
4. implement Switch-style `0x30` reports
5. choose and lock a controller persona

Recommended first persona:

- `Left Joy-Con`

Reason:

- existing projects and notes consistently suggest it is the least fiddly wireless path compared with full Pro Controller emulation

## Practical Notes

- The last confirmed RP2040 USB port was `COM3`, but do not assume it is stable across reconnects.
- Pairing success so far was with a normal host, not with the Switch.
- The current NINA HID implementation is intentionally a placeholder and should not be mistaken for partial Switch protocol support.
