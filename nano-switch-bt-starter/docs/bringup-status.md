# Bring-Up Status

Last updated: `2026-04-19`

## Goal

Use an `Arduino Nano RP2040 Connect` as:

- `PC -> USB CDC -> RP2040`
- `RP2040 -> UART -> NINA-W102`
- `NINA-W102 -> Bluetooth HID -> Switch`

The current repo is the starter implementation for that split.

## Where We Are

The hardware and transport bring-up succeeded, and the NINA firmware has been moved to a first `Left Joy-Con` scaffold.

Confirmed working:

- The `RP2040` side uploads from Arduino IDE.
- The custom `NINA` firmware builds with stock `ESP-IDF v4.4.x` and flashes through `SerialNINAPassthrough`.
- The shared bridge protocol works end to end.
- The NINA Bluetooth Classic HID stack initializes.
- The device advertises as `Joy-Con (L)`.
- Pairing to a normal host works.
- The HID layer now emits `0x30`-style reports and replies to a small set of Joy-Con subcommands.

Not done yet:

- verification against a real Switch pairing flow
- any extra subcommands or timing quirks the Switch still insists on
- pairing persistence and real SPI-flash-backed identity behavior
- rumble, LEDs, IMU, and reconnect behavior polished beyond the starter scaffold

Latest real-Switch result:

- authentication completes and a bond is created
- the HID stack then reports `OPEN status=0x01 conn_status=0x02`
- the GAP layer reports `ACL_DISCONN_CMPL reason=0x13`
- no `SET_REPORT`, `GET_REPORT`, or `INTR_DATA` traffic is seen before disconnect

Why this matters:

- the current blocker appears to be at or below the stock `esp_hidd` / SDP identity layer, not in the later Joy-Con subcommand reply code

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
- stock `ESP-IDF v4.4.x`

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

The current recommended diagnostic command for Switch-side failures is:

```powershell
python host_tools\send_state.py dump-events COM3
```

That dumps the recent NINA event ring buffer, including HID callback events, GAP callback events, and a few internal HID API calls such as `REGISTER_APP`, `CONNECT`, `SEND_REPORT`, and bond clearing.

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

Working command:

```powershell
idf.py -p COM3 -b 115200 flash
```

If `idf.py` still opens the port with the wrong baud, fall back to direct `esptool.py` with `--baud 115200` and `@build\flash_args`.

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
- advertises as `Joy-Con (L)`
- exposes Joy-Con report IDs instead of the old generic gamepad report
- sends `0x30`-style reports from RP2040-provided state
- replies to common Joy-Con subcommands such as device info, input report mode, SPI reads, player lights, IMU enable, vibration enable, and voltage query
- serves a sparse fake SPI/config image with controller type, colors, and baseline calibration values

This is enough for:

- status polling
- pairing to a normal host
- exercising a first Left Joy-Con persona over Bluetooth HID

This is not yet enough for:

- claiming full Switch compatibility
- knowing which remaining details the real Switch still rejects

## Resume Plan

The next implementation target should be:

1. keep the current RP2040 bridge as-is unless debugging forces changes
2. test the current Left Joy-Con scaffold against the Switch
3. log which subcommands, timing, or identity reads still fail
4. tighten the SPI/pairing identity behavior based on those observations
5. only then add rumble and real IMU data

Recommended first persona:

- `Left Joy-Con`

Reason:

- existing projects and notes consistently suggest it is the least fiddly wireless path compared with full Pro Controller emulation

## Practical Notes

- The last confirmed RP2040 USB port was `COM3`, but do not assume it is stable across reconnects.
- Pairing success so far was with a normal host, not with the Switch.
- The current NINA HID implementation is no longer the original generic placeholder, but it is still a scaffold and should not yet be mistaken for confirmed Switch interoperability.
- `SwitchCon`, `UARTSwitchCon`, and `BlueCon-esp32` all point at the custom `NathanReeves/esp-idf` fork rather than stock Espressif `esp-idf`.
- That strongly suggests the remaining Switch rejection may be caused by differences in the underlying Classic HID implementation, not only by missing app-layer subcommands.
- The current repo has been nudged closer to `SwitchCon` by matching its HID descriptor layout and a few early replies, but this has not yet been revalidated on hardware.
