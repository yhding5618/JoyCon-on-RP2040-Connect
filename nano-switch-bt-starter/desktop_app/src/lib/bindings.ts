import { defaultLeftJoyConProfile, defaultInputSnapshot } from "./defaults";
import { invokeTauri } from "./tauri";
import type { InputSnapshot } from "../models/input";
import type { AppStateSnapshot } from "../models/ui";

const mockStatus = {
  flags: 0x07,
  protocolMode: 0,
  inputReportMode: 0x30,
  batteryLevel: 8,
  lastHostReportId: 0,
  lastError: 0,
  lastSubcommand: 0,
  lastHidEvent: 0,
  lastHidStatus: 0,
  lastHidConnStatus: 0,
  lastHidReportType: 0,
  lastHidReportId: 0,
  lastGapEvent: 0,
  lastGapStatus: 0,
  lastGapReason: 0,
  bondDeviceCount: 0,
};

let browserState: AppStateSnapshot = {
  serial: {
    availablePorts: [],
    selectedPort: null,
    baudRate: 115200,
    connectionState: "Disconnected",
    lastConnectError: null,
    lastStatus: null,
  },
  profile: {
    activeProfileId: defaultLeftJoyConProfile.id,
    activeProfile: defaultLeftJoyConProfile,
  },
  input: {
    ...defaultInputSnapshot,
    windowFocused: true,
  },
  controller: {
    buttons: 0,
    lx: 0,
    ly: 0,
    rx: 0,
    ry: 0,
    hat: 8,
    misc: 0,
    batteryLevel: 8,
  },
  diagnostics: {
    txCount: 0,
    rxCount: 0,
    inputRateHz: 0,
    outputRateHz: 0,
    recentLogs: ["Browser fallback active. Tauri commands are stubbed."],
    lastSerialError: null,
    lastStatus: null,
    lastFrameTx: null,
    lastFrameRx: null,
    lastEvents: [],
  },
};

export function getAppStateSnapshot(): Promise<AppStateSnapshot> {
  return invokeTauri("get_app_state_snapshot", undefined, () => browserState);
}

export function setCaptureEnabledCommand(
  enabled: boolean,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "set_capture_enabled",
    { enabled },
    () => {
      browserState = {
        ...browserState,
        input: {
          ...browserState.input,
          captureEnabled: enabled,
        },
      };

      return browserState;
    },
  );
}

export function listSerialPortsCommand(
  fallbackSnapshot?: AppStateSnapshot,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "list_serial_ports",
    undefined,
    () => {
      const ports = [
        { portName: "COM3", displayName: "COM3 (RP2040 mock)" },
        { portName: "COM7", displayName: "COM7 (secondary mock)" },
      ];

      browserState = {
        ...(fallbackSnapshot ?? browserState),
        serial: {
          ...(fallbackSnapshot ?? browserState).serial,
          availablePorts: ports,
          selectedPort:
            (fallbackSnapshot ?? browserState).serial.selectedPort ?? ports[0].portName,
        },
        diagnostics: {
          ...(fallbackSnapshot ?? browserState).diagnostics,
          recentLogs: ["Port list refreshed", ...(fallbackSnapshot ?? browserState).diagnostics.recentLogs].slice(0, 8),
        },
      };

      return browserState;
    },
  );
}

export function selectSerialPortCommand(
  port: string | null,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "select_serial_port",
    { port },
    () => {
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          selectedPort: port,
        },
      };
      return browserState;
    },
  );
}

