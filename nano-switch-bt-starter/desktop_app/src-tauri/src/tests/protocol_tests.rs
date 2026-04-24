use crate::bridge_protocol::{
    crc16_ccitt, scan_frames, ControllerStatePayload, EventDumpPayload, Frame, FrameHeader,
    MessageType, StatusPayload, CONTROLLER_STATE_BYTES, EVENT_DUMP_PAYLOAD_BYTES,
    FRAME_HEADER_BYTES, FRAME_MAGIC0, FRAME_MAGIC1, MAX_PAYLOAD_BYTES,
};

#[test]
fn frame_header_is_eight_bytes() {
    assert_eq!(FRAME_HEADER_BYTES, 8);
    assert_eq!(std::mem::size_of::<FrameHeader>(), FRAME_HEADER_BYTES);
}

#[test]
fn max_payload_matches_header_contract() {
    assert_eq!(MAX_PAYLOAD_BYTES, 48);
}

#[test]
fn controller_payload_serializes_to_sixteen_bytes() {
    let payload = ControllerStatePayload::default().pack();
    assert_eq!(CONTROLLER_STATE_BYTES, 16);
    assert_eq!(payload.len(), CONTROLLER_STATE_BYTES);
}

#[test]
fn controller_payload_matches_python_layout() {
    let payload = ControllerStatePayload {
        buttons: (1 << 0) | (1 << 6),
        lx: 123,
        ly: -456,
        rx: 0,
        ry: 0,
        hat: 8,
        misc: 1,
        battery_level: 6,
        reserved: 0,
    }
    .pack();

    assert_eq!(
        payload,
        [
            0x41, 0x00, 0x00, 0x00, 0x7b, 0x00, 0x38, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01,
            0x06, 0x00,
        ]
    );
}

#[test]
fn crc_matches_known_vector() {
    assert_eq!(crc16_ccitt(b"123456789"), 0x29b1);
}

#[test]
fn get_status_frame_matches_python_tool() {
    let frame = Frame::new(MessageType::GetStatus, 1, &[]).expect("frame should build");
    assert_eq!(
        frame.to_bytes(),
        vec![0x53, 0x42, 0x01, 0x02, 0x01, 0x00, 0x25, 0xaf]
    );
}

#[test]
fn set_state_frame_matches_python_tool() {
    let payload = ControllerStatePayload {
        buttons: (1 << 0) | (1 << 6),
        lx: 123,
        ly: -456,
        rx: 0,
        ry: 0,
        hat: 8,
        misc: 1,
        battery_level: 6,
        reserved: 0,
    }
    .pack();

    let frame = Frame::new(MessageType::SetState, 1, &payload).expect("frame should build");
    assert_eq!(
        frame.to_bytes(),
        vec![
            0x53, 0x42, 0x01, 0x10, 0x01, 0x10, 0x76, 0x3b, 0x41, 0x00, 0x00, 0x00, 0x7b, 0x00,
            0x38, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x06, 0x00,
        ]
    );
}

#[test]
fn status_decoder_supports_current_status_payload() {
    let payload = [
        0x07, 0x00, 0x30, 0x08, 0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x00, 0x30, 0x10, 0x00, 0x13,
        0x01,
    ];
    let status = StatusPayload::decode(&payload).expect("status should decode");

    assert_eq!(status.flags, 0x07);
    assert_eq!(status.input_report_mode, 0x30);
    assert_eq!(status.last_hid_event, 0x04);
    assert_eq!(status.last_gap_event, 0x10);
    assert_eq!(status.bond_device_count, 0x01);
}

#[test]
fn event_dump_decoder_parses_fixed_payload() {
    let payload = [
        0x34, 0x12, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, 0xe8, 0x03, 0x00, 0x00, 0x01, 0x04, 0x11,
        0x02, 0xd0, 0x07, 0x00, 0x00, 0x02, 0x11, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00,
    ];
    assert_eq!(payload.len(), EVENT_DUMP_PAYLOAD_BYTES);

    let dump = EventDumpPayload::decode(&payload).expect("event dump should decode");
    assert_eq!(dump.first_sequence, 0x1234);
    assert_eq!(dump.chunk_count, 1);
    assert_eq!(dump.entry_count, 2);
    assert!(dump.overflowed);
    assert_eq!(dump.entries[0].timestamp_ms, 1000);
    assert_eq!(dump.entries[1].arg1, 0x13);
}

#[test]
fn frame_scanner_skips_noise_and_returns_valid_frames() {
    let raw = [
        0x00,
        FRAME_MAGIC0,
        FRAME_MAGIC1,
        0x01,
        0x02,
        0x01,
        0x00,
        0x25,
        0xaf,
        FRAME_MAGIC0,
        FRAME_MAGIC1,
        0x01,
        0x02,
        0x01,
        0x00,
        0x25,
        0xaf,
    ];
    let frames = scan_frames(&raw);
    assert_eq!(frames.len(), 2);
}
