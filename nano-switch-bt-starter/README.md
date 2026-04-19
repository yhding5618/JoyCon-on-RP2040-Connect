# Nano RP2040 Connect Switch BT Starter

This scaffold splits the Arduino Nano RP2040 Connect into two jobs:

- The `RP2040` side is a USB-to-UART bridge that accepts control frames from your PC.
- The `NINA-W102` side is an `ESP-IDF` app that exposes a Bluetooth HID device and consumes those frames.

The result is a practical starting point for `PC -> RP2040 -> NINA -> Switch`. The repo has now moved past the original generic HID placeholder into a `Left Joy-Con`-first scaffold, but it is still not a finished Switch-compatible controller.

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
- A NINA-side Bluetooth Classic HID app built around `esp_hidd`.
- A `Left Joy-Con`-style Bluetooth HID identity with Joy-Con report IDs.
- Periodic `0x30`-style input report generation from controller state received over UART.
- Basic `0x21` subcommand replies for device info, report mode, SPI reads, player lights, IMU enable, vibration enable, and voltage queries.
- A small NINA-side event log that can be dumped from the PC for HID and GAP troubleshooting.

## What Is Still Missing For Real Switch Support

- Verification that the current Left Joy-Con scaffold is accepted by a real Switch.
- Any additional output reports or subcommands the Switch still insists on during pairing or reconnection.
- Pairing/bond persistence and device identity behavior that matches a Joy-Con or Pro Controller.
- Optional IMU, player LEDs, and rumble translation.

Use this repo as the first clean implementation boundary, not as the last mile.

## Current Status

As of `2026-04-19`, the transport bring-up is working and the NINA firmware now contains a first Left Joy-Con scaffold:

- The `RP2040` bridge sketch uploads and runs.
- The `NINA` firmware builds and flashes through `SerialNINAPassthrough`.
- `PC -> RP2040 -> NINA` framed status requests work.
- The NINA advertises as `Joy-Con (L)`.
- Pairing to a host works.
- The HID layer now responds to common Joy-Con subcommands and emits Switch-style reports.

The project is still not confirmed Switch-compatible. The next phase is hardware validation against a real Switch and then tightening whatever probe/reply details the console still rejects.

For the current failure mode, the quickest diagnostic command is:

```powershell
python host_tools\send_state.py dump-events COM3
```

The most recent real-Switch test got through Bluetooth authentication and bonding, but the connection still dropped before any HID report exchange. In practice, that means the remaining blocker likely sits in the stock `esp_hidd` / SDP identity path, not only in the later Joy-Con subcommand replies. Existing ESP32 Switch-controller projects consistently point at a custom `esp-idf` fork for this layer, so keep that in mind before spending too long polishing app-layer behavior alone.

See `docs/bringup-status.md` before resuming work.
