import { useEffect, useMemo, useState } from "react";
import {
  clearBondsCommand,
  connectSerialCommand,
  disconnectSerialCommand,
  getAppStateSnapshot,
  getEventsCommand,
  getStatusCommand,
  listSerialPortsCommand,
  pushInputSnapshot,
  selectSerialPortCommand,
  setBluetoothEnabledCommand,
  setCaptureEnabledCommand,
  setControllerModeCommand,
  tapLeftJoyConButtonCommand,
  virtualCableUnplugCommand,
} from "../lib/bindings";
import { defaultInputSnapshot } from "../lib/defaults";
import { useKeyboardCapture } from "./useKeyboardCapture";
import { useMouseCapture } from "./useMouseCapture";
import type { InputSnapshot } from "../models/input";
import type { ControllerModel } from "../models/profile";
import type { AppStateSnapshot } from "../models/ui";

const FRAME_MS = 1000 / 60;

export function useControllerInput() {
  const [appState, setAppState] = useState<AppStateSnapshot | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [latestSnapshot, setLatestSnapshot] =
    useState<InputSnapshot>(defaultInputSnapshot);
  const [captureEnabled, setCaptureEnabledState] = useState(false);
  const [pendingAction, setPendingAction] = useState<string | null>(null);

  const keyboard = useKeyboardCapture(captureEnabled);
  const mouse = useMouseCapture(captureEnabled);

  useEffect(() => {
    void runCommand("initial-load", async () => {
      const snapshot = await getAppStateSnapshot();
      const hydrated = await listSerialPortsCommand(snapshot);
      return hydrated;
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const snapshot = useMemo<InputSnapshot>(
    () => ({
      pressedCodes: keyboard.pressedCodes,
      mouseButtons: [],
      mouseDeltaX: mouse.mouseDeltaX,
      mouseDeltaY: mouse.mouseDeltaY,
      pointerLocked: mouse.pointerLocked,
      captureEnabled,
      timestampMs: Date.now(),
    }),
    [
      captureEnabled,
      keyboard.pressedCodes,
      mouse.mouseDeltaX,
      mouse.mouseDeltaY,
      mouse.pointerLocked,
    ],
  );

  useEffect(() => {
    setLatestSnapshot(snapshot);
  }, [snapshot]);

  useEffect(() => {
    const interval = window.setInterval(() => {
      void pushInputSnapshot({
        ...snapshot,
        timestampMs: Date.now(),
      })
        .then((state) => {
          applySnapshot(state);
          setError(null);
          mouse.resetMouseDelta();
        })
        .catch((value: unknown) => {
          setError(value instanceof Error ? value.message : String(value));
        });
    }, FRAME_MS);

    return () => {
      window.clearInterval(interval);
    };
  }, [mouse, snapshot]);

  return {
    appState: appState ?? defaultAppStateSnapshot(),
    latestSnapshot,
    isLoading: appState === null,
    error,
    pendingAction,
    captureEnabled,
    setCaptureEnabled: (enabled: boolean) => {
      setCaptureEnabledState(enabled);
      if (!enabled) {
        keyboard.clearPressedCodes();
        mouse.resetMouseDelta();
      }

      void runCommand("capture-toggle", () => setCaptureEnabledCommand(enabled));
    },
    requestPointerLock: mouse.requestPointerLock,
    releasePointerLock: mouse.releasePointerLock,
    releaseAll: () => {
      keyboard.clearPressedCodes();
      mouse.resetMouseDelta();
      mouse.releasePointerLock();
      void pushInputSnapshot({
        ...defaultInputSnapshot,
        captureEnabled,
        timestampMs: Date.now(),
      })
        .then((state) => applySnapshot(state))
        .catch((value: unknown) => {
          setError(value instanceof Error ? value.message : String(value));
        });
    },
    surfaceRef: mouse.surfaceRef,
    selectSerialPort: (port: string) => {
      void selectSerialPortCommand(port || null)
        .then((state) => {
          applySnapshot(state);
          setError(null);
        })
        .catch((value: unknown) => {
          setError(value instanceof Error ? value.message : String(value));
        });
    },
    refreshPorts: () => {
      void runCommand("refresh-ports", () => listSerialPortsCommand());
    },
    connectSerial: () => {
      const port = appState?.serial.selectedPort;
      if (!port) {
        setError("Select a serial port first.");
        return;
      }

      void runCommand("connect", () =>
        connectSerialCommand(port, appState?.serial.baudRate ?? 115200),
      );
    },
    disconnectSerial: () => {
      void runCommand("disconnect", () => disconnectSerialCommand());
    },
    requestStatus: () => {
      void runCommand("status", () => getStatusCommand());
    },
    requestEvents: () => {
      void runCommand("events", () => getEventsCommand());
    },
    requestVirtualCableUnplug: () => {
      void runCommand("virtual-cable-unplug", () => virtualCableUnplugCommand());
    },
    requestClearBonds: () => {
      void runCommand("clear-bonds", () => clearBondsCommand());
    },
    setControllerMode: (mode: ControllerModel) => {
      keyboard.clearPressedCodes();
      mouse.resetMouseDelta();
      void runCommand("set-mode", () => setControllerModeCommand(mode));
    },
    setBluetoothEnabled: (enabled: boolean) => {
      if (!enabled) {
        keyboard.clearPressedCodes();
        mouse.resetMouseDelta();
      }
      void runCommand("bluetooth-toggle", () => setBluetoothEnabledCommand(enabled));
    },
    tapLeftJoyConButton: (button: string) => {
      void runCommand("tap-button", () => tapLeftJoyConButtonCommand(button));
    },
  };

  async function runCommand(
    label: string,
    operation: () => Promise<AppStateSnapshot>,
  ) {
    setPendingAction(label);
    try {
      const state = await operation();
      applySnapshot(state);
      setError(null);
      return state;
    } catch (value: unknown) {
      setError(value instanceof Error ? value.message : String(value));
      throw value;
    } finally {
      setPendingAction(null);
    }
  }

  function applySnapshot(snapshot: AppStateSnapshot) {
    setAppState(snapshot);
    setCaptureEnabledState(snapshot.input.captureEnabled);
  }
}

function defaultAppStateSnapshot(): AppStateSnapshot {
  return {
    serial: {
      availablePorts: [],
      selectedPort: null,
      baudRate: 115200,
      connectionState: "Disconnected",
      lastConnectError: null,
      lastStatus: null,
    },
    profile: {
      activeProfileId: "default-left-joycon",
      activeProfile: {
        id: "default-left-joycon",
        name: "Default Left Joy-Con",
        controllerModel: "LeftJoyCon",
        bindings: {},
        mouse: {
          enabled: false,
          sensitivityX: 1,
          sensitivityY: 1,
          invertY: false,
          deadzone: 0.05,
          smoothing: 0.2,
          decayMs: 40,
        },
        outputRateHz: 125,
      },
    },
    input: {
      pressedCodes: [],
      mouseButtons: [],
      mouseDeltaX: 0,
      mouseDeltaY: 0,
      pointerLocked: false,
      captureEnabled: false,
      timestampMs: 0,
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
      recentLogs: [],
      lastSerialError: null,
      lastStatus: null,
      lastFrameTx: null,
      lastFrameRx: null,
      lastEvents: [],
    },
  };
}
