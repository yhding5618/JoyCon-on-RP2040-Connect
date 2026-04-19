#!/usr/bin/env python3
import argparse
import struct
import sys
import time

import serial

MAGIC0 = 0x53
MAGIC1 = 0x42
VERSION = 0x01
MSG_GET_STATUS = 0x02
MSG_STATUS = 0x03
MSG_GET_EVENTS = 0x04
MSG_EVENTS = 0x05
MSG_SET_STATE = 0x10
MSG_VC_UNPLUG = 0x11
MSG_CLEAR_BONDS = 0x12
HEADER_SIZE = 8
EVENT_PAYLOAD_SIZE = 48

FLAG_BRIDGE_READY = 1 << 0
FLAG_BT_READY = 1 << 1
FLAG_HID_READY = 1 << 2
FLAG_CONNECTED = 1 << 3
FLAG_VIRTUAL_CABLE = 1 << 4

EVENT_SOURCE_NAMES = {
    0x00: "NONE",
    0x01: "HID_CALLBACK",
    0x02: "GAP_CALLBACK",
    0x03: "HID_API",
    0x04: "BRIDGE",
}

HID_EVENT_NAMES = {
    0x00: "INIT",
    0x01: "DEINIT",
    0x02: "REGISTER_APP",
    0x03: "UNREGISTER_APP",
    0x04: "OPEN",
    0x05: "CLOSE",
    0x06: "SEND_REPORT",
    0x07: "REPORT_ERR",
    0x08: "GET_REPORT",
    0x09: "SET_REPORT",
    0x0A: "SET_PROTOCOL",
    0x0B: "INTR_DATA",
    0x0C: "VC_UNPLUG",
    0x0D: "API_ERR",
}

GAP_EVENT_NAMES = {
    0x00: "DISC_RES",
    0x01: "DISC_STATE_CHANGED",
    0x02: "RMT_SRVCS",
    0x03: "RMT_SRVC_REC",
    0x04: "AUTH_CMPL",
    0x05: "PIN_REQ",
    0x06: "CFM_REQ",
    0x07: "KEY_NOTIF",
    0x08: "KEY_REQ",
    0x09: "READ_RSSI_DELTA",
    0x0A: "CONFIG_EIR_DATA",
    0x0B: "SET_AFH_CHANNELS",
    0x0C: "READ_REMOTE_NAME",
    0x0D: "MODE_CHG",
    0x0E: "REMOVE_BOND_DEV_COMPLETE",
    0x0F: "QOS_CMPL",
    0x10: "ACL_CONN_CMPL",
    0x11: "ACL_DISCONN_CMPL",
}

HID_API_EVENT_NAMES = {
    0x01: "SEND_REPORT",
    0x02: "REGISTER_APP",
    0x03: "CONNECT",
    0x04: "VC_UNPLUG",
    0x05: "CLEAR_BONDS_BEGIN",
    0x06: "CLEAR_BONDS_DONE",
    0x07: "REMOVE_BOND",
}

BRIDGE_EVENT_NAMES = {
    MSG_GET_EVENTS: "GET_EVENTS",
    MSG_VC_UNPLUG: "VIRTUAL_CABLE_UNPLUG",
    MSG_CLEAR_BONDS: "CLEAR_BONDS",
}


def crc16_ccitt_seed(seed: int, data: bytes) -> int:
    crc = seed & 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(message_type: int, sequence: int, payload: bytes) -> bytes:
    metadata = struct.pack("<BBBB", VERSION, message_type, sequence & 0xFF, len(payload))
    crc = crc16_ccitt_seed(0xFFFF, metadata)
    if payload:
        crc = crc16_ccitt_seed(crc, payload)

    header = struct.pack(
        "<BBBBBBH",
        MAGIC0,
        MAGIC1,
        VERSION,
        message_type,
        sequence & 0xFF,
        len(payload),
        crc,
    )
    return header + payload


def parse_frames(data: bytes) -> list[bytes]:
    frames: list[bytes] = []
    offset = 0

    while offset + HEADER_SIZE <= len(data):
        if data[offset] != MAGIC0 or data[offset + 1] != MAGIC1:
            offset += 1
            continue

        payload_len = data[offset + 5]
        frame_len = HEADER_SIZE + payload_len
        if offset + frame_len > len(data):
            break

        frames.append(data[offset : offset + frame_len])
        offset += frame_len

    return frames


def flag_names(flags: int) -> list[str]:
    names: list[str] = []
    if flags & FLAG_BRIDGE_READY:
        names.append("BRIDGE_READY")
    if flags & FLAG_BT_READY:
        names.append("BT_READY")
    if flags & FLAG_HID_READY:
        names.append("HID_READY")
    if flags & FLAG_CONNECTED:
        names.append("CONNECTED")
    if flags & FLAG_VIRTUAL_CABLE:
        names.append("VIRTUAL_CABLE")
    return names


