#!/usr/bin/env python3
"""Golden checks for Switch HID profile consistency.

This is intentionally source-level: it can run on a host without ESP-IDF while
still catching regressions in the firmware profile identity and 0x30 report
mapping that are hard to unit-test outside the NINA firmware.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "nina_firmware" / "main" / "switch_hid.c"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_text(source: str, text: str, message: str) -> None:
    require(text in source, message)


def require_regex(source: str, pattern: str, message: str) -> None:
    require(re.search(pattern, source, re.DOTALL) is not None, message)


def check_identity(source: str) -> None:
    for text in (
        "SWITCH_PROFILE_LEFT_JOYCON = SB_CONTROLLER_MODE_LEFT_JOYCON",
        "SWITCH_PROFILE_RIGHT_JOYCON = SB_CONTROLLER_MODE_RIGHT_JOYCON",
        "SWITCH_PROFILE_PRO_CONTROLLER = SB_CONTROLLER_MODE_PRO_CONTROLLER",
        'return "Joy-Con (L)"',
        'return "Joy-Con (R)"',
        'return "Pro Controller"',
        "return SWITCH_CONTROLLER_TYPE_LEFT_JOYCON",
        "return SWITCH_CONTROLLER_TYPE_RIGHT_JOYCON",
        "return SWITCH_CONTROLLER_TYPE_PRO",
        "reply[2] = switch_controller_type();",
        "reply[10] = 0x01u;",
        "reply[11] = 0x01u;",
        "uint8_t device_type[1] = {switch_controller_type()};",
    ):
        require_text(source, text, f"missing identity invariant: {text}")


def check_common_report_buttons(source: str) -> None:
    expected = {
        "SB_BTN_RJC_Y": ("right_buttons", "0x01u"),
        "SB_BTN_RJC_X": ("right_buttons", "0x02u"),
        "SB_BTN_RJC_B": ("right_buttons", "0x04u"),
        "SB_BTN_RJC_A": ("right_buttons", "0x08u"),
        "SB_BTN_RJC_SR": ("right_buttons", "0x10u"),
        "SB_BTN_RJC_SL": ("right_buttons", "0x20u"),
        "SB_BTN_RJC_R": ("right_buttons", "0x40u"),
        "SB_BTN_RJC_ZR": ("right_buttons", "0x80u"),
        "SB_BTN_LJC_MINUS": ("shared_buttons", "0x01u"),
        "SB_BTN_RJC_PLUS": ("shared_buttons", "0x02u"),
        "SB_BTN_RJC_STICK": ("shared_buttons", "0x04u"),
        "SB_BTN_LJC_STICK": ("shared_buttons", "0x08u"),
        "SB_BTN_RJC_HOME": ("shared_buttons", "0x10u"),
        "SB_BTN_LJC_CAPTURE": ("shared_buttons", "0x20u"),
        "SB_BTN_LJC_DOWN": ("left_buttons", "0x01u"),
        "SB_BTN_LJC_UP": ("left_buttons", "0x02u"),
        "SB_BTN_LJC_RIGHT": ("left_buttons", "0x04u"),
        "SB_BTN_LJC_LEFT": ("left_buttons", "0x08u"),
        "SB_BTN_LJC_SR": ("left_buttons", "0x10u"),
        "SB_BTN_LJC_SL": ("left_buttons", "0x20u"),
        "SB_BTN_LJC_L": ("left_buttons", "0x40u"),
        "SB_BTN_LJC_ZL": ("left_buttons", "0x80u"),
    }

    for button, (target, bit) in expected.items():
        require_regex(
            source,
            rf"{button}\) != 0u\) \{{\s*{target} \|= {bit};",
            f"{button} must map to {target} {bit}",
        )


def check_profile_rules(source: str) -> None:
    for text in (
        "const bool reports_left = switch_profile_has_left_side();",
        "const bool reports_right = switch_profile_has_right_side();",
        "buttons |= hat_to_profile_dpad_bits(s_state.hat);",
        "pack_center_stick(&report[5]);",
        "pack_center_stick(&report[8]);",
        "SWITCH_SIMPLE_STICK_CENTER",
        "report[2] = reports_left ?",
        "SWITCH_TRIGGER_ELAPSED_REPLY_BYTES 14u",
        "SWITCH_SUBCMD_SET_NFC_IR_MCU_CONFIG 0x21u",
        "SWITCH_SUBCMD_SET_NFC_IR_MCU_STATE 0x22u",
    ):
        require_text(source, text, f"missing profile/report rule: {text}")

    require_regex(
        source,
        r"s_controller_profile == SWITCH_PROFILE_PRO_CONTROLLER.*reply\[0\] = elapsed_lo;"
        r".*reply\[1\] = elapsed_hi;.*reply\[2\] = elapsed_lo;.*reply\[3\] = elapsed_hi;",
        "Pro Controller trigger elapsed must populate L/R fields",
    )
    require_regex(
        source,
        r"reply\[8\] = elapsed_lo;.*reply\[9\] = elapsed_hi;"
        r".*reply\[10\] = elapsed_lo;.*reply\[11\] = elapsed_hi;",
        "Joy-Con trigger elapsed must populate SL/SR fields",
    )
    require_regex(
        source,
        r"SWITCH_SUBCMD_SET_NFC_IR_MCU_CONFIG:.*build_nfc_ir_mcu_config_reply\(reply\);"
        r".*SWITCH_ACK_NFC_IR_MCU_CONFIG.*SWITCH_NFC_IR_MCU_CONFIG_REPLY_BYTES",
        "NFC/IR MCU config subcommand must reply with ACK 0xA0 and payload",
    )
    require_regex(
        source,
        r"SWITCH_SUBCMD_SET_NFC_IR_MCU_STATE:.*send_subcommand_reply\(SWITCH_ACK_SIMPLE",
        "NFC/IR MCU state subcommand must be explicitly acknowledged",
    )


def check_runtime_and_logging(source: str) -> None:
    for text in (
        "switch_hid_set_controller_mode(sb_controller_mode_t mode)",
        "switch_profile_from_mode(mode, &profile)",
        "s_controller_profile = profile;",
        "esp_bt_dev_set_device_name(switch_controller_name())",
        "HID_REPORT_SHAPE report_id=0x%02X len=%u expected=%u hidp_prefix=none",
        "COMMON_REPORT profile=%u type=0x%02X mode=0x%02X bytes=",
    ):
        require_text(source, text, f"missing runtime/logging invariant: {text}")


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8")
    check_identity(source)
    check_common_report_buttons(source)
    check_profile_rules(source)
    check_runtime_and_logging(source)
    print("switch_hid profile golden checks passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"switch_hid profile golden check failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
