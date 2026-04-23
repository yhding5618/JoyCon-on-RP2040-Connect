#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


DEFAULT_REFERENCE = Path("docs/reference/joycontrol-sdp-record-hid.xml")
DEFAULT_OUTPUT = Path("docs/test-results/sdp-diff.md")


FIELDS = [
    ("ServiceRecordHandle", "service_record_handle", "report"),
    ("ServiceClassIDList", "service_class_ids", "compare"),
    ("Control PSM", "control_psm", "compare"),
    ("BrowseGroupList", "browse_group_ids", "report"),
    ("LanguageBaseAttributeIDList", "language_base", "compare"),
    ("BluetoothProfileDescriptorList", "profile_descriptor", "compare"),
    ("Interrupt PSM", "interrupt_psm", "compare"),
    ("ServiceName", "service_name", "compare"),
    ("ServiceDescription", "service_description", "compare"),
    ("ProviderName", "provider_name", "compare"),
    ("HIDDeviceReleaseNumber", "hid_device_release_number", "compare"),
    ("HIDParserVersion", "hid_parser_version", "compare"),
    ("HIDDeviceSubclass", "hid_device_subclass", "compare"),
    ("HIDCountryCode", "hid_country_code", "compare"),
    ("HIDVirtualCable", "hid_virtual_cable", "compare"),
    ("HIDReconnectInitiate", "hid_reconnect_initiate", "compare"),
    ("HIDDescriptorList length", "hid_descriptor_len", "compare"),
    ("HIDDescriptorList first 16 bytes", "hid_descriptor_first16", "compare"),
    ("HIDDescriptorList SHA256", "hid_descriptor_sha256", "compare"),
    ("HIDBatteryPower", "hid_battery_power", "compare"),
    ("HIDRemoteWake", "hid_remote_wake", "compare"),
    ("HIDNormallyConnectable", "hid_normally_connectable", "compare"),
    ("HIDBootDevice", "hid_boot_device", "compare"),
]


def extract_record_xml(text: str) -> str:
    start = text.find("<record")
    end = text.rfind("</record>")
    if start < 0 or end < 0:
        raise ValueError("No <record>...</record> XML block found in SDP input")
    return text[start : end + len("</record>")]


def load_record_from_text(text: str) -> ET.Element:
    if "<record" in text and "</record>" in text:
        text = extract_record_xml(text)
    return ET.fromstring(text)


def attr(record: ET.Element, attr_id: str) -> ET.Element | None:
    wanted = int(attr_id, 16)
    for element in record.findall("attribute"):
        raw = element.attrib.get("id", "")
        try:
            if int(raw, 16) == wanted:
                return element
        except ValueError:
            continue
    return None


def values_of(element: ET.Element | None, tag: str) -> list[str]:
    if element is None:
        return []
    return [child.attrib["value"] for child in element.iter(tag) if "value" in child.attrib]


def int_values(element: ET.Element | None, tags: tuple[str, ...] = ("uint8", "uint16", "uint32")) -> list[int]:
    if element is None:
        return []
    values: list[int] = []
    for child in element.iter():
        if child.tag in tags and "value" in child.attrib:
            values.append(int(child.attrib["value"], 16))
    return values


def one_int(record: ET.Element, attr_id: str) -> int | None:
    values = int_values(attr(record, attr_id))
    return values[0] if values else None


def one_text(record: ET.Element, attr_id: str) -> str | None:
    values = values_of(attr(record, attr_id), "text")
    if not values:
        return None
    return values[0].rstrip(" \x00")


def one_bool(record: ET.Element, attr_id: str) -> bool | None:
    values = values_of(attr(record, attr_id), "boolean")
    if not values:
        return None
    return values[0].lower() == "true"


def load_record(path: Path) -> ET.Element:
    return load_record_from_text(path.read_text(encoding="utf-8", errors="replace"))


def looks_like_hex_blob(value: str) -> bool:
    stripped = value.strip()
    return len(stripped) >= 32 and len(stripped) % 2 == 0 and re.fullmatch(r"[0-9a-fA-F]+", stripped) is not None


def descriptor_bytes_from_xml(record: ET.Element) -> bytes:
    element = attr(record, "0x0206")
    if element is None:
        return b""
    for text in element.iter("text"):
        value = text.attrib.get("value", "")
        if text.attrib.get("encoding") == "hex" and value:
            return bytes.fromhex(value)
        if looks_like_hex_blob(value):
            return bytes.fromhex(value)
    return b""


def descriptor_bytes_from_raw(raw_text: str) -> bytes:
    xml_match = re.search(
        r'(?is)<attribute\s+id=["\']0x0206["\']>(.*?)</attribute>',
        raw_text,
    )
    if xml_match is not None:
        for value in re.findall(r'value=["\']([0-9a-fA-F]+)["\']', xml_match.group(1)):
            if looks_like_hex_blob(value):
                return bytes.fromhex(value)

    section_match = re.search(
        r"(?is)(?:Attribute Identifier\s*:\s*0x206|Attribute\s+0x0206)(.*?)(?:Attribute Identifier\s*:\s*0x207|Attribute\s+0x0207|$)",
        raw_text,
    )
    if section_match is None:
        return b""

    data_match = re.search(
        r"(?is)Data\s*:?\s*([0-9a-fA-F]{2}(?:\s+[0-9a-fA-F]{2})+)",
        section_match.group(1),
    )
    if data_match is None:
        return descriptor_bytes_from_global_text(raw_text)

    return bytes.fromhex(data_match.group(1))


