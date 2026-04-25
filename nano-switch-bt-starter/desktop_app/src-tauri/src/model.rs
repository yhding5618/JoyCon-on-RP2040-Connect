use std::collections::BTreeMap;

use serde::{Deserialize, Serialize};

pub type ControllerStateUi = crate::bridge_protocol::ControllerStatePayload;
pub type StatusPayloadUi = crate::bridge_protocol::StatusPayload;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum ControllerModel {
    #[default]
    LeftJoyCon,
    RightJoyCon,
    ProController,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum ConnectionState {
    #[default]
    Disconnected,
    Connecting,
    Connected,
    Error,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum LogicalAction {
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    DpadUp,
    DpadDown,
    DpadLeft,
    DpadRight,
    A,
    B,
    X,
    Y,
    L,
    ZL,
    R,
    ZR,
    SL,
    SR,
    Minus,
    Plus,
    Stick,
    Capture,
    Home,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct MouseSettings {
    pub enabled: bool,
    pub sensitivity_x: f32,
    pub sensitivity_y: f32,
    pub invert_y: bool,
    pub deadzone: f32,
    pub smoothing: f32,
    pub decay_ms: u32,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Profile {
    pub id: String,
    pub name: String,
    pub controller_model: ControllerModel,
    pub bindings: BTreeMap<String, LogicalAction>,
    pub mouse: MouseSettings,
    pub output_rate_hz: u16,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SerialPortInfoUi {
    pub port_name: String,
    pub display_name: String,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SerialSessionState {
    pub available_ports: Vec<SerialPortInfoUi>,
    pub selected_port: Option<String>,
    pub baud_rate: u32,
    pub connection_state: ConnectionState,
    pub last_connect_error: Option<String>,
    pub last_status: Option<StatusPayloadUi>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ActiveProfileState {
    pub active_profile_id: String,
    pub active_profile: Profile,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct InputSnapshot {
    pub pressed_codes: Vec<String>,
    pub mouse_buttons: Vec<u8>,
    pub mouse_delta_x: f32,
    pub mouse_delta_y: f32,
    pub pointer_locked: bool,
    pub capture_enabled: bool,
    pub timestamp_ms: u64,
}

impl InputSnapshot {
    pub fn normalized(mut self) -> Self {
        self.pressed_codes.sort();
        self.pressed_codes.dedup();
        self.mouse_buttons.sort();
        self.mouse_buttons.dedup();
        self
    }
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LatestInputState {
    pub pressed_codes: Vec<String>,
    pub mouse_buttons: Vec<u8>,
    pub mouse_delta_x: f32,
    pub mouse_delta_y: f32,
    pub pointer_locked: bool,
    pub capture_enabled: bool,
    pub timestamp_ms: u64,
    pub window_focused: bool,
}

impl LatestInputState {
    pub fn from_snapshot(snapshot: InputSnapshot, window_focused: bool) -> Self {
        Self {
            pressed_codes: snapshot.pressed_codes,
            mouse_buttons: snapshot.mouse_buttons,
            mouse_delta_x: snapshot.mouse_delta_x,
            mouse_delta_y: snapshot.mouse_delta_y,
            pointer_locked: snapshot.pointer_locked,
            capture_enabled: snapshot.capture_enabled,
            timestamp_ms: snapshot.timestamp_ms,
            window_focused,
        }
    }

    pub fn release_all(&mut self) {
        self.pressed_codes.clear();
        self.mouse_buttons.clear();
        self.mouse_delta_x = 0.0;
        self.mouse_delta_y = 0.0;
        self.pointer_locked = false;
        self.timestamp_ms = 0;
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct FrameMetaUi {
    pub message_type: u8,
    pub sequence: u8,
    pub payload_len: u8,
    pub crc16: u16,
    pub details: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct CommandLogEntryUi {
    pub index: u64,
    pub direction: String,
    pub message_type: u8,
    pub message_name: String,
    pub sequence: u8,
    pub payload_len: u8,
    pub crc16: u16,
    pub details: String,
    pub summary: String,
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EventEntryUi {
    pub sequence: u16,
    pub timestamp_ms: u32,
    pub source: u8,
    pub event: u8,
    pub arg0: u8,
    pub arg1: u8,
    pub source_name: String,
    pub event_name: String,
    pub details: String,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct DiagnosticsState {
    pub tx_count: u64,
    pub rx_count: u64,
    pub input_rate_hz: f32,
    pub output_rate_hz: f32,
    pub command_log: Vec<CommandLogEntryUi>,
    pub recent_logs: Vec<String>,
    pub last_serial_error: Option<String>,
    pub last_status: Option<StatusPayloadUi>,
    pub last_frame_tx: Option<FrameMetaUi>,
    pub last_frame_rx: Option<FrameMetaUi>,
    pub last_events: Vec<EventEntryUi>,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct AppStateSnapshot {
    pub serial: SerialSessionState,
    pub profile: ActiveProfileState,
    pub input: LatestInputState,
    pub controller: ControllerStateUi,
    pub diagnostics: DiagnosticsState,
}
