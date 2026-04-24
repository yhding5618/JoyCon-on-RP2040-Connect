use std::io::ErrorKind;
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};

use serialport::{ClearBuffer, SerialPort, SerialPortType};

use crate::bridge_protocol::{
    scan_frames, ControllerStatePayload, EventDumpPayload, Frame, MessageType, StatusPayload,
};
use crate::model::{EventEntryUi, FrameMetaUi, SerialPortInfoUi, StatusPayloadUi};

const CONNECT_SETTLE_DELAY: Duration = Duration::from_millis(1000);
const DEFAULT_IO_TIMEOUT: Duration = Duration::from_millis(1000);
const READ_POLL_TIMEOUT: Duration = Duration::from_millis(100);
const STATUS_WAIT: Duration = Duration::from_millis(1000);
const EVENTS_WAIT: Duration = Duration::from_millis(1500);
const COMMAND_WAIT: Duration = Duration::from_millis(1000);

#[derive(Debug, Clone)]
pub enum WorkerCommand {
    Connect { port: String, baud: u32 },
    Disconnect,
    RefreshPorts,
    SendState(ControllerStatePayload),
    GetStatus,
    GetEvents,
    VirtualCableUnplug,
    ClearBonds,
    Shutdown,
}

#[derive(Debug, Clone)]
pub enum WorkerReply {
    Ports(Vec<SerialPortInfoUi>),
    Connected {
        port: String,
        baud: u32,
        status: Option<StatusPayloadUi>,
        tx_frames: Vec<FrameMetaUi>,
        rx_frames: Vec<FrameMetaUi>,
    },
    Disconnected,
    Status {
        status: StatusPayloadUi,
        tx_frames: Vec<FrameMetaUi>,
        rx_frames: Vec<FrameMetaUi>,
    },
    Events {
        events: Vec<EventEntryUi>,
        tx_frames: Vec<FrameMetaUi>,
        rx_frames: Vec<FrameMetaUi>,
    },
    CommandAck {
        status: Option<StatusPayloadUi>,
        tx_frames: Vec<FrameMetaUi>,
        rx_frames: Vec<FrameMetaUi>,
    },
    StateSent {
        tx_frames: Vec<FrameMetaUi>,
        rx_frames: Vec<FrameMetaUi>,
    },
}

struct WorkerRequest {
    command: WorkerCommand,
    reply: mpsc::Sender<Result<WorkerReply, String>>,
}

#[derive(Clone)]
pub struct SerialWorkerHandle {
    sender: mpsc::Sender<WorkerRequest>,
}

impl SerialWorkerHandle {
    pub fn spawn() -> Self {
        let (sender, receiver) = mpsc::channel::<WorkerRequest>();
        thread::spawn(move || worker_main(receiver));
        Self { sender }
    }

    pub fn request(&self, command: WorkerCommand) -> Result<WorkerReply, String> {
        let (reply_tx, reply_rx) = mpsc::channel();
        self.sender
            .send(WorkerRequest {
                command,
                reply: reply_tx,
            })
            .map_err(|_| "serial worker is unavailable".to_string())?;
        reply_rx
            .recv()
            .map_err(|_| "serial worker did not respond".to_string())?
    }
}

fn worker_main(receiver: mpsc::Receiver<WorkerRequest>) {
    let mut runtime = WorkerRuntime::new();

    while let Ok(request) = receiver.recv() {
        if matches!(request.command, WorkerCommand::Shutdown) {
            let _ = request.reply.send(Ok(WorkerReply::Disconnected));
            break;
        }

        let result = runtime.handle(request.command);
        let _ = request.reply.send(result);
    }
}

struct WorkerRuntime {
    port: Option<Box<dyn SerialPort>>,
    port_name: Option<String>,
    baud_rate: u32,
    sequence: u8,
}

impl WorkerRuntime {
    fn new() -> Self {
        Self {
            port: None,
            port_name: None,
            baud_rate: 115_200,
            sequence: 1,
        }
    }

