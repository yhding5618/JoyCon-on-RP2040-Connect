import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  clearBondsCommand,
  connectSerialCommand,
  disconnectSerialCommand,
  getAppStateSnapshot,
  getStatusCommand,
  listSerialPortsCommand,
  pushInputSnapshot,
  selectSerialPortCommand,
  setBluetoothEnabledCommand,
  setCaptureEnabledCommand,
  setControllerModeCommand,
  tapControllerButtonCommand,
  virtualCableUnplugCommand,
} from "../lib/bindings";
import { defaultInputSnapshot } from "../lib/defaults";
import { useKeyboardCapture } from "./useKeyboardCapture";
import { useMouseCapture } from "./useMouseCapture";
import type { InputSnapshot } from "../models/input";
import type { ControllerModel } from "../models/profile";
import type { AppStateSnapshot } from "../models/ui";

export function useControllerInput() {
  const [appState, setAppState] = useState<AppStateSnapshot | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [latestSnapshot, setLatestSnapshot] =
    useState<InputSnapshot>(defaultInputSnapshot);
  const [captureEnabled, setCaptureEnabledState] = useState(false);
  const [pendingAction, setPendingAction] = useState<string | null>(null);
  const [directTapOverlay, setDirectTapOverlay] = useState<string[]>([]);

  const captureEnabledRef = useRef(false);
  const currentSnapshotRef = useRef<InputSnapshot>(defaultInputSnapshot);
  const pushInFlightRef = useRef(false);
  const lastSentHashRef = useRef<string | null>(null);
  const lastCaptureEnabledRef = useRef(false);

  const handleToggleCaptureHotkey = useCallback(() => {
    setCaptureEnabled(!captureEnabledRef.current);
  }, []);

  const keyboard = useKeyboardCapture(
    captureEnabled,
    handleToggleCaptureHotkey,
  );
  const mouse = useMouseCapture(captureEnabled);
  const resetMouseDeltaRef = useRef(mouse.resetMouseDelta);

  useEffect(() => {
    resetMouseDeltaRef.current = mouse.resetMouseDelta;
  }, [mouse.resetMouseDelta]);

  useEffect(() => {
    void runCommand("initial-load", async () => {
      const snapshot = await getAppStateSnapshot();
      return listSerialPortsCommand(snapshot);
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const snapshot = useMemo<InputSnapshot>(
    () => ({
      pressedCodes: keyboard.capturedPressedCodes,
      mouseButtons: [],
      mouseDeltaX: mouse.mouseDeltaX,
      mouseDeltaY: mouse.mouseDeltaY,
      pointerLocked: mouse.pointerLocked,
      captureEnabled,
      timestampMs: Date.now(),
    }),
    [
      captureEnabled,
      keyboard.capturedPressedCodes,
      mouse.mouseDeltaX,
      mouse.mouseDeltaY,
      mouse.pointerLocked,
    ],
  );

  useEffect(() => {
    currentSnapshotRef.current = snapshot;
    setLatestSnapshot(snapshot);
  }, [snapshot]);

  useEffect(() => {
    let cancelled = false;
    let timer: number | undefined;

    const tick = async () => {
      if (cancelled) {
        return;
      }

      const captureNow = captureEnabledRef.current;
      const needsRelease = lastCaptureEnabledRef.current && !captureNow;
      const shouldSend = captureNow || needsRelease;
      const timestampMs = Date.now();
      const effectiveSnapshot = captureNow
        ? {
            ...currentSnapshotRef.current,
            captureEnabled: true,
            timestampMs,
          }
        : neutralSnapshot(timestampMs);
      const hash = hashInputSnapshot(effectiveSnapshot);

      if (
        shouldSend &&
        !pushInFlightRef.current &&
        (needsRelease || hash !== lastSentHashRef.current)
      ) {
        pushInFlightRef.current = true;
        try {
          const state = await pushInputSnapshot(effectiveSnapshot);
          if (!cancelled) {
            applySnapshot(state);
            setError(null);
            resetMouseDeltaRef.current();
            lastSentHashRef.current = hash;
          }
        } catch (value: unknown) {
          if (!cancelled) {
            setError(value instanceof Error ? value.message : String(value));
          }
        } finally {
          pushInFlightRef.current = false;
        }
      }

      lastCaptureEnabledRef.current = captureNow;
      timer = window.setTimeout(tick, captureNow ? 16 : 50);
    };

    void tick();

    return () => {
      cancelled = true;
      if (timer !== undefined) {
        window.clearTimeout(timer);
      }
    };
    // Stable refs keep the loop from being recreated by every input frame.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return {
    appState: appState ?? defaultAppStateSnapshot(),
    latestSnapshot,
    isLoading: appState === null,
    error,
    pendingAction,
    captureEnabled,
    keyboard,
    directTapOverlay,
    setCaptureEnabled,
    requestPointerLock: mouse.requestPointerLock,
    releasePointerLock: mouse.releasePointerLock,
    releaseAll: () => {
      keyboard.clear();
      mouse.resetMouseDelta();
      mouse.releasePointerLock();
      void pushInputSnapshot({
        ...defaultInputSnapshot,
        captureEnabled: captureEnabledRef.current,
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
      keyboard.clear();
      mouse.resetMouseDelta();
      mouse.releasePointerLock();
      captureEnabledRef.current = false;
      setCaptureEnabledState(false);
      void runCommand("disconnect", () => disconnectSerialCommand());
    },
    requestStatus: () => {
      void runCommand("status", () => getStatusCommand());
    },
    requestVirtualCableUnplug: () => {
      void runCommand("virtual-cable-unplug", () => virtualCableUnplugCommand());
    },
    requestClearBonds: () => {
      void runCommand("clear-bonds", () => clearBondsCommand());
    },
    setControllerMode: (mode: ControllerModel) => {
      keyboard.clear();
      mouse.resetMouseDelta();
      mouse.releasePointerLock();
      captureEnabledRef.current = false;
      setCaptureEnabledState(false);
      void runCommand("set-mode", () => setControllerModeCommand(mode));
    },
    setBluetoothEnabled: (enabled: boolean) => {
      if (!enabled) {
        keyboard.clear();
        mouse.resetMouseDelta();
        mouse.releasePointerLock();
        captureEnabledRef.current = false;
        setCaptureEnabledState(false);
      }

      void runCommand("bluetooth-toggle", async () => {
        const bluetoothState = await setBluetoothEnabledCommand(enabled);

        if (!enabled || bluetoothState.serial.lastStatus?.bluetoothEnabled !== 1) {
          return bluetoothState;
        }

        return setCaptureEnabledCommand(true);
      });
    },
    tapControllerButton: (button: string, durationMs = 120) => {
      if (captureEnabledRef.current) {
        setError("Direct tap is disabled while capture is active.");
        return;
      }

      const currentState = appState ?? defaultAppStateSnapshot();
      if (currentState.serial.connectionState !== "Connected") {
        setError("Connect serial before using direct tap.");
        return;
      }

      flashDirectTap(button);
      void runCommand("tap-button", () =>
        tapControllerButtonCommand(button, durationMs),
      );
    },
  };

  function setCaptureEnabled(enabled: boolean) {
    captureEnabledRef.current = enabled;
    setCaptureEnabledState(enabled);

    if (!enabled) {
      keyboard.clear();
      mouse.resetMouseDelta();
      mouse.releasePointerLock();
    }

    void runCommand("capture-toggle", () => setCaptureEnabledCommand(enabled));
  }

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

  function applySnapshot(nextSnapshot: AppStateSnapshot) {
    setAppState(nextSnapshot);
    captureEnabledRef.current = nextSnapshot.input.captureEnabled;
    setCaptureEnabledState(nextSnapshot.input.captureEnabled);
  }

  function flashDirectTap(button: string) {
    setDirectTapOverlay((current) =>
      current.includes(button) ? current : [...current, button],
    );
    window.setTimeout(() => {
      setDirectTapOverlay((current) =>
        current.filter((activeButton) => activeButton !== button),
      );
    }, 180);
  }
}

function neutralSnapshot(timestampMs: number): InputSnapshot {
  return {
    ...defaultInputSnapshot,
    captureEnabled: false,
    timestampMs,
  };
}

function hashInputSnapshot(snapshot: InputSnapshot): string {
  return JSON.stringify({
    pressedCodes: [...snapshot.pressedCodes].sort(),
    mouseButtons: [...snapshot.mouseButtons].sort(),
    mouseDeltaX: Number(snapshot.mouseDeltaX.toFixed(2)),
    mouseDeltaY: Number(snapshot.mouseDeltaY.toFixed(2)),
    pointerLocked: snapshot.pointerLocked,
    captureEnabled: snapshot.captureEnabled,
  });
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