export function connectSerialCommand(
  port: string,
  baud: number,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "connect_serial",
    { port, baud },
    () => {
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          selectedPort: port,
          baudRate: baud,
          connectionState: "Connected",
          lastStatus: mockStatus,
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: mockStatus,
          recentLogs: [`Connected to ${port}`, ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function disconnectSerialCommand(): Promise<AppStateSnapshot> {
  return invokeTauri(
    "disconnect_serial",
    undefined,
    () => {
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          connectionState: "Disconnected",
        },
        input: {
          ...browserState.input,
          captureEnabled: false,
        },
        diagnostics: {
          ...browserState.diagnostics,
          recentLogs: ["Disconnected", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function getStatusCommand(): Promise<AppStateSnapshot> {
  return invokeTauri(
    "get_status",
    undefined,
    () => {
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          lastStatus: mockStatus,
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: mockStatus,
          lastFrameTx: {
            messageType: 0x02,
            sequence: 1,
            payloadLen: 0,
            crc16: 0xaf25,
          },
          lastFrameRx: {
            messageType: 0x03,
            sequence: 1,
            payloadLen: 16,
            crc16: 0,
          },
          recentLogs: ["Status refreshed", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function getEventsCommand(): Promise<AppStateSnapshot> {
  return invokeTauri(
    "get_events",
    undefined,
    () => {
      browserState = {
        ...browserState,
        diagnostics: {
          ...browserState.diagnostics,
          lastEvents: [
            {
              sequence: 1,
              timestampMs: 1000,
              source: 1,
              event: 4,
              arg0: 0,
              arg1: 2,
              sourceName: "HID_CALLBACK",
              eventName: "OPEN",
              details: "status=0x00 conn=0x02",
            },
            {
              sequence: 2,
              timestampMs: 1200,
              source: 2,
              event: 0x11,
              arg0: 0,
              arg1: 0x13,
              sourceName: "GAP_CALLBACK",
              eventName: "ACL_DISCONN_CMPL",
              details: "reason=0x13",
            },
          ],
          recentLogs: ["Event dump refreshed", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function virtualCableUnplugCommand(): Promise<AppStateSnapshot> {
  return invokeTauri(
    "virtual_cable_unplug",
    undefined,
    () => {
      browserState = {
        ...browserState,
        diagnostics: {
          ...browserState.diagnostics,
          recentLogs: ["Virtual cable unplug requested", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function clearBondsCommand(): Promise<AppStateSnapshot> {
  return invokeTauri(
    "clear_bonds",
    undefined,
    () => {
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          lastStatus: {
            ...mockStatus,
            bondDeviceCount: 0,
          },
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: {
            ...mockStatus,
            bondDeviceCount: 0,
          },
          recentLogs: ["Clear bonds requested", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function tapLeftJoyConButtonCommand(
  button: string,
  durationMs = 250,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "tap_left_joycon_button",
    { button, durationMs },
    () => {
      const bits: Record<string, number> = {
        down: 1 << 0,
        up: 1 << 1,
        right: 1 << 2,
        left: 1 << 3,
        sl: 1 << 4,
        sr: 1 << 5,
        l: 1 << 6,
        zl: 1 << 7,
        minus: 1 << 8,
        stick: 1 << 9,
        capture: 1 << 10,
      };

      browserState = {
        ...browserState,
        input: {
          ...browserState.input,
          captureEnabled: false,
          pressedCodes: [],
        },
        controller: {
          ...browserState.controller,
          buttons: 0,
        },
        diagnostics: {
          ...browserState.diagnostics,
          txCount: browserState.diagnostics.txCount + 2,
          lastFrameTx: {
            messageType: 0x10,
            sequence: 1,
            payloadLen: 16,
            crc16: 0,
          },
          recentLogs: [
            `Test tap ${button} sent buttons=0x${(bits[button] ?? 0).toString(16)}`,
            ...browserState.diagnostics.recentLogs,
          ].slice(0, 8),
        },
      };

      return browserState;
    },
  );
}

export function pushInputSnapshot(
  snapshot: InputSnapshot,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "push_input_snapshot",
    { snapshot },
    () => {
      const pressed = new Set(snapshot.pressedCodes);
      let buttons = 0;

      if (pressed.has("KeyS")) buttons |= 1 << 0;
      if (pressed.has("KeyW")) buttons |= 1 << 1;
      if (pressed.has("KeyD")) buttons |= 1 << 2;
      if (pressed.has("KeyA")) buttons |= 1 << 3;
      if (pressed.has("KeyZ")) buttons |= 1 << 4;
      if (pressed.has("KeyC")) buttons |= 1 << 5;
      if (pressed.has("KeyQ")) buttons |= 1 << 6;
      if (pressed.has("KeyE")) buttons |= 1 << 7;
      if (pressed.has("Tab")) buttons |= 1 << 8;
      if (pressed.has("ShiftLeft") || pressed.has("ShiftRight")) buttons |= 1 << 9;
      if (pressed.has("F12")) buttons |= 1 << 10;

      browserState = {
        ...browserState,
        input: {
          ...snapshot,
          windowFocused: document.hasFocus(),
        },
        controller: {
          ...browserState.controller,
          buttons,
        },
        diagnostics: {
          ...browserState.diagnostics,
          inputRateHz: snapshot.captureEnabled ? 60 : 0,
          outputRateHz: snapshot.captureEnabled ? 125 : 0,
          recentLogs: [
            `Snapshot ${snapshot.pressedCodes.length} keys locked=${snapshot.pointerLocked}`,
            ...browserState.diagnostics.recentLogs,
          ].slice(0, 8),
        },
      };

      return browserState;
    },
  );
}