    fn handle(&mut self, command: WorkerCommand) -> Result<WorkerReply, String> {
        match command {
            WorkerCommand::RefreshPorts => self.refresh_ports(),
            WorkerCommand::Connect { port, baud } => self.connect(port, baud),
            WorkerCommand::Disconnect => {
                self.disconnect();
                Ok(WorkerReply::Disconnected)
            }
            WorkerCommand::GetStatus => {
                let (status, tx_frames, rx_frames) = self.request_status()?;
                Ok(WorkerReply::Status {
                    status,
                    tx_frames,
                    rx_frames,
                })
            }
            WorkerCommand::GetEvents => {
                let (events, tx_frames, rx_frames) = self.request_events()?;
                Ok(WorkerReply::Events {
                    events,
                    tx_frames,
                    rx_frames,
                })
            }
            WorkerCommand::VirtualCableUnplug => {
                self.command_then_status(MessageType::VirtualCableUnplug)
            }
            WorkerCommand::ClearBonds => self.command_then_status(MessageType::ClearBonds),
            WorkerCommand::SendState(controller_state) => {
                let (tx_frame, frames) = self.send_state(controller_state)?;
                Ok(WorkerReply::StateSent {
                    tx_frames: vec![tx_frame],
                    rx_frames: frames.iter().map(frame_meta).collect(),
                })
            }
            WorkerCommand::Shutdown => Ok(WorkerReply::Disconnected),
        }
    }

    fn refresh_ports(&self) -> Result<WorkerReply, String> {
        let ports = serialport::available_ports()
            .map_err(|error| format!("failed to enumerate serial ports: {error}"))?;
        Ok(WorkerReply::Ports(
            ports.iter().map(port_info_to_ui).collect::<Vec<_>>(),
        ))
    }

    fn connect(&mut self, port_name: String, baud: u32) -> Result<WorkerReply, String> {
        self.disconnect();

        let mut port = serialport::new(&port_name, baud)
            .timeout(DEFAULT_IO_TIMEOUT)
            .open()
            .map_err(|error| format!("failed to open {port_name}: {error}"))?;

        port.write_data_terminal_ready(true)
            .map_err(|error| format!("failed to assert DTR on {port_name}: {error}"))?;
        port.write_request_to_send(true)
            .map_err(|error| format!("failed to assert RTS on {port_name}: {error}"))?;

        thread::sleep(CONNECT_SETTLE_DELAY);
        let _ = port.clear(ClearBuffer::All);

        self.port_name = Some(port_name.clone());
        self.baud_rate = baud;
        self.port = Some(port);

        let connected_port_name = port_name.clone();
        self.request_status()
            .map(|(status, tx_frames, rx_frames)| WorkerReply::Connected {
                port: connected_port_name,
                baud,
                status: Some(status),
                tx_frames,
                rx_frames,
            })
            .map_err(|error| {
                self.disconnect();
                format!("opened {port_name}, but bridge handshake failed: {error}")
            })
    }

    fn disconnect(&mut self) {
        self.port = None;
        self.port_name = None;
    }

    fn command_then_status(&mut self, message_type: MessageType) -> Result<WorkerReply, String> {
        let (tx_frame, frames) = self.send_message(message_type, &[], COMMAND_WAIT)?;
        let mut tx_frames = vec![tx_frame];
        let mut rx_frames = frames.iter().map(frame_meta).collect::<Vec<_>>();
        let status = match self.request_status() {
            Ok((status, mut status_tx, mut status_rx)) => {
                tx_frames.append(&mut status_tx);
                rx_frames.append(&mut status_rx);
                Some(status)
            }
            Err(_) => None,
        };

        Ok(WorkerReply::CommandAck {
            status,
            tx_frames,
            rx_frames,
        })
    }

