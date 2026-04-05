# Architecture

## Board Split

The Nano RP2040 Connect has two processors on board:

- `RP2040`
  Handles USB device mode and talks to the NINA internally.
- `NINA-W102`
  ESP32-based open CPU module with Bluetooth BR/EDR and BLE support.

For this project the cleanest split is:

- `PC -> USB CDC -> RP2040`
- `RP2040 -> UART -> NINA`
- `NINA -> Bluetooth HID -> Switch`

This keeps your host-facing protocol stable while the Switch-facing Bluetooth implementation evolves independently.

## Internal Link

Arduino's Nano RP2040 Connect variant maps the internal NINA link to RP2040 internal pins:

- `RESET_NINA -> D24`
- `SPI1_CIPO / UART1_TX -> D25`
- `SPI1_CS / UART1_RX -> D26`
- `SPI1_ACK / UART1_CTS -> D27`
- `SPI1_COPI / UART1_RTS -> D28`
- `SPI1_SCK -> D29`

The Arduino core exposes the internal UART as `SerialHCI`, so the bridge sketch uses:

- `Serial` for the PC-facing USB CDC channel
- `SerialHCI` for the NINA-facing internal UART

This starter only uses TX/RX. CTS/RTS can be added later if you want stricter flow control.

## Bridge Protocol

All three layers use the same binary frame:

- 2-byte magic
- protocol version
- message type
- sequence
- payload length
- CRC16-CCITT
- payload

Current message types:

- `HELLO`
- `GET_STATUS`
- `STATUS`
- `SET_STATE`
- `VIRTUAL_CABLE_UNPLUG`

`SET_STATE` carries a compact controller struct:

- `buttons` as a 32-bit bitfield
- `lx`, `ly`, `rx`, `ry` as signed 16-bit axes
- `hat`
- `misc`
- `battery_level`

## NINA Firmware Layers

The ESP-IDF app is split into:

- `bridge_uart.c`
  Reads and validates frames from the RP2040 and routes them to the HID layer.
- `switch_hid.c`
  Owns Bluetooth Classic HID setup and periodic report emission.
- `app_main.c`
  Brings up NVS, UART, and the HID stack, then drives the report tick.

## Current HID Behavior

The starter HID implementation uses a generic gamepad descriptor so the NINA app has something real to register and send. This is intentionally a placeholder.

The next meaningful Switch-specific upgrades are:

1. Replace the starter descriptor with a Switch-compatible identity.
2. Parse host output reports and build `0x21` subcommand responses.
3. Stream `0x30` input reports at the right cadence.
4. Add pairing persistence and any identity/calibration data the Switch expects.

## PC Tool

`host_tools/send_state.py` is intentionally small and dumb. It just emits bridge frames so you can validate:

- USB CDC on the RP2040
- UART forwarding into the NINA
- state updates arriving at the HID layer

Once the bridge is stable, you can replace the Python script with your real controller application or emulator pipeline.