def describe_event(source: int, event: int, arg0: int, arg1: int) -> tuple[str, str]:
    if source == 0x01:
        name = HID_EVENT_NAMES.get(event, f"0x{event:02x}")
        if event in (0x00, 0x02, 0x07):
            return name, f"status=0x{arg0:02x}"
        if event in (0x04, 0x05, 0x0C):
            return name, f"status=0x{arg0:02x} conn=0x{arg1:02x}"
        if event == 0x06:
            return name, f"status=0x{arg0:02x} report_id=0x{arg1:02x}"
        if event in (0x08, 0x09):
            return name, f"report_id=0x{arg0:02x} report_type=0x{arg1:02x}"
        if event == 0x0A:
            return name, f"protocol=0x{arg0:02x}"
        if event == 0x0B:
            return name, f"report_id=0x{arg0:02x} len=0x{arg1:02x}"
        return name, f"arg0=0x{arg0:02x} arg1=0x{arg1:02x}"

    if source == 0x02:
        name = GAP_EVENT_NAMES.get(event, f"0x{event:02x}")
        if event == 0x04:
            return name, f"status=0x{arg0:02x}"
        if event == 0x05:
            return name, f"pin_digits={arg0}"
        if event == 0x0A:
            return name, f"status=0x{arg0:02x}"
        if event == 0x0D:
            return name, f"mode=0x{arg0:02x}"
        if event == 0x10:
            return name, f"status=0x{arg0:02x}"
        if event == 0x11:
            return name, f"reason=0x{arg1:02x}"
        return name, f"arg0=0x{arg0:02x} arg1=0x{arg1:02x}"

    if source == 0x03:
        name = HID_API_EVENT_NAMES.get(event, f"0x{event:02x}")
        if event == 0x01:
            return name, f"status=0x{arg0:02x} report_id=0x{arg1:02x}"
        if event in (0x02, 0x03, 0x04):
            return name, f"status=0x{arg0:02x}"
        if event == 0x05:
            return name, f"bonds_before={arg0}"
        if event == 0x06:
            return name, f"status=0x{arg0:02x} bonds_after={arg1}"
        if event == 0x07:
            return name, f"status=0x{arg0:02x} index={arg1}"
        return name, f"arg0=0x{arg0:02x} arg1=0x{arg1:02x}"

    if source == 0x04:
        name = BRIDGE_EVENT_NAMES.get(event, f"0x{event:02x}")
        return name, f"arg0=0x{arg0:02x} arg1=0x{arg1:02x}"

    return f"0x{event:02x}", f"arg0=0x{arg0:02x} arg1=0x{arg1:02x}"


def decode_status_frame(frame: bytes) -> str | None:
    if len(frame) not in (HEADER_SIZE + 8, HEADER_SIZE + 12, HEADER_SIZE + 16):
        return None
    if frame[3] != MSG_STATUS:
        return None

    payload = frame[HEADER_SIZE:]
    if len(payload) == 8:
        (
            flags,
            protocol_mode,
            input_report_mode,
            battery_level,
            last_host_report_id,
            last_error,
            last_subcommand,
            last_hid_event,
        ) = struct.unpack("<BBBBBBBB", payload)
        last_gap_event = 0
        last_gap_status = 0
        last_gap_reason = 0
        bond_device_count = 0
        last_hid_status = 0
        last_hid_conn_status = 0
        last_hid_report_type = 0
        last_hid_report_id = 0
    elif len(payload) == 12:
        (
            flags,
            protocol_mode,
            input_report_mode,
            battery_level,
            last_host_report_id,
            last_error,
            last_subcommand,
            last_hid_event,
            last_gap_event,
            last_gap_status,
            last_gap_reason,
            bond_device_count,
        ) = struct.unpack("<BBBBBBBBBBBB", payload)
        last_hid_status = 0
        last_hid_conn_status = 0
        last_hid_report_type = 0
        last_hid_report_id = 0
    else:
        (
            flags,
            protocol_mode,
            input_report_mode,
            battery_level,
            last_host_report_id,
            last_error,
            last_subcommand,
            last_hid_event,
            last_hid_status,
            last_hid_conn_status,
            last_hid_report_type,
            last_hid_report_id,
            last_gap_event,
            last_gap_status,
            last_gap_reason,
            bond_device_count,
        ) = struct.unpack("<BBBBBBBBBBBBBBBB", payload)

    flags_text = "|".join(flag_names(flags)) or "none"
    hid_event_name = HID_EVENT_NAMES.get(last_hid_event, f"0x{last_hid_event:02x}")
    gap_event_name = GAP_EVENT_NAMES.get(last_gap_event, f"0x{last_gap_event:02x}")

    summary = (
        f"STATUS flags=0x{flags:02x} ({flags_text}) "
        f"protocol=0x{protocol_mode:02x} "
        f"input_mode=0x{input_report_mode:02x} "
        f"battery={battery_level} "
        f"last_report_id=0x{last_host_report_id:02x} "
        f"last_error=0x{last_error:02x} "
        f"last_subcommand=0x{last_subcommand:02x} "
        f"last_hid_event={hid_event_name}"
    )

    if len(payload) >= 12:
        if len(payload) == 16:
            summary += (
                f" last_hid_status=0x{last_hid_status:02x} "
                f"last_hid_conn_status=0x{last_hid_conn_status:02x} "
                f"last_hid_report_type=0x{last_hid_report_type:02x} "
                f"last_hid_report_id=0x{last_hid_report_id:02x}"
            )
        summary += (
            f" last_gap_event={gap_event_name} "
            f"last_gap_status=0x{last_gap_status:02x} "
            f"last_gap_reason=0x{last_gap_reason:02x} "
            f"bonds={bond_device_count}"
        )

    return summary


