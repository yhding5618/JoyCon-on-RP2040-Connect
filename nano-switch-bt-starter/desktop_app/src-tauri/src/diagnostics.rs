use crate::model::{
    ControllerStateUi, DiagnosticsState, EventEntryUi, FrameMetaUi, LatestInputState,
    StatusPayloadUi,
};

const MAX_RECENT_LOGS: usize = 16;

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
