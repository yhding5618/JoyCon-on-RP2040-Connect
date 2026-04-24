import type { Profile } from "./profile";

export type ConnectionState =
  | "Disconnected"
  | "Connecting"
  | "Connected"
  | "Error";

export type SerialPortInfoUi = {
  portName: string;
  displayName: string;
};

export type StatusPayloadUi = {
  flags: number;
  protocolMode: number;
  inputReportMode: number;
  batteryLevel: number;
  lastHostReportId: number;
  lastError: number;
  lastSubcommand: number;
  lastHidEvent: number;
  lastHidStatus: number;
  lastHidConnStatus: number;
  lastHidReportType: number;
  lastHidReportId: number;
  lastGapEvent: number;
  lastGapStatus: number;
  lastGapReason: number;
  bondDeviceCount: number;
  controllerMode: number;
  bluetoothEnabled: number;
  reserved0: number;
  reserved1: number;
};

export type FrameMetaUi = {
  messageType: number;
  sequence: number;
  payloadLen: number;
  crc16: number;
};

export type EventEntryUi = {
  sequence: number;
  timestampMs: number;
  source: number;
  event: number;
  arg0: number;
  arg1: number;
  sourceName: string;
  eventName: string;
  details: string;
};

export type SerialSessionState = {
  availablePorts: SerialPortInfoUi[];
  selectedPort: string | null;
  baudRate: number;
  connectionState: ConnectionState;
  lastConnectError: string | null;
  lastStatus: StatusPayloadUi | null;
};

export type ActiveProfileState = {
  activeProfileId: string;
  activeProfile: Profile;
};

export type LatestInputState = {
  pressedCodes: string[];
  mouseButtons: number[];
  mouseDeltaX: number;
  mouseDeltaY: number;
  pointerLocked: boolean;
  captureEnabled: boolean;
  timestampMs: number;
  windowFocused: boolean;
};

export type ControllerStateUi = {
  buttons: number;
  lx: number;
  ly: number;
  rx: number;
  ry: number;
  hat: number;
  misc: number;
  batteryLevel: number;
};

export type DiagnosticsState = {
  txCount: number;
  rxCount: number;
  inputRateHz: number;
  outputRateHz: number;
  recentLogs: string[];
  lastSerialError: string | null;
  lastStatus: StatusPayloadUi | null;
  lastFrameTx: FrameMetaUi | null;
  lastFrameRx: FrameMetaUi | null;
  lastEvents: EventEntryUi[];
};

export type AppStateSnapshot = {
  serial: SerialSessionState;
  profile: ActiveProfileState;
  input: LatestInputState;
  controller: ControllerStateUi;
  diagnostics: DiagnosticsState;
};
