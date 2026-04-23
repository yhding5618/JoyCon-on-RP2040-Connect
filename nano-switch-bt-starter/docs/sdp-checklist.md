# SDP Checklist

Use this checklist for T03 and T07 when comparing the NINA Classic Bluetooth HID SDP record against the `joycontrol` Joy-Con profile.

## Expected HID Identity

| Field | Expected |
|---|---|
| Service Class UUID | `0x1124` / Human Interface Device Service |
| Protocol Descriptor List | includes L2CAP PSM `0x0011` and HIDP |
| Additional Protocol Descriptor List | includes L2CAP PSM `0x0013` and HIDP |
| Service name | `Wireless Gamepad` |
| Service description | `Gamepad` |
| Provider name | `Nintendo` |
| HIDDeviceReleaseNumber | `0x0100` |
| HIDParserVersion | `0x0111` |
| HIDDeviceSubclass | `0x08` |
| HIDCountryCode | `0x00` |
| HIDVirtualCable | `true` |
| HIDReconnectInitiate | `true` |
| HIDDescriptorList prefix | begins with the active firmware descriptor |
| HIDDescriptorList length | T02 current descriptor is `170` bytes / `0x00AA`; T04 joycontrol descriptor should be `203` bytes |

## T03 Result

Bluetooth address tested: `D4:F0:57:4D:4E:32`

| Field | Match? | Observed / Notes |
|---|---|---|
| Service Class UUID `0x1124` | yes | `"Human Interface Device" (0x1124)` |
| Control PSM `0x0011` | yes | `Protocol Descriptor List` includes L2CAP `PSM: 17` and HIDP |
| Interrupt PSM `0x0013` | yes | `AdditionalProtocolDescriptorLists` includes L2CAP `0x0013` and HIDP |
| Service name `Wireless Gamepad` | yes | `Service Name: Wireless Gamepad` |
| Description `Gamepad` | yes | `Service Description: Gamepad` |
| Provider `Nintendo` | yes | `Service Provider: Nintendo` |
| HIDDeviceReleaseNumber `0x0100` | yes | Attribute `0x0200` is `0x0100` |
| HIDParserVersion `0x0111` | yes | Attribute `0x0201` is `0x0111` |
| HIDDeviceSubclass `0x08` | yes | Attribute `0x0202` is `0x08` |
| HIDCountryCode `0x00` | no | Attribute `0x0203` is `0x21` |
| HIDVirtualCable `true` | yes | Attribute `0x0204` is `true` |
| HIDReconnectInitiate `true` | yes | Attribute `0x0205` is `true` |
| HIDDescriptorList length | no | T03 descriptor is 170 bytes, not the 203-byte joycontrol descriptor |
| HIDDescriptorList first 16 bytes | no | T03 starts `05 01 09 05 A1 01 06 01 FF 85 21 09 21 75 08 95`; joycontrol starts `05 01 15 00 09 04 A1 01 85 30 05 01 05 09 19 01` |

## Notes

`sdptool browse` formatting varies by BlueZ version. If a field is present under a friendly label instead of a raw attribute ID, count it as a match and paste the observed line in the notes column.

The fuller T03 dump confirms the stock `esp_hidd` SDP has the expected control and interrupt PSMs plus HID virtual-cable fields. Remaining mismatches before T04 are HID country code `0x21` and the active 170-byte descriptor.

## T07 Finding

With the 203-byte Joy-Con descriptor active, ESP-IDF v4.4.8 can silently fail to publish HID SDP attribute `0x0206` because the stock `SDP_MAX_PAD_LEN` is only `300` bytes. The descriptor attribute itself is about `211` bytes after SDP wrapping, and the other HID attributes already occupy enough of the same per-record pad that `SDP_AddAttribute` rejects it.

Patch ESP-IDF with:

```powershell
pwsh host_tools\patch_idf_hid_switch_compat.ps1 -IdfPath C:\Espressif\frameworks\esp-idf-v4.4.8
```

Expected post-patch T07 result:

| Field | Expected |
|---|---|
| HIDCountryCode | `0x00` |
| HIDDescriptorList length | `0xCB` / 203 bytes |
| HIDDescriptorList prefix | `05 01 15 00 09 04 A1 01 85 30 05 01 05 09 19 01` |
