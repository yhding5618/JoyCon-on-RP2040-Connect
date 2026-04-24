use serde::{Deserialize, Serialize};
use thiserror::Error;

pub const FRAME_MAGIC0: u8 = 0x53;
pub const FRAME_MAGIC1: u8 = 0x42;
pub const PROTOCOL_VERSION: u8 = 0x01;
pub const MAX_PAYLOAD_BYTES: usize = 48;
pub const FRAME_HEADER_BYTES: usize = 8;
pub const MAX_FRAME_BYTES: usize = FRAME_HEADER_BYTES + MAX_PAYLOAD_BYTES;
pub const CONTROLLER_STATE_BYTES: usize = 16;
pub const EVENT_ENTRY_BYTES: usize = 8;
pub const EVENT_DUMP_PAYLOAD_BYTES: usize = 48;
pub const EVENT_DUMP_MAX_ENTRIES: usize = 5;

pub const STATUS_FLAG_BRIDGE_READY: u8 = 1 << 0;
pub const STATUS_FLAG_BT_READY: u8 = 1 << 1;
pub const STATUS_FLAG_HID_READY: u8 = 1 << 2;
pub const STATUS_FLAG_CONNECTED: u8 = 1 << 3;
pub const STATUS_FLAG_VIRTUAL_CABLE: u8 = 1 << 4;

pub const BTN_LJC_DOWN: u32 = 1 << 0;
pub const BTN_LJC_UP: u32 = 1 << 1;
pub const BTN_LJC_RIGHT: u32 = 1 << 2;
pub const BTN_LJC_LEFT: u32 = 1 << 3;
pub const BTN_LJC_SL: u32 = 1 << 4;
pub const BTN_LJC_SR: u32 = 1 << 5;
pub const BTN_LJC_L: u32 = 1 << 6;
pub const BTN_LJC_ZL: u32 = 1 << 7;
pub const BTN_LJC_MINUS: u32 = 1 << 8;
pub const BTN_LJC_STICK: u32 = 1 << 9;
pub const BTN_LJC_CAPTURE: u32 = 1 << 10;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[repr(u8)]
pub enum MessageType {
    Hello = 0x01,
    GetStatus = 0x02,
    Status = 0x03,
    GetEvents = 0x04,
    Events = 0x05,
    SetState = 0x10,
    VirtualCableUnplug = 0x11,
    ClearBonds = 0x12,
}

impl TryFrom<u8> for MessageType {
    type Error = ProtocolError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            0x01 => Ok(Self::Hello),
            0x02 => Ok(Self::GetStatus),
            0x03 => Ok(Self::Status),
            0x04 => Ok(Self::GetEvents),
            0x05 => Ok(Self::Events),
            0x10 => Ok(Self::SetState),
            0x11 => Ok(Self::VirtualCableUnplug),
            0x12 => Ok(Self::ClearBonds),
            _ => Err(ProtocolError::UnknownMessageType(value)),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Error)]
