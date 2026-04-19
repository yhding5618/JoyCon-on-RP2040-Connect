# Nano RP2040 Connect Switch BT Starter

This scaffold splits the Arduino Nano RP2040 Connect into two jobs:

- The `RP2040` side is a USB-to-UART bridge that accepts control frames from your PC.
- The `NINA-W102` side is an `ESP-IDF` app that exposes a Bluetooth HID device and consumes those frames.

The result is a practical starting point for `PC -> RP2040 -> NINA -> Switch`, but it is still a starter and not a finished Switch-compatible controller. The hard Switch-specific work is still ahead of us: descriptor fidelity, subcommand replies, controller identity, pairing persistence, and calibration/IMU behavior.

## Layout

- `shared/include/switch_bridge_protocol.h`
  Shared frame format used by the PC tool, RP2040 bridge, and NINA app.
- `rp2040_bridge/rp2040_bridge.ino`
  Arduino sketch that forwards framed packets between USB CDC and the internal NINA UART.
- `nina_firmware/`
  ESP-IDF project for the NINA-W102 with a UART transport layer and HID starter app.
- `host_tools/send_state.py`
  Minimal Python sender for quick PC-side testing.
- `docs/architecture.md`
  Wiring, protocol, and implementation notes.
- `docs/building.md`
  Flash order and build steps.
- `docs/bringup-status.md`
  A concise handoff log of what has been built, flashed, tested, fixed, and what remains.

## What Works In This Scaffold

- A binary bridge protocol with CRC checks.
- A USB serial entry point for PC control.
- A NINA-side Bluetooth Classic HID starter built around `esp_hidd`.
- Periodic HID report generation from controller state received over UART.

## What Is Still Missing For Real Switch Support

- Switch-specific Bluetooth HID descriptor and SDP tuning.
- Handling of Switch output reports such as subcommands and rumble.
- Proper `0x21` and `0x30` report behavior instead of the starter generic gamepad report.
- Pairing/bond persistence and device identity behavior that matches a Joy-Con or Pro Controller.
- Optional IMU, player LEDs, and rumble translation.

Use this repo as the first clean implementation boundary, not as the last mile.

## Current Status

As of `2026-04-19`, the transport and generic Bluetooth HID bring-up is working:

- The `RP2040` bridge sketch uploads and runs.
- The `NINA` firmware builds and flashes through `SerialNINAPassthrough`.
- `PC -> RP2040 -> NINA` framed status requests work.
- The NINA advertises as `Nano Switch Starter`.
- Pairing to a host works with the current generic HID placeholder.

The project is not yet Switch-compatible. The next phase is to replace the generic HID logic in `nina_firmware/main/switch_hid.c` with Switch-style `0x21` subcommand replies and `0x30` input reports.

See `docs/bringup-status.md` before resuming work.