    fn request_status(
        &mut self,
    ) -> Result<(StatusPayloadUi, Vec<FrameMetaUi>, Vec<FrameMetaUi>), String> {
        let (tx_frame, frames) = self.send_message(MessageType::GetStatus, &[], STATUS_WAIT)?;
        let status_frame = frames
            .iter()
            .find(|frame| frame.header.message_type == MessageType::Status as u8)
            .ok_or_else(|| "no STATUS frame received".to_string())?;

        let status = StatusPayload::decode(&status_frame.payload)
            .map_err(|error| format!("failed to decode STATUS payload: {error}"))?;

        Ok((status, vec![tx_frame], vec![frame_meta(status_frame)]))
    }

    fn request_events(
        &mut self,
    ) -> Result<(Vec<EventEntryUi>, Vec<FrameMetaUi>, Vec<FrameMetaUi>), String> {
        let (tx_frame, frames) = self.send_message(MessageType::GetEvents, &[], EVENTS_WAIT)?;
        let mut events = Vec::new();
        let mut rx_frames = Vec::new();

        for frame in frames
            .iter()
            .filter(|frame| frame.header.message_type == MessageType::Events as u8)
        {
            let dump = EventDumpPayload::decode(&frame.payload)
                .map_err(|error| format!("failed to decode EVENTS payload: {error}"))?;
            rx_frames.push(frame_meta(frame));

            for (index, entry) in dump.entries.iter().enumerate() {
                let sequence = dump.first_sequence.wrapping_add(index as u16);
                let (source_name, event_name, details) =
                    describe_event(entry.source, entry.event, entry.arg0, entry.arg1);
                events.push(EventEntryUi {
                    sequence,
                    timestamp_ms: entry.timestamp_ms,
                    source: entry.source,
                    event: entry.event,
                    arg0: entry.arg0,
                    arg1: entry.arg1,
                    source_name: source_name.to_string(),
                    event_name: event_name.to_string(),
                    details,
                });
            }
        }

        if events.is_empty() {
            return Err("no EVENTS frames received".to_string());
        }

        Ok((events, vec![tx_frame], rx_frames))
    }

    fn send_message(
        &mut self,
        message_type: MessageType,
        payload: &[u8],
        wait: Duration,
    ) -> Result<(FrameMetaUi, Vec<Frame>), String> {
        let sequence = self.next_sequence();
        let frame = Frame::new(message_type, sequence, payload)
            .map_err(|error| format!("failed to build frame: {error}"))?;
        let bytes = frame.to_bytes();
        let meta = frame_meta(&frame);

        let port = self
            .port
            .as_mut()
            .ok_or_else(|| "serial port is not connected".to_string())?;

        write_frame(port.as_mut(), &bytes, true)?;

        let frames = read_frames(port.as_mut(), wait)?;
        Ok((meta, frames))
    }

    fn send_state(
        &mut self,
        controller_state: ControllerStatePayload,
    ) -> Result<(FrameMetaUi, Vec<Frame>), String> {
        let sequence = self.next_sequence();
        let payload = controller_state.pack();
        let frame = Frame::new(MessageType::SetState, sequence, &payload)
            .map_err(|error| format!("failed to build frame: {error}"))?;
        let bytes = frame.to_bytes();
        let meta = frame_meta(&frame);

        let port = self
            .port
            .as_mut()
            .ok_or_else(|| "serial port is not connected".to_string())?;

        write_frame(port.as_mut(), &bytes, false)?;

        let frames = read_available_frames(port.as_mut())?;
        Ok((meta, frames))
    }

    fn next_sequence(&mut self) -> u8 {
        let sequence = self.sequence;
        self.sequence = self.sequence.wrapping_add(1);
        if self.sequence == 0 {
            self.sequence = 1;
        }
        sequence
    }
}

fn write_frame(
    port: &mut dyn SerialPort,
    bytes: &[u8],
    flush_after_write: bool,
) -> Result<(), String> {
    port.write_all(bytes)
        .map_err(|error| format!("failed to write serial frame: {error}"))?;

    if flush_after_write {
        port.flush()
            .map_err(|error| format!("failed to flush serial frame: {error}"))?;
    }

    Ok(())
}