pub enum ProtocolError {
    #[error("payload length {0} exceeds max payload size {MAX_PAYLOAD_BYTES}")]
    PayloadTooLarge(usize),
    #[error("frame is shorter than the required header size")]
    FrameTooShort,
    #[error("frame magic mismatch")]
    InvalidMagic,
    #[error("protocol version {0:#04x} is unsupported")]
    InvalidVersion(u8),
    #[error("frame length {actual} does not match header payload length {expected}")]
    InvalidFrameLength { expected: usize, actual: usize },
    #[error("frame crc mismatch: expected {expected:#06x}, got {actual:#06x}")]
    InvalidCrc { expected: u16, actual: u16 },
    #[error("unknown bridge message type {0:#04x}")]
    UnknownMessageType(u8),
    #[error("unexpected status payload length {0}")]
    InvalidStatusPayloadLength(usize),
    #[error("unexpected event dump payload length {0}")]
    InvalidEventDumpLength(usize),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct FrameHeader {
    pub magic0: u8,
    pub magic1: u8,
    pub version: u8,
    pub message_type: u8,
    pub sequence: u8,
    pub payload_len: u8,
    pub crc16: u16,
}

impl FrameHeader {
    pub fn to_bytes(self) -> [u8; FRAME_HEADER_BYTES] {
        [
            self.magic0,
            self.magic1,
            self.version,
            self.message_type,
            self.sequence,
            self.payload_len,
            (self.crc16 & 0x00FF) as u8,
            (self.crc16 >> 8) as u8,
        ]
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Self, ProtocolError> {
        if bytes.len() < FRAME_HEADER_BYTES {
            return Err(ProtocolError::FrameTooShort);
        }

        Ok(Self {
            magic0: bytes[0],
            magic1: bytes[1],
            version: bytes[2],
            message_type: bytes[3],
            sequence: bytes[4],
            payload_len: bytes[5],
            crc16: u16::from_le_bytes([bytes[6], bytes[7]]),
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Frame {
    pub header: FrameHeader,
    pub payload: Vec<u8>,
}

impl Frame {
    pub fn new(
        message_type: MessageType,
        sequence: u8,
        payload: &[u8],
    ) -> Result<Self, ProtocolError> {
        if payload.len() > MAX_PAYLOAD_BYTES {
            return Err(ProtocolError::PayloadTooLarge(payload.len()));
        }

        let crc16 = compute_frame_crc(PROTOCOL_VERSION, message_type as u8, sequence, payload);
        let header = FrameHeader {
            magic0: FRAME_MAGIC0,
            magic1: FRAME_MAGIC1,
            version: PROTOCOL_VERSION,
            message_type: message_type as u8,
            sequence,
            payload_len: payload.len() as u8,
            crc16,
        };

        Ok(Self {
            header,
            payload: payload.to_vec(),
        })
    }

    pub fn from_bytes(bytes: &[u8]) -> Result<Self, ProtocolError> {
        let header = FrameHeader::from_bytes(bytes)?;
        let expected_len = FRAME_HEADER_BYTES + header.payload_len as usize;
        if bytes.len() != expected_len {
            return Err(ProtocolError::InvalidFrameLength {
                expected: expected_len,
                actual: bytes.len(),
            });
        }

        if header.magic0 != FRAME_MAGIC0 || header.magic1 != FRAME_MAGIC1 {
            return Err(ProtocolError::InvalidMagic);
        }

        if header.version != PROTOCOL_VERSION {
            return Err(ProtocolError::InvalidVersion(header.version));
        }

        let payload = bytes[FRAME_HEADER_BYTES..].to_vec();
        let expected_crc = compute_frame_crc(
            header.version,
            header.message_type,
            header.sequence,
            &payload,
        );
        if header.crc16 != expected_crc {
            return Err(ProtocolError::InvalidCrc {
                expected: expected_crc,
                actual: header.crc16,
            });
        }

        Ok(Self { header, payload })
    }

    pub fn to_bytes(&self) -> Vec<u8> {
        let mut bytes = Vec::with_capacity(FRAME_HEADER_BYTES + self.payload.len());
        bytes.extend_from_slice(&self.header.to_bytes());
        bytes.extend_from_slice(&self.payload);
        bytes
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ControllerStatePayload {
    pub buttons: u32,
    pub lx: i16,
    pub ly: i16,
    pub rx: i16,
    pub ry: i16,
    pub hat: u8,
    pub misc: u8,
    pub battery_level: u8,
    pub reserved: u8,
}

impl Default for ControllerStatePayload {
    fn default() -> Self {
        Self {
            buttons: 0,
            lx: 0,
            ly: 0,
            rx: 0,
            ry: 0,
            hat: 8,
            misc: 0,
            battery_level: 8,
            reserved: 0,
        }
    }
}

impl ControllerStatePayload {
    pub fn pack(self) -> [u8; CONTROLLER_STATE_BYTES] {
        let mut bytes = [0u8; CONTROLLER_STATE_BYTES];
        bytes[0..4].copy_from_slice(&self.buttons.to_le_bytes());
        bytes[4..6].copy_from_slice(&self.lx.to_le_bytes());
        bytes[6..8].copy_from_slice(&self.ly.to_le_bytes());
        bytes[8..10].copy_from_slice(&self.rx.to_le_bytes());
        bytes[10..12].copy_from_slice(&self.ry.to_le_bytes());
        bytes[12] = self.hat;
        bytes[13] = self.misc;
        bytes[14] = self.battery_level;
        bytes[15] = self.reserved;
        bytes
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct StatusPayload {
    pub flags: u8,
    pub protocol_mode: u8,
    pub input_report_mode: u8,
    pub battery_level: u8,
    pub last_host_report_id: u8,
    pub last_error: u8,
    pub last_subcommand: u8,
    pub last_hid_event: u8,
    pub last_hid_status: u8,
    pub last_hid_conn_status: u8,
    pub last_hid_report_type: u8,
    pub last_hid_report_id: u8,
    pub last_gap_event: u8,
    pub last_gap_status: u8,
    pub last_gap_reason: u8,
    pub bond_device_count: u8,
}

impl StatusPayload {
    pub fn decode(payload: &[u8]) -> Result<Self, ProtocolError> {
        match payload.len() {
            8 => Ok(Self {
                flags: payload[0],
                protocol_mode: payload[1],
                input_report_mode: payload[2],
                battery_level: payload[3],
                last_host_report_id: payload[4],
                last_error: payload[5],
                last_subcommand: payload[6],
                last_hid_event: payload[7],
                ..Self::default()
            }),
            12 => Ok(Self {
                flags: payload[0],
                protocol_mode: payload[1],
                input_report_mode: payload[2],
                battery_level: payload[3],
                last_host_report_id: payload[4],
                last_error: payload[5],
                last_subcommand: payload[6],
                last_hid_event: payload[7],
                last_gap_event: payload[8],
                last_gap_status: payload[9],
                last_gap_reason: payload[10],
                bond_device_count: payload[11],
                ..Self::default()
            }),
            16 => Ok(Self {
                flags: payload[0],
                protocol_mode: payload[1],
                input_report_mode: payload[2],
                battery_level: payload[3],
                last_host_report_id: payload[4],
                last_error: payload[5],
                last_subcommand: payload[6],
                last_hid_event: payload[7],
                last_hid_status: payload[8],
                last_hid_conn_status: payload[9],
                last_hid_report_type: payload[10],
                last_hid_report_id: payload[11],
                last_gap_event: payload[12],
                last_gap_status: payload[13],
                last_gap_reason: payload[14],
                bond_device_count: payload[15],
            }),
            len => Err(ProtocolError::InvalidStatusPayloadLength(len)),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EventEntry {
    pub timestamp_ms: u32,
    pub source: u8,
    pub event: u8,
    pub arg0: u8,
    pub arg1: u8,
}

impl EventEntry {
    pub fn decode(bytes: &[u8]) -> Result<Self, ProtocolError> {
        if bytes.len() != EVENT_ENTRY_BYTES {
            return Err(ProtocolError::InvalidEventDumpLength(bytes.len()));
        }

        Ok(Self {
            timestamp_ms: u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]),
            source: bytes[4],
            event: bytes[5],
            arg0: bytes[6],
            arg1: bytes[7],
        })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct EventDumpPayload {
    pub first_sequence: u16,
    pub chunk_index: u8,
    pub chunk_count: u8,
    pub entry_count: u8,
    pub total_entries: u8,
    pub overflowed: bool,
    pub entries: Vec<EventEntry>,
}

impl EventDumpPayload {
    pub fn decode(payload: &[u8]) -> Result<Self, ProtocolError> {
        if payload.len() != EVENT_DUMP_PAYLOAD_BYTES {
            return Err(ProtocolError::InvalidEventDumpLength(payload.len()));
        }

        let first_sequence = u16::from_le_bytes([payload[0], payload[1]]);
        let chunk_index = payload[2];
        let chunk_count = payload[3];
        let entry_count = payload[4];
        let total_entries = payload[5];
        let overflowed = payload[6] != 0;

        let max_entries = usize::min(entry_count as usize, EVENT_DUMP_MAX_ENTRIES);
        let mut entries = Vec::with_capacity(max_entries);
        let mut offset = 8;

        for _ in 0..max_entries {
            entries.push(EventEntry::decode(
                &payload[offset..offset + EVENT_ENTRY_BYTES],
            )?);
            offset += EVENT_ENTRY_BYTES;
        }

        Ok(Self {
            first_sequence,
            chunk_index,
            chunk_count,
            entry_count,
            total_entries,
            overflowed,
            entries,
        })
    }
}

pub fn crc16_ccitt_seed(seed: u16, data: &[u8]) -> u16 {
    let mut crc = seed;

    for value in data {
        crc ^= (*value as u16) << 8;
        for _ in 0..8 {
            crc = if (crc & 0x8000) != 0 {
                (crc << 1) ^ 0x1021
            } else {
                crc << 1
            };
        }
    }

    crc
}

pub fn crc16_ccitt(data: &[u8]) -> u16 {
    crc16_ccitt_seed(0xFFFF, data)
}

pub fn compute_frame_crc(version: u8, message_type: u8, sequence: u8, payload: &[u8]) -> u16 {
    let metadata = [version, message_type, sequence, payload.len() as u8];
    let mut crc = crc16_ccitt(&metadata);
    if !payload.is_empty() {
        crc = crc16_ccitt_seed(crc, payload);
    }

    crc
}

pub fn scan_frames(buffer: &[u8]) -> Vec<Frame> {
    let mut frames = Vec::new();
    let mut offset = 0usize;

    while offset + FRAME_HEADER_BYTES <= buffer.len() {
        if buffer[offset] != FRAME_MAGIC0 || buffer[offset + 1] != FRAME_MAGIC1 {
            offset += 1;
            continue;
        }

        let payload_len = buffer[offset + 5] as usize;
        let frame_len = FRAME_HEADER_BYTES + payload_len;
        if frame_len > MAX_FRAME_BYTES || offset + frame_len > buffer.len() {
            break;
        }

        let candidate = &buffer[offset..offset + frame_len];
        match Frame::from_bytes(candidate) {
            Ok(frame) => {
                frames.push(frame);
                offset += frame_len;
            }
            Err(_) => {
                offset += 1;
            }
        }
    }

    frames
}