def descriptor_bytes_from_global_text(raw_text: str) -> bytes:
    candidates: list[bytes] = []

    for value in re.findall(r'value=["\']([0-9a-fA-F]{128,})["\']', raw_text):
        if len(value) % 2 == 0:
            candidates.append(bytes.fromhex(value))

    spaced_hex_pattern = r"(?is)([0-9a-fA-F]{2}(?:\s+[0-9a-fA-F]{2}){63,})"
    for value in re.findall(spaced_hex_pattern, raw_text):
        try:
            candidates.append(bytes.fromhex(value))
        except ValueError:
            continue

    descriptor_like = [
        candidate
        for candidate in candidates
        if len(candidate) >= 64 and candidate.startswith(b"\x05\x01")
    ]
    if not descriptor_like:
        return b""
    return max(descriptor_like, key=len)


def descriptor_bytes(record: ET.Element, raw_text: str) -> bytes:
    descriptor = descriptor_bytes_from_xml(record)
    if descriptor:
        return descriptor
    descriptor = descriptor_bytes_from_raw(raw_text)
    if descriptor:
        return descriptor
    return descriptor_bytes_from_global_text(raw_text)


def profile_descriptor(record: ET.Element) -> str:
    element = attr(record, "0x0009")
    uuids = values_of(element, "uuid")
    ints = int_values(element, ("uint16",))
    if not uuids and not ints:
        return "missing"
    return ",".join(uuids + [hex(value) for value in ints])


def format_value(value: object) -> str:
    if value is None:
        return "missing"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return f"0x{value:04x}" if value > 0xFF else f"0x{value:02x}"
    if isinstance(value, bytes):
        return value.hex(" ")
    if isinstance(value, (list, tuple)):
        return ", ".join(format_value(item) for item in value)
    return str(value)


def summarize(record: ET.Element, raw_text: str) -> dict[str, object]:
    descriptor = descriptor_bytes(record, raw_text)
    service_class_ids = values_of(attr(record, "0x0001"), "uuid")
    browse_group_ids = values_of(attr(record, "0x0005"), "uuid")
    language_base = tuple(int_values(attr(record, "0x0006"), ("uint16",)))
    control_psms = int_values(attr(record, "0x0004"), ("uint16",))
    interrupt_psms = int_values(attr(record, "0x000d"), ("uint16",))

    return {
        "service_record_handle": one_int(record, "0x0000"),
        "service_class_ids": tuple(service_class_ids),
        "control_psm": control_psms[0] if control_psms else None,
        "browse_group_ids": tuple(browse_group_ids),
        "language_base": language_base,
        "profile_descriptor": profile_descriptor(record),
        "interrupt_psm": interrupt_psms[0] if interrupt_psms else None,
        "service_name": one_text(record, "0x0100"),
        "service_description": one_text(record, "0x0101"),
        "provider_name": one_text(record, "0x0102"),
        "hid_device_release_number": one_int(record, "0x0200"),
        "hid_parser_version": one_int(record, "0x0201"),
        "hid_device_subclass": one_int(record, "0x0202"),
        "hid_country_code": one_int(record, "0x0203"),
        "hid_virtual_cable": one_bool(record, "0x0204"),
        "hid_reconnect_initiate": one_bool(record, "0x0205"),
        "hid_descriptor_len": len(descriptor),
        "hid_descriptor_first16": descriptor[:16],
        "hid_descriptor_sha256": hashlib.sha256(descriptor).hexdigest() if descriptor else "missing",
        "hid_battery_power": one_bool(record, "0x0209"),
        "hid_remote_wake": one_bool(record, "0x020a"),
        "hid_normally_connectable": one_bool(record, "0x020d"),
        "hid_boot_device": one_bool(record, "0x020e"),
    }


def build_report(nina_path: Path, reference_path: Path) -> tuple[str, int]:
    nina_text = nina_path.read_text(encoding="utf-8", errors="replace")
    reference_text = reference_path.read_text(encoding="utf-8", errors="replace")
    nina = summarize(load_record_from_text(nina_text), nina_text)
    reference = summarize(load_record_from_text(reference_text), reference_text)
    rows: list[str] = []
    mismatch_count = 0

    for label, key, mode in FIELDS:
        nina_value = nina.get(key)
        reference_value = reference.get(key)
        if mode == "report":
            result = "reported"
        elif nina_value == reference_value:
            result = "match"
        else:
            result = "diff"
            mismatch_count += 1
        rows.append(
            f"| {label} | {format_value(nina_value)} | {format_value(reference_value)} | {result} |"
        )

    status = "match" if mismatch_count == 0 else f"{mismatch_count} difference(s)"
    report = [
        "# SDP Difference",
        "",
        f"- NINA SDP: `{nina_path}`",
        f"- Reference SDP: `{reference_path}`",
        f"- Summary: {status}",
        "",
        "| Field | NINA | Reference | Result |",
        "|---|---|---|---|",
        *rows,
        "",
    ]

    if mismatch_count == 0:
        report.append("Decision: SDP matches the reference fields checked by this script.")
    else:
        report.append(
            "Decision: SDP still differs from the reference; inspect `diff` rows before moving back to app-layer subcommands."
        )
        if nina.get("hid_descriptor_len") == 0:
            report.append(
                "Note: no HID descriptor bytes were found in the NINA input file. Re-run `tools/dump_sdp.sh` and confirm the dump contains attribute `0x0206` / `DescriptorList`."
            )

    return "\n".join(report) + "\n", mismatch_count


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare a NINA SDP dump against the Joy-Con HID SDP reference.")
    parser.add_argument("nina_sdp", type=Path, help="Path to sdptool output containing a records --xml block.")
    parser.add_argument("--reference", type=Path, default=DEFAULT_REFERENCE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    report, mismatch_count = build_report(args.nina_sdp, args.reference)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(report, encoding="utf-8")
    print(report, end="")
    return 1 if mismatch_count else 0


if __name__ == "__main__":
    sys.exit(main())
