import {
  defaultInputSnapshot,
  defaultLeftJoyConProfile,
  defaultProfileForControllerModel,
} from "./defaults";
import { invokeTauri } from "./tauri";
import type { InputSnapshot } from "../models/input";
import type { AppStateSnapshot } from "../models/ui";
import type { ControllerModel } from "../models/profile";

const mockStatus = {
  flags: 0x01,
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
  controllerMode: 0,
  bluetoothEnabled: 0,
  reserved0: 0,
  reserved1: 0,
};

const controllerModeValues: Record<ControllerModel, number> = {
  LeftJoyCon: 0,
  RightJoyCon: 1,
  ProController: 2,
};

function controllerModelFromMode(mode: number): ControllerModel {
  if (mode === 1) {
    return "RightJoyCon";
  }
  if (mode === 2) {
    return "ProController";
  }
  return "LeftJoyCon";
}

function applyMockStatus(status: typeof mockStatus): void {
  const profile = defaultProfileForControllerModel(
    controllerModelFromMode(status.controllerMode),
  );
  browserState = {
    ...browserState,
    serial: {
      ...browserState.serial,
      lastStatus: status,
    },
    profile: {
      activeProfileId: profile.id,
      activeProfile: profile,
    },
    diagnostics: {
      ...browserState.diagnostics,
      lastStatus: status,
    },
  };
}

const leftJoyConButtonBits: Record<string, number> = {
  a: 1 << 0,
  down: 1 << 0,
  y: 1 << 1,
  up: 1 << 1,
  x: 1 << 2,
  right: 1 << 2,
  b: 1 << 3,
  left: 1 << 3,
  sl: 1 << 4,
  sr: 1 << 5,
  l: 1 << 6,
  r: 1 << 6,
  zl: 1 << 7,
  zr: 1 << 7,
  minus: 1 << 8,
  plus: 1 << 8,
  stick: 1 << 9,
  capture: 1 << 10,
  home: 1 << 10,
};

const rightJoyConButtonBits: Record<string, number> = {
  y: 1 << 16,
  x: 1 << 17,
  b: 1 << 18,
  a: 1 << 19,
  sr: 1 << 20,
  sl: 1 << 21,
  r: 1 << 22,
  l: 1 << 22,
  zr: 1 << 23,
  zl: 1 << 23,
  plus: 1 << 24,
  minus: 1 << 24,
  stick: 1 << 25,
  home: 1 << 26,
  capture: 1 << 26,
};

