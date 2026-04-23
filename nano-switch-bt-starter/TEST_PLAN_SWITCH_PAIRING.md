# Test Plan: RP2040 Connect / NINA Switch Joy-Con Emulation

This test plan is intended for Codex or another coding agent to implement and run against:

- Target repo: <https://github.com/yhding5618/JoyCon-on-RP2040-Connect>
- Reference repo: <https://github.com/mart1nro/joycontrol>

The current failure mode to isolate is:

> PC can discover and pair with the HID gamepad, but Nintendo Switch does not discover it or disconnects before HID report traffic.

The repo already has UART commands for virtual-cable unplug and clearing bonds, so this plan treats stale bonding as a condition to verify, not the main suspected root cause. The current bring-up notes suggest the Switch pairing attempt reaches authentication/bonding, then receives an HID open event with non-success status or a GAP disconnect before `SET_REPORT`, `GET_REPORT`, or interrupt data. That points below the Joy-Con subcommand layer, likely at SDP/HID identity or stock `esp_hidd` behavior.

---

## Goals

1. Confirm whether bond/virtual-cable cleanup is actually completed before every Switch test.
2. Confirm whether the advertised Classic Bluetooth identity matches the expected Joy-Con-like identity.
3. Replace the current HID descriptor with the byte-for-byte `joycontrol` descriptor and test whether the Switch begins sending HID reports.
4. Fix obvious app-layer protocol mismatches that will matter after the Switch reaches report exchange.
5. Determine whether stock `esp_hidd` is the remaining blocker.

---

## Non-Goals

- Do not implement raw L2CAP HID from scratch in this test pass.
- Do not port all of `joycontrol` yet.
- Do not add motion, rumble, NFC, or IR features.
- Do not make many simultaneous changes; each test should isolate one variable.

---

## Definitions

### Pass Level 0: Discoverable by PC

The device appears in `bluetoothctl scan on` or a similar PC Bluetooth scan.

### Pass Level 1: Discoverable by Switch

The device appears on Nintendo Switch under:

```text
Controllers → Change Grip/Order
```

### Pass Level 2: HID Connection Starts

Firmware logs show:

```text
ESP_HIDD_OPEN_EVT
status == ESP_HIDD_SUCCESS
```

or an equivalent successful connection state.

### Pass Level 3: Switch Sends HID Traffic

Firmware logs show at least one of:

```text
ESP_HIDD_SET_REPORT_EVT
ESP_HIDD_GET_REPORT_EVT
ESP_HIDD_INTR_DATA_EVT
```

### Pass Level 4: Joy-Con Protocol Handshake

Firmware logs show Switch subcommands such as:

```text
0x02 GET_DEVICE_INFO
0x03 SET_INPUT_REPORT_MODE
0x10 SPI_FLASH_READ
0x30 SET_PLAYER_LIGHTS
```

### Pass Level 5: Usable Controller

Switch accepts input reports and a button press changes state on the Switch UI.

---

## Test Matrix

Run these tests in order.

| Test ID | Change Under Test | Expected Result |
|---|---|---|
| T00 | Baseline build and log capture | Establish current failure signature |
| T01 | Confirm bond cleanup before HID registration | `in_use == 0`, no auto-reconnect to stale host |
| T02 | Add identity/status logging | Confirm name, CoD, MAC, descriptor length |
| T03 | PC SDP dump | Compare generated SDP against `joycontrol` |
| T04 | Replace descriptor with `joycontrol` descriptor only | Switch should reach at least Pass Level 3 |
| T05 | Fix 12-byte device-info reply only | If Pass Level 3 exists, improve subcommand handshake |
| T06 | Disable auto reconnect on `register_app.in_use` | Confirm no stale host path remains |
| T07 | Compare stock `esp_hidd` SDP with expected SDP | Decide whether custom SDP/L2CAP is needed |
| T08 | Switch final functional test | Confirm button input |

---