def decode_events_frame(frame: bytes) -> list[str] | None:
    if len(frame) != HEADER_SIZE + EVENT_PAYLOAD_SIZE:
        return None
    if frame[3] != MSG_EVENTS:
        return None

    payload = frame[HEADER_SIZE:]
    (
        first_sequence,
        chunk_index,
        chunk_count,
        entry_count,
        total_entries,
        overflowed,
        _reserved,
    ) = struct.unpack("<HBBBBBB", payload[:8])

    lines = [
        (
            f"EVENTS chunk={chunk_index + 1}/{chunk_count} "
            f"entries={entry_count} total={total_entries} "
            f"overflowed={'yes' if overflowed else 'no'} "
            f"first_seq={first_sequence}"
        )
    ]

    offset = 8
    for i in range(min(entry_count, 5)):
        timestamp_ms, source, event, arg0, arg1 = struct.unpack(
            "<IBBBB", payload[offset : offset + 8]
        )
        offset += 8
        source_name = EVENT_SOURCE_NAMES.get(source, f"0x{source:02x}")
        event_name, details = describe_event(source, event, arg0, arg1)
        sequence = (first_sequence + i) & 0xFFFF
        lines.append(
            f"EVENT seq={sequence} t={timestamp_ms}ms source={source_name} "
            f"event={event_name} {details}"
        )

    return lines


def state_payload(args: argparse.Namespace) -> bytes:
    return struct.pack(
        "<IhhhhBBBB",
        args.buttons,
        args.lx,
        args.ly,
        args.rx,
        args.ry,
        args.hat,
        args.misc,
        args.battery,
        0,
    )


def read_reply(port: serial.Serial, total_wait_s: float = 1.0) -> None:
    deadline = time.time() + total_wait_s
    chunks = []

    while time.time() < deadline:
        waiting = port.in_waiting
        if waiting:
            chunks.append(port.read(waiting))
            time.sleep(0.05)
            continue
        time.sleep(0.05)

    if chunks:
        data = b"".join(chunks)
        print(data.hex(" "))
        for frame in parse_frames(data):
            decoded = decode_status_frame(frame)
            if decoded is not None:
                print(decoded)
                continue

            event_lines = decode_events_frame(frame)
            if event_lines is not None:
                for line in event_lines:
                    print(line)
    else:
        print("No reply received.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send bridge protocol packets to the RP2040 USB CDC port."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    state = subparsers.add_parser("state", help="Send a controller state frame.")
    state.add_argument("port", help="Serial port, for example COM5.")
    state.add_argument("--baud", type=int, default=115200)
    state.add_argument("--buttons", type=lambda value: int(value, 0), default=0)
    state.add_argument("--lx", type=int, default=0)
    state.add_argument("--ly", type=int, default=0)
    state.add_argument("--rx", type=int, default=0)
    state.add_argument("--ry", type=int, default=0)
    state.add_argument("--hat", type=int, default=8)
    state.add_argument("--misc", type=int, default=0)
    state.add_argument("--battery", type=int, default=8)

    status = subparsers.add_parser("status", help="Request a status frame.")
    status.add_argument("port", help="Serial port, for example COM5.")
    status.add_argument("--baud", type=int, default=115200)

    dump_events = subparsers.add_parser("dump-events", help="Request the recent NINA event log.")
    dump_events.add_argument("port", help="Serial port, for example COM5.")
    dump_events.add_argument("--baud", type=int, default=115200)

    unplug = subparsers.add_parser("unplug", help="Request a virtual cable unplug.")
    unplug.add_argument("port", help="Serial port, for example COM5.")
    unplug.add_argument("--baud", type=int, default=115200)

    clear_bonds = subparsers.add_parser("clear-bonds", help="Remove all stored Bluetooth bonds.")
    clear_bonds.add_argument("port", help="Serial port, for example COM5.")
    clear_bonds.add_argument("--baud", type=int, default=115200)

    args = parser.parse_args()

    if args.command == "state":
        message_type = MSG_SET_STATE
        payload = state_payload(args)
    elif args.command == "status":
        message_type = MSG_GET_STATUS
        payload = b""
    elif args.command == "dump-events":
        message_type = MSG_GET_EVENTS
        payload = b""
    elif args.command == "clear-bonds":
        message_type = MSG_CLEAR_BONDS
        payload = b""
    else:
        message_type = MSG_VC_UNPLUG
        payload = b""

    frame = build_frame(message_type, 1, payload)

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        time.sleep(1.0)
        port.reset_input_buffer()
        port.write(frame)
        port.flush()
        read_reply(port, total_wait_s=1.5 if args.command == "dump-events" else 1.0)

    return 0


if __name__ == "__main__":
    sys.exit(main())
