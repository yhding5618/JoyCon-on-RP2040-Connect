# Building And Flashing

## Prerequisites

- Arduino IDE or `arduino-cli` with the `Arduino Mbed OS RP2040` core installed
- ESP-IDF `v4.4.8`
- Python 3
- `esptool.py`
- `pyserial` if you want to use the host test tool

The recommendation to use ESP-IDF `v4.4.8` is deliberate: Arduino's `nina-fw` build instructions for the Nano RP2040 Connect target that branch, which reduces surprise when you are flashing through the board's established NINA path.

## Flash Order

You normally flash the NINA first, then restore the RP2040 bridge sketch.

### 1. Temporarily Put The RP2040 Into NINA Passthrough Mode

Upload Arduino's `SerialNINAPassthrough` example to the Nano RP2040 Connect.

This is the official pattern Arduino uses for flashing the NINA on this board family.

### 2. Build The NINA Firmware

From `nano-switch-bt-starter/nina_firmware`:

```sh
idf.py set-target esp32
idf.py build
```

### 3. Flash The NINA Firmware

Use the same serial port that the Nano exposes while `SerialNINAPassthrough` is running. If you use `esptool.py` directly, keep Arduino's `no_reset` note in mind for Nano RP2040 Connect passthrough flashing.

If you prefer to stay close to Arduino's published flow, review the official `arduino/nina-fw` flashing notes first.

### 4. Restore The RP2040 Bridge

Upload `rp2040_bridge/rp2040_bridge.ino` to the RP2040.

After this step the RP2040 is no longer a transparent flash bridge for the NINA. If you want reflashing convenience later, add a bridge command that drops into passthrough mode or just re-upload `SerialNINAPassthrough` temporarily.

## Host Test

Install `pyserial`:

```sh
pip install pyserial
```

Send a neutral controller state:

```sh
python host_tools/send_state.py state COM5
```

Request status:

```sh
python host_tools/send_state.py status COM5
```

Replace `COM5` with your board's serial port.

## Important Configuration Note

The NINA app uses `UART0` for the RP2040 link because the board wiring routes NINA `GPIO1/GPIO3` to the RP2040's internal UART. That means you should disable the ESP-IDF console on UART before relying on binary traffic, otherwise boot logs and bridge frames will collide.

If your local ESP-IDF configuration still emits console logs on `UART0`, change it in `menuconfig` before final testing.

## Recommended Next Milestones

1. Confirm the PC can send `SET_STATE` and the NINA can reply with `STATUS`.
2. Confirm the generic HID starter enumerates to a normal Bluetooth host.
3. Replace the generic HID descriptor with the Switch-specific path.
4. Implement host output report handling and subcommand replies.