fn read_frames(port: &mut dyn SerialPort, wait: Duration) -> Result<Vec<Frame>, String> {
    let original_timeout = port.timeout();
    port.set_timeout(READ_POLL_TIMEOUT)
        .map_err(|error| format!("failed to update serial timeout: {error}"))?;

    let result = (|| {
        let mut buffer = Vec::new();
        let mut chunk = [0u8; 256];
        let deadline = Instant::now() + wait;

        while Instant::now() < deadline {
            match port.read(&mut chunk) {
                Ok(read) if read > 0 => buffer.extend_from_slice(&chunk[..read]),
                Ok(_) => {}
                Err(error) if error.kind() == ErrorKind::TimedOut => {}
                Err(error) => return Err(format!("failed to read serial reply: {error}")),
            }
        }

        let frames = scan_frames(&buffer);
        if !buffer.is_empty() && frames.is_empty() {
            return Err("reply bytes received but no valid frames were parsed".to_string());
        }

        Ok(frames)
    })();

    let _ = port.set_timeout(original_timeout);
    result
}

fn read_available_frames(port: &mut dyn SerialPort) -> Result<Vec<Frame>, String> {
    let available = port
        .bytes_to_read()
        .map_err(|error| format!("failed to inspect serial buffer: {error}"))?;
    if available == 0 {
        return Ok(Vec::new());
    }

    let original_timeout = port.timeout();
    port.set_timeout(Duration::from_millis(1))
        .map_err(|error| format!("failed to update serial timeout: {error}"))?;

    let result = (|| {
        let mut buffer = Vec::with_capacity(available as usize);
        let mut chunk = [0u8; 256];
        while let Ok(pending) = port.bytes_to_read() {
            if pending == 0 {
                break;
            }

            match port.read(&mut chunk) {
                Ok(read) if read > 0 => buffer.extend_from_slice(&chunk[..read]),
                Ok(_) => break,
                Err(error) if error.kind() == ErrorKind::TimedOut => break,
                Err(error) => return Err(format!("failed to read serial buffer: {error}")),
            }
        }

        Ok(scan_frames(&buffer))
    })();

    let _ = port.set_timeout(original_timeout);
    result
}

fn frame_meta(frame: &Frame) -> FrameMetaUi {
    FrameMetaUi {
        message_type: frame.header.message_type,
        sequence: frame.header.sequence,
        payload_len: frame.header.payload_len,
        crc16: frame.header.crc16,
    }
}

fn port_info_to_ui(info: &serialport::SerialPortInfo) -> SerialPortInfoUi {
    SerialPortInfoUi {
        port_name: info.port_name.clone(),
        display_name: match &info.port_type {
            SerialPortType::UsbPort(details) => {
                let mut parts = Vec::new();
                if let Some(manufacturer) = &details.manufacturer {
                    parts.push(manufacturer.clone());
                }
                if let Some(product) = &details.product {
                    parts.push(product.clone());
                }
                if parts.is_empty() {
                    info.port_name.clone()
                } else {
                    format!("{} ({})", info.port_name, parts.join(" / "))
                }
            }
            SerialPortType::BluetoothPort => format!("{} (Bluetooth)", info.port_name),
            SerialPortType::PciPort => format!("{} (PCI)", info.port_name),
            SerialPortType::Unknown => info.port_name.clone(),
        },
    }
}