# T00: Baseline Build and Runtime Log Capture

## Purpose

Create a clean baseline before making changes.

## Codex Tasks

1. Build the current NINA firmware.
2. Confirm there are no compiler warnings in:
   - `switch_hid.c`
   - `bridge_uart.c`
   - UART protocol files
3. Add a `docs/test-results/` folder if missing.
4. Add a template file:

```text
docs/test-results/YYYY-MM-DD-baseline.md
```

5. Record:
   - Git commit hash.
   - ESP-IDF version.
   - NINA firmware build config.
   - RP2040 sketch used.
   - Whether PC can discover device.
   - Whether Switch can discover device.
   - Full UART/HID/GAP event log.

## Manual Test Steps

1. Flash NINA firmware.
2. Flash or run the RP2040 bridge.
3. Power-cycle the board.
4. Run the normal bond-clear command.
5. Put Switch into:

```text
Controllers → Change Grip/Order
```

6. Capture all NINA logs during the pairing attempt.

## Pass Criteria

Baseline log must include enough data to classify the failure:

```text
REGISTER_APP status
REGISTER_APP in_use
OPEN status
OPEN conn_status
GAP disconnect reason
whether any SET_REPORT / GET_REPORT / INTR_DATA occurred
```

## Fail Criteria

Logs do not include event IDs/statuses needed to classify the failure.

---

# T01: Verify Bond and Virtual-Cable Cleanup Actually Happens Before Switch Test

## Purpose

The repo has clear-bond and virtual-cable-unplug UART commands, but this test confirms they complete before the Switch attempt.

Expected relevant paths:

```c
SB_MSG_VIRTUAL_CABLE_UNPLUG -> switch_hid_virtual_cable_unplug()
SB_MSG_CLEAR_BONDS          -> switch_hid_clear_all_bonds()
```

## Codex Tasks

Add or verify logs around:

```c
switch_hid_virtual_cable_unplug();
switch_hid_clear_all_bonds();
esp_bt_gap_get_bond_device_num();
ESP_HIDD_REGISTER_APP_EVT param->register_app.in_use;
```

The log should emit a compact line like:

```text
BOND_STATE before_clear=<n> after_clear=<n> vc_unplug_called=<0|1> register_in_use=<0|1>
```

## Manual Test Steps

1. Pair with PC once to intentionally create a host bond.
2. Reboot.
3. Run clear-bond and virtual-cable-unplug command.
4. Reboot again.
5. Start Switch pairing test.
6. Capture `REGISTER_APP` event.

## Pass Criteria

Expected log before Switch test:

```text
after_clear=0
register_in_use=0
```

No call to:

```c
esp_bt_hid_device_connect(param->register_app.bd_addr);
```

during the Switch test.

## Fail Criteria

Any of the following:

```text
after_clear > 0
register_in_use = 1
automatic esp_bt_hid_device_connect() happens
```

## Follow-Up If Failing

Move clear-bond / virtual-cable-unplug earlier in startup, or add a temporary compile-time test mode that prevents reconnecting to the stored HID host.

---

# T02: Add Bluetooth Identity Logging

## Purpose

Confirm the actual advertised identity, not just intended constants.

The firmware should verify and log:

- Nintendo-like MAC prefix, if spoofing is enabled.
- HID subclass `0x08`.
- Device name expected by the Switch-facing GAP identity.
- HID service name `Wireless Gamepad`.
- HID description `Gamepad`.
- HID provider `Nintendo`.
- Class of Device peripheral/gamepad value.

## Codex Tasks

Add a function like:

```c
static void log_bt_identity(const char *stage);
```

It should log:

```text
stage
BT address from esp_bt_dev_get_address()
intended base MAC
GAP device name
CoD value
HID app name
HID description
HID provider
HID subclass
HID descriptor length
```

Call it at:

```text
after configure_nintendo_like_base_mac()
after Bluetooth controller init
after esp_bluedroid_enable()
after configure_gap_identity()
after ESP_HIDD_REGISTER_APP_EVT
```

## Pass Criteria

Logs show a stable and expected identity:

```text
Name: Joy-Con (L), or intended Switch-facing GAP name
HID service name: Wireless Gamepad
Description: Gamepad
Provider: Nintendo
HID subclass: 0x08
MAC prefix: D4:F0:57, if spoofing is expected to work
Descriptor length: known value
```

## Fail Criteria

Any of the following:

```text
MAC spoofing runs too late and actual BT address does not use intended prefix
CoD is not Peripheral/Gamepad-like
GAP name is not the intended Switch-facing name
HID descriptor length is unexpected
```

---

# T03: Dump SDP from a Linux PC and Compare Against `joycontrol`

## Purpose

The Switch likely rejects before app-layer report exchange. A PC may accept generic HID, but the Switch may require the SDP/HID identity to look much closer to a Joy-Con.

`joycontrol` registers a HID SDP record with:

```text
Service Class UUID: 0x1124
Control PSM:        0x0011
Interrupt PSM:      0x0013
Service name:       Wireless Gamepad
Description:        Gamepad
Provider:           Nintendo
HIDDeviceSubclass:  0x08
HIDVirtualCable:    true
HIDReconnectInit:   true
```

Reference file:

```text
joycontrol/profile/sdp_record_hid.xml
```

## Codex Tasks

Create a helper script:

```text
tools/dump_sdp.sh
```

Suggested content:

```bash
#!/usr/bin/env bash
set -euo pipefail

ADDR="${1:?usage: $0 <BT_ADDR>}"

mkdir -p docs/test-results

echo "Browsing SDP for $ADDR"
sdptool browse "$ADDR" | tee "docs/test-results/sdp-${ADDR//:/-}.txt"
```

Create a comparison checklist file:

```text
docs/sdp-checklist.md
```

Checklist fields:

```text
Service Class UUID: 0x1124
Protocol Descriptor List includes L2CAP PSM 0x0011
Additional Protocol Descriptor List includes L2CAP PSM 0x0013
Service name: Wireless Gamepad
Description: Gamepad
Provider: Nintendo
HIDDeviceReleaseNumber: 0x0100
HIDParserVersion: 0x0111
HIDDeviceSubclass: 0x08
HIDCountryCode: 0x00
HIDVirtualCable: true
HIDReconnectInitiate: true
HIDDescriptorList begins with joycontrol descriptor prefix
```

## Manual Test Steps

1. Put NINA into discoverable mode.
2. On Linux PC:

```bash
bluetoothctl scan on
```

3. Note the NINA Bluetooth address.
4. Run:

```bash
./tools/dump_sdp.sh XX:XX:XX:XX:XX:XX
```

5. Compare the result to the checklist.

## Pass Criteria

All checklist fields match or are explainably equivalent.

## Fail Criteria

Any missing or substantially different HID SDP fields.

## Important Output

Record whether the generated `esp_hidd` SDP exposes the same HID descriptor bytes as compiled into `kSwitchJoyConDescriptor`.

---

# T04: Replace HID Descriptor With `joycontrol` Descriptor Only

## Purpose

The current descriptor in `switch_hid.c` appears to differ from `joycontrol`'s SDP descriptor. `joycontrol`'s descriptor begins with:

```text
05 01 15 00 09 04 A1 01 85 30
```

This test changes only the HID descriptor and checks whether the Switch begins sending HID reports.

## Codex Tasks

Create a feature flag:

```c
#define SWITCH_HID_USE_JOYCONTROL_DESCRIPTOR 1
```

Add two descriptors:

```c
static const uint8_t kCurrentDescriptor[] = { ... };
static const uint8_t kJoycontrolDescriptor[] = { ... };
```

Select using the feature flag:

```c
#if SWITCH_HID_USE_JOYCONTROL_DESCRIPTOR
#define SWITCH_HID_DESCRIPTOR kJoycontrolDescriptor
#else
#define SWITCH_HID_DESCRIPTOR kCurrentDescriptor
#endif
```

Update HID app registration:

```c
.desc_list = (uint8_t *)SWITCH_HID_DESCRIPTOR,
.desc_list_len = sizeof(SWITCH_HID_DESCRIPTOR),
```

Use this exact descriptor from `joycontrol` SDP attribute `0x0206`:

```c
static const uint8_t kJoycontrolDescriptor[] = {
    0x05, 0x01, 0x15, 0x00, 0x09, 0x04, 0xA1, 0x01,
    0x85, 0x30, 0x05, 0x01, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0A, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x0A, 0x55, 0x00, 0x65, 0x00, 0x81, 0x02,
    0x05, 0x09, 0x19, 0x0B, 0x29, 0x0E, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x02, 0x81, 0x03, 0x0B, 0x01,
    0x00, 0x01, 0x00, 0xA1, 0x00, 0x0B, 0x30, 0x00,
    0x01, 0x00, 0x0B, 0x31, 0x00, 0x01, 0x00, 0x0B,
    0x32, 0x00, 0x01, 0x00, 0x0B, 0x35, 0x00, 0x01,
    0x00, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x04, 0x81, 0x02, 0xC0, 0x0B,
    0x39, 0x00, 0x01, 0x00, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x02, 0x05, 0x09, 0x19,
    0x0F, 0x29, 0x12, 0x15, 0x00, 0x25, 0x01, 0x75,
    0x01, 0x95, 0x04, 0x81, 0x02, 0x75, 0x08, 0x95,
    0x34, 0x81, 0x03, 0x06, 0x00, 0xFF, 0x85, 0x21,
    0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x03,
    0x85, 0x81, 0x09, 0x02, 0x75, 0x08, 0x95, 0x3F,
    0x81, 0x03, 0x85, 0x01, 0x09, 0x03, 0x75, 0x08,
    0x95, 0x3F, 0x91, 0x83, 0x85, 0x10, 0x09, 0x04,
    0x75, 0x08, 0x95, 0x3F, 0x91, 0x83, 0x85, 0x80,
    0x09, 0x05, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x83,
    0x85, 0x82, 0x09, 0x06, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0xC0
};
```

Add a compile-time check:

```c
_Static_assert(sizeof(kJoycontrolDescriptor) == 203, "Unexpected joycontrol descriptor length");
```

## Manual Test Steps

1. Flash firmware with only this descriptor change.
2. Clear bonds and virtual cable.
3. Reboot.
4. Put Switch into Change Grip/Order.
5. Capture logs.

## Pass Criteria

Switch reaches at least Pass Level 3:

```text
SET_REPORT, GET_REPORT, or INTR_DATA observed
```

## Strong Pass Criteria

Switch reaches Pass Level 4 and sends Joy-Con subcommands.

## Fail Criteria

Failure signature remains exactly:

```text
authentication/bonding happens
OPEN status non-success
GAP disconnect reason 0x13
no SET_REPORT / GET_REPORT / INTR_DATA
```

If this fails, the descriptor alone is not enough. Continue to T07.

---

# T05: Fix Device-Info Reply Length

## Purpose

This is probably not the discovery blocker, but it is an app-layer correctness issue.

Current code appears to build an 11-byte device-info reply. `joycontrol`-style device info should include two trailing `0x01` bytes after the 6-byte MAC, so this should be 12 bytes.

## Codex Tasks

Patch the builder:

```c
static void build_device_info_reply(uint8_t reply[12]) {
    uint8_t address_be[6];

    copy_bt_address_be(address_be);
    memset(reply, 0, 12u);

    reply[0] = 0x04u;
    reply[1] = 0x00u;
    reply[2] = SWITCH_CONTROLLER_TYPE_LEFT_JOYCON;
    reply[3] = 0x02u;
    memcpy(&reply[4], address_be, sizeof(address_be));
    reply[10] = 0x01u;
    reply[11] = 0x01u;
}
```

