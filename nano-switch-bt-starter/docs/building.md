# Building And Flashing

## Prerequisites

- Arduino IDE or `arduino-cli` with the `Arduino Mbed OS RP2040` core installed
- ESP-IDF `v4.4.8`
- Python 3
- `esptool.py`
- `pyserial` if you want to use the host test tool

The recommendation to use stock `ESP-IDF v4.4.8` is deliberate: Arduino's `nina-fw` build instructions for the Nano RP2040 Connect target that branch, which reduces surprise when you are flashing through the board's established NINA path.

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

On this board, the practical flash command should be explicit about baud:

```sh
idf.py -p COM3 -b 115200 flash
```

Replace `COM3` with the port currently exposed by the Nano while `SerialNINAPassthrough` is loaded.

If `idf.py flash` still chooses the wrong baud or stalls at `Serial port COMx`, use the generated flash args directly:

```sh
C:\Espressif\python_env\idf4.4_py3.11_env\Scripts\python.exe ^
  C:\Espressif\frameworks\esp-idf-v4.4.8\components\esptool_py\esptool\esptool.py ^
  --chip esp32 --port COM3 --baud 115200 --before no_reset --after hard_reset ^
  write_flash @build\flash_args
```

Common causes of a stall at `Serial port COMx`:

- the RP2040 is still running `rp2040_bridge.ino` instead of `SerialNINAPassthrough`
- another serial tool still has the COM port open
- the COM port number changed after re-uploading the passthrough sketch or reconnecting USB

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

## Switch HID SDP Compatibility Patch

The 203-byte Joy-Con HID descriptor does not fit in stock ESP-IDF v4.4.8's default per-record SDP pad once the other HID attributes are present. Stock ESP-IDF also advertises HID country code `0x21`, while the Joy-Con reference SDP uses `0x00`.

Before testing the Joy-Con descriptor build against the Switch, patch the local ESP-IDF tree:

```powershell
pwsh host_tools\patch_idf_hid_switch_compat.ps1 -IdfPath C:\Espressif\frameworks\esp-idf-v4.4.8
```

Then rebuild the NINA firmware with a clean BT component rebuild:

```powershell
idf.py fullclean
idf.py build
```

## Per-Mode Pairing Persistence Note

The firmware now stores app-owned pairing metadata in separate NVS namespaces:

- `sw_ljc`
- `sw_rjc`
- `sw_pro`

It also selects per-mode Bluedroid bond paths:

- `bt_ljc`
- `bt_rjc`
- `bt_pro`

`esp_bt_config_file_path_update()` is only available in newer ESP-IDF releases. On the original stock `v4.4.x` toolchain, the firmware logs a warning and still builds, but Bluedroid's internal bond store remains shared unless that API is backported or the project is built on an ESP-IDF version that provides it.

## Recommended Next Milestones

1. Confirm the PC can send `SET_STATE` and the NINA can reply with `STATUS`.
2. Confirm the generic HID starter enumerates to a normal Bluetooth host.
3. Validate `pairing-info`, `pairing-start`, and `forget-current` over `host_tools/send_state.py`.
4. Run the Left/Right/Pro pairing persistence test matrix against hardware.