fn describe_event(
    source: u8,
    event: u8,
    arg0: u8,
    arg1: u8,
) -> (&'static str, &'static str, String) {
    match source {
        0x01 => (
            "HID_CALLBACK",
            hid_event_name(event),
            match event {
                0x04 | 0x05 | 0x0c => format!("status=0x{arg0:02x} conn=0x{arg1:02x}"),
                0x06 => format!("status=0x{arg0:02x} report_id=0x{arg1:02x}"),
                0x08 | 0x09 => format!("report_id=0x{arg0:02x} report_type=0x{arg1:02x}"),
                0x0b => format!("report_id=0x{arg0:02x} len=0x{arg1:02x}"),
                _ => format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
            },
        ),
        0x02 => (
            "GAP_CALLBACK",
            gap_event_name(event),
            match event {
                0x04 | 0x0a | 0x10 => format!("status=0x{arg0:02x}"),
                0x11 => format!("reason=0x{arg1:02x}"),
                _ => format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
            },
        ),
        0x03 => (
            "HID_API",
            hid_api_event_name(event),
            match event {
                0x01 => format!("status=0x{arg0:02x} report_id=0x{arg1:02x}"),
                0x05 => format!("bonds_before={arg0}"),
                0x06 => format!("status=0x{arg0:02x} bonds_after={arg1}"),
                0x07 => format!("status=0x{arg0:02x} index={arg1}"),
                _ => format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
            },
        ),
        0x04 => (
            "BRIDGE",
            bridge_event_name(event),
            format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
        ),
        0x05 => (
            "BT_IDENTITY",
            bt_identity_event_name(event),
            format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
        ),
        _ => (
            "UNKNOWN",
            "UNKNOWN",
            format!("arg0=0x{arg0:02x} arg1=0x{arg1:02x}"),
        ),
    }
}

fn hid_event_name(event: u8) -> &'static str {
    match event {
        0x00 => "INIT",
        0x01 => "DEINIT",
        0x02 => "REGISTER_APP",
        0x03 => "UNREGISTER_APP",
        0x04 => "OPEN",
        0x05 => "CLOSE",
        0x06 => "SEND_REPORT",
        0x07 => "REPORT_ERR",
        0x08 => "GET_REPORT",
        0x09 => "SET_REPORT",
        0x0a => "SET_PROTOCOL",
        0x0b => "INTR_DATA",
        0x0c => "VC_UNPLUG",
        0x0d => "API_ERR",
        _ => "UNKNOWN",
    }
}

fn gap_event_name(event: u8) -> &'static str {
    match event {
        0x04 => "AUTH_CMPL",
        0x05 => "PIN_REQ",
        0x0a => "CONFIG_EIR_DATA",
        0x0d => "MODE_CHG",
        0x10 => "ACL_CONN_CMPL",
        0x11 => "ACL_DISCONN_CMPL",
        _ => "UNKNOWN",
    }
}

fn hid_api_event_name(event: u8) -> &'static str {
    match event {
        0x01 => "SEND_REPORT",
        0x02 => "REGISTER_APP",
        0x03 => "CONNECT",
        0x04 => "VC_UNPLUG",
        0x05 => "CLEAR_BONDS_BEGIN",
        0x06 => "CLEAR_BONDS_DONE",
        0x07 => "REMOVE_BOND",
        _ => "UNKNOWN",
    }
}

fn bridge_event_name(event: u8) -> &'static str {
    match event {
        0x04 => "GET_EVENTS",
        0x11 => "VIRTUAL_CABLE_UNPLUG",
        0x12 => "CLEAR_BONDS",
        0x13 => "SET_CONTROLLER_MODE",
        0x14 => "SET_BLUETOOTH_ENABLED",
        _ => "UNKNOWN",
    }
}

fn bt_identity_event_name(event: u8) -> &'static str {
    match event {
        0x01 => "STAGE",
        0x02 => "BT_ADDR_0_1",
        0x03 => "BT_ADDR_2_3",
        0x04 => "BT_ADDR_4_5",
        0x05 => "BASE_MAC_0_1",
        0x06 => "BASE_MAC_2_3",
        0x07 => "BASE_MAC_4_5",
        0x08 => "COD_STATUS",
        0x09 => "COD_MAJOR_MINOR",
        0x0a => "COD_SERVICE",
        0x0b => "HID_SUBCLASS_DESC_LO",
        0x0c => "HID_DESC_HI_NAME_LEN",
        0x0d => "HID_STRING_LENGTHS",
        0x0e => "HID_PROVIDER_LENGTH",
        0x0f => "GAP_IDENTITY_API_0",
        0x10 => "GAP_IDENTITY_API_1",
        _ => "UNKNOWN",
    }
}