const proControllerButtonBits: Record<string, number> = {
  a: rightJoyConButtonBits.a,
  b: rightJoyConButtonBits.b,
  x: rightJoyConButtonBits.x,
  y: rightJoyConButtonBits.y,
  l: leftJoyConButtonBits.l,
  r: rightJoyConButtonBits.r,
  zl: leftJoyConButtonBits.zl,
  zr: rightJoyConButtonBits.zr,
  minus: leftJoyConButtonBits.minus,
  plus: rightJoyConButtonBits.plus,
  stick: leftJoyConButtonBits.stick,
  capture: leftJoyConButtonBits.capture,
  home: rightJoyConButtonBits.home,
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
      applyMockStatus(mockStatus);
      browserState = {
        ...browserState,
        serial: {
          ...browserState.serial,
          selectedPort: port,
          baudRate: baud,
          connectionState: "Connected",
        },
        diagnostics: {
          ...browserState.diagnostics,
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
      applyMockStatus(browserState.serial.lastStatus ?? mockStatus);
      browserState = {
        ...browserState,
        diagnostics: {
          ...browserState.diagnostics,
          lastFrameTx: {
            messageType: 0x02,
            sequence: 1,
            payloadLen: 0,
            crc16: 0xaf25,
          },
          lastFrameRx: {
            messageType: 0x03,
            sequence: 1,
            payloadLen: 20,
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
            ...(browserState.serial.lastStatus ?? mockStatus),
            bondDeviceCount: 0,
          },
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: {
            ...(browserState.serial.lastStatus ?? mockStatus),
            bondDeviceCount: 0,
          },
          recentLogs: ["Clear bonds requested", ...browserState.diagnostics.recentLogs].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function setControllerModeCommand(
  mode: ControllerModel,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "set_controller_mode",
    { mode },
    () => {
      const profile = defaultProfileForControllerModel(mode);
      const status = {
        ...(browserState.serial.lastStatus ?? mockStatus),
        controllerMode: controllerModeValues[mode],
        bluetoothEnabled: 0,
        flags: (browserState.serial.lastStatus?.flags ?? mockStatus.flags) & ~0x3e,
      };
      browserState = {
        ...browserState,
        profile: {
          activeProfileId: profile.id,
          activeProfile: profile,
        },
        input: {
          ...browserState.input,
          captureEnabled: false,
          pressedCodes: [],
        },
        controller: {
          ...browserState.controller,
          buttons: 0,
          lx: 0,
          ly: 0,
          rx: 0,
          ry: 0,
        },
        serial: {
          ...browserState.serial,
          lastStatus: status,
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: status,
          recentLogs: [
            `Controller mode set to ${mode}`,
            ...browserState.diagnostics.recentLogs,
          ].slice(0, 8),
        },
      };
      return browserState;
    },
  );
}

export function setBluetoothEnabledCommand(
  enabled: boolean,
): Promise<AppStateSnapshot> {
  return invokeTauri(
    "set_bluetooth_enabled",
    { enabled },
    () => {
      const previousStatus = browserState.serial.lastStatus ?? mockStatus;
      const flags = enabled
        ? previousStatus.flags | 0x20
        : previousStatus.flags & ~0x3e;
      const status = {
        ...previousStatus,
        flags,
        bluetoothEnabled: enabled ? 1 : 0,
      };
      browserState = {
        ...browserState,
        input: {
          ...browserState.input,
          captureEnabled: enabled ? browserState.input.captureEnabled : false,
          pressedCodes: enabled ? browserState.input.pressedCodes : [],
        },
        controller: enabled
          ? browserState.controller
          : {
              ...browserState.controller,
              buttons: 0,
              lx: 0,
              ly: 0,
              rx: 0,
              ry: 0,
            },
        serial: {
          ...browserState.serial,
          lastStatus: status,
        },
        diagnostics: {
          ...browserState.diagnostics,
          lastStatus: status,
          recentLogs: [
            `Bluetooth ${enabled ? "enabled" : "disabled"}`,
            ...browserState.diagnostics.recentLogs,
          ].slice(0, 8),
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
      const bits = testButtonBitsForControllerModel(
        browserState.profile.activeProfile.controllerModel,
      );

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

function testButtonBitsForControllerModel(
  controllerModel: ControllerModel,
): Record<string, number> {
  if (controllerModel === "RightJoyCon") {
    return rightJoyConButtonBits;
  }
  if (controllerModel === "ProController") {
    return proControllerButtonBits;
  }
  return leftJoyConButtonBits;
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
      const stickExtent = 32767;
      let lx = 0;
      let ly = 0;

      if (browserState.profile.activeProfile.controllerModel === "RightJoyCon") {
        if (pressed.has("KeyW")) lx -= stickExtent;
        if (pressed.has("KeyS")) lx += stickExtent;
        if (pressed.has("KeyA")) ly -= stickExtent;
        if (pressed.has("KeyD")) ly += stickExtent;
        if (pressed.has("KeyJ")) buttons |= rightJoyConButtonBits.y;
        if (pressed.has("KeyI")) buttons |= rightJoyConButtonBits.x;
        if (pressed.has("KeyK")) buttons |= rightJoyConButtonBits.b;
        if (pressed.has("KeyL")) buttons |= rightJoyConButtonBits.a;
        if (pressed.has("KeyE")) buttons |= rightJoyConButtonBits.sl;
        if (pressed.has("KeyU")) buttons |= rightJoyConButtonBits.sr;
        if (pressed.has("KeyQ")) buttons |= rightJoyConButtonBits.plus;
        if (pressed.has("ShiftRight")) buttons |= rightJoyConButtonBits.stick;
        if (pressed.has("KeyO")) buttons |= rightJoyConButtonBits.home;
      } else if (browserState.profile.activeProfile.controllerModel === "ProController") {
        if (pressed.has("KeyA")) lx -= stickExtent;
        if (pressed.has("KeyD")) lx += stickExtent;
        if (pressed.has("KeyS")) ly -= stickExtent;
        if (pressed.has("KeyW")) ly += stickExtent;
        if (pressed.has("KeyJ")) buttons |= rightJoyConButtonBits.y;
        if (pressed.has("KeyI")) buttons |= rightJoyConButtonBits.x;
        if (pressed.has("KeyK")) buttons |= rightJoyConButtonBits.b;
        if (pressed.has("KeyL")) buttons |= rightJoyConButtonBits.a;
        if (pressed.has("KeyE")) buttons |= leftJoyConButtonBits.l;
        if (pressed.has("KeyU")) buttons |= rightJoyConButtonBits.r;
        if (pressed.has("KeyQ")) buttons |= leftJoyConButtonBits.minus;
        if (pressed.has("Enter")) buttons |= rightJoyConButtonBits.plus;
        if (pressed.has("ShiftLeft") || pressed.has("ShiftRight")) {
          buttons |= leftJoyConButtonBits.stick;
        }
        if (pressed.has("KeyO")) buttons |= leftJoyConButtonBits.capture;
      } else {
        if (pressed.has("KeyS")) lx -= stickExtent;
        if (pressed.has("KeyW")) lx += stickExtent;
        if (pressed.has("KeyD")) ly -= stickExtent;
        if (pressed.has("KeyA")) ly += stickExtent;
        if (pressed.has("KeyL")) buttons |= leftJoyConButtonBits.a;
        if (pressed.has("KeyJ")) buttons |= leftJoyConButtonBits.y;
        if (pressed.has("KeyI")) buttons |= leftJoyConButtonBits.x;
        if (pressed.has("KeyK")) buttons |= leftJoyConButtonBits.b;
        if (pressed.has("KeyE")) buttons |= leftJoyConButtonBits.sl;
        if (pressed.has("KeyU")) buttons |= leftJoyConButtonBits.sr;
        if (pressed.has("KeyQ")) buttons |= leftJoyConButtonBits.minus;
        if (pressed.has("ShiftLeft") || pressed.has("ShiftRight")) {
          buttons |= leftJoyConButtonBits.stick;
        }
        if (pressed.has("KeyO")) buttons |= leftJoyConButtonBits.capture;
      }

      if (!snapshot.captureEnabled || !document.hasFocus()) {
        buttons = 0;
        lx = 0;
        ly = 0;
      }

      browserState = {
        ...browserState,
        input: {
          ...snapshot,
          windowFocused: document.hasFocus(),
        },
        controller: {
          ...browserState.controller,
          buttons,
          lx: browserState.profile.activeProfile.controllerModel === "RightJoyCon"
            ? 0
            : Math.max(-stickExtent, Math.min(stickExtent, lx)),
          ly: browserState.profile.activeProfile.controllerModel === "RightJoyCon"
            ? 0
            : Math.max(-stickExtent, Math.min(stickExtent, ly)),
          rx: browserState.profile.activeProfile.controllerModel === "RightJoyCon"
            ? Math.max(-stickExtent, Math.min(stickExtent, lx))
            : 0,
          ry: browserState.profile.activeProfile.controllerModel === "RightJoyCon"
            ? Math.max(-stickExtent, Math.min(stickExtent, ly))
            : 0,
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