Patch the call site:

```c
case SWITCH_SUBCMD_GET_DEVICE_INFO:
    build_device_info_reply(reply);
    (void)send_subcommand_reply(SWITCH_ACK_DEVICE_INFO, subcommand, reply, 12u);
    break;
```

Add a unit-style test or compile-time helper if possible:

```text
GET_DEVICE_INFO reply length == 12
reply[2] == LEFT_JOYCON type
reply[3] == 0x02
reply[10] == 0x01
reply[11] == 0x01
```

## Manual Test Steps

Only meaningful if T04 reaches Pass Level 3 or higher.

1. Flash descriptor + device-info patch.
2. Clear bonds.
3. Pair on Switch.
4. Capture subcommand logs.

## Pass Criteria

Switch sends `GET_DEVICE_INFO`, firmware replies with a 12-byte payload, and Switch continues to subsequent subcommands.

## Fail Criteria

Switch disconnects immediately after `GET_DEVICE_INFO`.

---

# T06: Disable Auto-Reconnect During Test Mode

## Purpose

Even with clear-bond code, make test behavior deterministic.

Current behavior to guard against:

```c
if (param->register_app.in_use) {
    esp_bt_hid_device_connect(param->register_app.bd_addr);
}
```

## Codex Tasks

Add a compile-time switch:

```c
#define SWITCH_HID_TEST_DISABLE_AUTO_RECONNECT 1
```

Patch the `register_app.in_use` branch:

```c
if (param->register_app.in_use) {
    s_status.flags |= SB_STATUS_FLAG_VIRTUAL_CABLE;

#if SWITCH_HID_TEST_DISABLE_AUTO_RECONNECT
    ESP_LOGW(TAG, "Test mode: skipping esp_bt_hid_device_connect() for stored virtual cable");
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_CONNECT,
                 0xFEu,
                 0u);
#else
    esp_err_t connect_err = esp_bt_hid_device_connect(param->register_app.bd_addr);
    /* existing handling */
#endif
}
```

## Manual Test Steps

1. Pair with PC to create stale host data.
2. Reboot without clearing bonds.
3. Confirm no automatic reconnect occurs in test mode.
4. Clear bonds and run Switch test.

## Pass Criteria

Logs clearly show either:

```text
register_in_use=1
auto reconnect skipped
```

or, after clear:

```text
register_in_use=0
```

## Fail Criteria

Any automatic reconnect to a PC/stale host happens during a Switch test.

---

# T07: Full SDP Difference Test

## Purpose

If T04 still fails before HID traffic, stock `esp_hidd` is likely generating SDP/HID fields the Switch does not accept. This test produces a decision point: continue with stock `esp_hidd` or implement/customize SDP/L2CAP.

## Codex Tasks

Create:

```text
tools/compare_sdp.py
```

Inputs:

```text
docs/test-results/sdp-nina.txt
docs/reference/joycontrol-sdp-record-hid.xml
```

The script should report these fields:

```text
ServiceRecordHandle
ServiceClassIDList
ProtocolDescriptorList
BrowseGroupList
LanguageBaseAttributeIDList
BluetoothProfileDescriptorList
AdditionalProtocolDescriptorLists
ServiceName
ServiceDescription
ProviderName
HIDDeviceReleaseNumber
HIDParserVersion
HIDDeviceSubclass
HIDCountryCode
HIDVirtualCable
HIDReconnectInitiate
HIDDescriptorList length
HIDDescriptorList first 16 bytes
HIDDescriptorList full SHA256
HIDBatteryPower
HIDRemoteWake
HIDNormallyConnectable
```

## Manual Test Steps

1. Dump NINA SDP from Linux.
2. Obtain or store `joycontrol` reference SDP XML.
3. Run comparison script.
4. Save output:

```text
docs/test-results/sdp-diff.md
```

## Pass Criteria

Differences are limited to harmless fields such as service record handle or local address.

## Fail Criteria

Any of these differ:

```text
control PSM
interrupt PSM
HID subclass
virtual cable
reconnect initiate
descriptor length/hash
service name/description/provider
```

## Decision Rule

If T04 fails and T07 shows significant SDP differences, stop debugging subcommands. The next engineering task is to modify/bypass `esp_hidd` so the NINA advertises the same SDP/L2CAP identity as `joycontrol`.

---

# T08: Switch Functional Input Test

## Purpose

Confirm the port works beyond pairing.

## Codex Tasks

Add a simple deterministic test command over UART:

```text
press A for 200 ms
release A
press LEFT for 200 ms
release LEFT
press HOME disabled by default unless explicitly enabled
```

Add logs for every sent input report:

```text
report_id
timer
button bytes
left stick bytes
right stick bytes
input_report_mode
send result
```

## Manual Test Steps

1. Pair with Switch.
2. Wait until player lights are set or controller is accepted.
3. Send UART command to press a safe button.
4. Observe Switch UI.

## Pass Criteria

A visible UI action happens on Switch.

## Fail Criteria

Firmware sends reports but Switch UI does not react.

---

# Regression Tests Codex Should Add

## Descriptor Regression

Add a test or static assertion verifying:

```text
joycontrol descriptor length == 203
first bytes == 05 01 15 00 09 04 A1 01
contains report IDs 0x30, 0x21, 0x81, 0x01, 0x10, 0x80, 0x82
```

## Device Info Regression

Verify:

```text
reply length == 12
MAC bytes are copied into reply[4..9]
reply[10] == 0x01
reply[11] == 0x01
```

## Output Report Parser Regression

For report `0x01`, verify parser treats payload as:

```text
data[0]    packet counter
data[1..8] rumble
data[9]    subcommand
data[10..] args
```

## No-Reconnect Test Mode Regression

Verify that with:

```c
SWITCH_HID_TEST_DISABLE_AUTO_RECONNECT == 1
```

the code never calls:

```c
esp_bt_hid_device_connect();
```

inside the `register_app.in_use` branch.

---

# Recommended Patch Order

Codex should create separate commits in this order:

1. `test: add runtime identity and bond-state logging`
2. `test: add SDP dump/checklist tooling`
3. `hid: add joycontrol descriptor behind feature flag`
4. `hid: fix get-device-info reply length`
5. `hid: add test mode to disable stale virtual-cable reconnect`
6. `docs: record Switch pairing test results`

---

# Final Diagnosis Logic

Use this decision tree after running the tests:

```text
Does REGISTER_APP report in_use=1 after clear-bond?
  yes -> cleanup timing bug; fix startup sequence
  no  -> continue

Does PC SDP match joycontrol after descriptor replacement?
  no  -> stock esp_hidd SDP generation differs; inspect/patch SDP layer
  yes -> continue

Does Switch send SET_REPORT / GET_REPORT / INTR_DATA?
  no  -> blocker is still below app-layer Joy-Con protocol
  yes -> continue

Does Switch send GET_DEVICE_INFO then disconnect?
  yes -> validate 12-byte device info and ACK format
  no  -> continue

Does Switch send SPI_FLASH_READ then disconnect?
  yes -> validate SPI reply address/length/calibration bytes
  no  -> continue

Does Switch set player lights?
  yes -> pairing handshake likely complete; debug input reports
```

---

# Expected Outcome

The most important experiment is **T04**.

If the byte-for-byte `joycontrol` descriptor causes the Switch to start sending HID report callbacks, then the main issue was the current descriptor.

If T04 still fails with no HID report callbacks, the evidence points to stock `esp_hidd`, generated SDP, or L2CAP behavior rather than Joy-Con subcommand code.

