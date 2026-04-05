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
MSG_SET_STATE = 0x10
MSG_VC_UNPLUG = 0x11


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


def read_reply(port: serial.Serial) -> None:
    time.sleep(0.1)
    waiting = port.in_waiting
    if waiting:
        data = port.read(waiting)
        print(data.hex(" "))


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

    unplug = subparsers.add_parser("unplug", help="Request a virtual cable unplug.")
    unplug.add_argument("port", help="Serial port, for example COM5.")
    unplug.add_argument("--baud", type=int, default=115200)

    args = parser.parse_args()

    if args.command == "state":
        message_type = MSG_SET_STATE
        payload = state_payload(args)
    elif args.command == "status":
        message_type = MSG_GET_STATUS
        payload = b""
    else:
        message_type = MSG_VC_UNPLUG
        payload = b""

    frame = build_frame(message_type, 1, payload)

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        port.write(frame)
        port.flush()
        read_reply(port)

    return 0


if __name__ == "__main__":
    sys.exit(main())
