use crate::model::{
    CommandLogEntryUi, ControllerStateUi, DiagnosticsState, EventEntryUi, FrameMetaUi,
    LatestInputState, StatusPayloadUi,
};

const MAX_RECENT_LOGS: usize = 16;
const MAX_COMMAND_LOGS: usize = 160;

pub fn push_log(diagnostics: &mut DiagnosticsState, message: impl Into<String>) {
    diagnostics.recent_logs.insert(0, message.into());
    diagnostics.recent_logs.truncate(MAX_RECENT_LOGS);
}

pub fn note_input_update(
    diagnostics: &mut DiagnosticsState,
    input: &LatestInputState,
    controller: &ControllerStateUi,
) {
    let _ = (diagnostics, input, controller);
}

pub fn note_serial_frames(
    diagnostics: &mut DiagnosticsState,
    tx_frames: &[FrameMetaUi],
    rx_frames: &[FrameMetaUi],
) {
    diagnostics.tx_count += tx_frames.len() as u64;
    diagnostics.rx_count += rx_frames.len() as u64;

    if let Some(frame) = tx_frames.last() {
        diagnostics.last_frame_tx = Some(frame.clone());
    }

    if let Some(frame) = rx_frames.last() {
        diagnostics.last_frame_rx = Some(frame.clone());
    }

    for frame in tx_frames {
        push_command_frame(diagnostics, "[PC->RP2040]", frame);
    }

    for frame in rx_frames {
        push_command_frame(diagnostics, "[RP2040->PC]", frame);
    }
}

pub fn note_status(diagnostics: &mut DiagnosticsState, status: &StatusPayloadUi) {
    diagnostics.last_status = Some(*status);
}

pub fn note_events(diagnostics: &mut DiagnosticsState, events: &[EventEntryUi]) {
    diagnostics.last_events = events.to_vec();
}

pub fn note_serial_error(diagnostics: &mut DiagnosticsState, error: impl Into<String>) {
    let error = error.into();
    diagnostics.last_serial_error = Some(error.clone());
    push_log(diagnostics, error);
}

fn push_command_frame(diagnostics: &mut DiagnosticsState, direction: &str, frame: &FrameMetaUi) {
    let index = diagnostics
        .command_log
        .first()
        .map(|entry| entry.index.saturating_add(1))
        .unwrap_or(1);
    let message_name = message_type_name(frame.message_type).to_string();
    let summary = format!(
        "{direction} {message_name} seq={} len={} crc=0x{:04X} {}",
        frame.sequence, frame.payload_len, frame.crc16, frame.details
    );

    diagnostics.command_log.insert(
        0,
        CommandLogEntryUi {
            index,
            direction: direction.to_string(),
            message_type: frame.message_type,
            message_name,
            sequence: frame.sequence,
            payload_len: frame.payload_len,
            crc16: frame.crc16,
            details: frame.details.clone(),
            summary,
        },
    );
    diagnostics.command_log.truncate(MAX_COMMAND_LOGS);
}

fn message_type_name(message_type: u8) -> &'static str {
    match message_type {
        0x01 => "HELLO",
        0x02 => "GET_STATUS",
        0x03 => "STATUS",
        0x04 => "GET_EVENTS",
        0x05 => "EVENTS",
        0x10 => "SET_STATE",
        0x11 => "VIRTUAL_CABLE_UNPLUG",
        0x12 => "CLEAR_BONDS",
        0x13 => "SET_CONTROLLER_MODE",
        0x14 => "SET_BLUETOOTH_ENABLED",
        0x15 => "PAIRING_START",
        0x16 => "PAIRING_FORGET_CURRENT_MODE",
        0x17 => "PAIRING_GET_INFO",
        0x18 => "PAIRING_INFO",
        _ => "UNKNOWN",
    }
}
