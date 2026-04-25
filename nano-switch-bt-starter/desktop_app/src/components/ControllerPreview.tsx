import { useEffect, useState } from "react";
import { Panel } from "./Panel";
import { ControllerStage } from "./ControllerStage";
import type { InputSnapshot } from "../models/input";
import type { ControllerModel, Profile } from "../models/profile";
import type { ControllerStateUi, SerialSessionState } from "../models/ui";

type ControllerPreviewProps = {
  profile: Profile;
  serial: SerialSessionState;
  controller: ControllerStateUi;
  latestSnapshot: InputSnapshot;
  captureEnabled: boolean;
  loading: boolean;
  observedPressedCodes: string[];
  directTapOverlay: string[];
  directTapAvailable: boolean;
  onTurnOnBluetoothWithMode: (mode: ControllerModel) => void;
  onSetBluetoothEnabled: (enabled: boolean) => void;
  onTapControllerButton: (button: string, durationMs?: number) => void;
};

export function ControllerPreview({
  profile,
  serial,
  controller,
  latestSnapshot,
  captureEnabled,
  loading,
  observedPressedCodes,
  directTapOverlay,
  directTapAvailable,
  onTurnOnBluetoothWithMode,
  onSetBluetoothEnabled,
  onTapControllerButton,
}: ControllerPreviewProps) {
  const isConnected = serial.connectionState === "Connected";
  const bluetoothEnabled = serial.lastStatus?.bluetoothEnabled === 1;
  const firmwareMode = serial.lastStatus
    ? controllerModelFromStatus(serial.lastStatus.controllerMode)
    : null;
  const [selectedBluetoothMode, setSelectedBluetoothMode] =
    useState<ControllerModel>(profile.controllerModel);

  useEffect(() => {
    if (!bluetoothEnabled) {
      setSelectedBluetoothMode(profile.controllerModel);
    }
  }, [bluetoothEnabled, profile.controllerModel]);

  const canChangeBluetoothMode = !loading && isConnected && !bluetoothEnabled;
  const displayedBluetoothMode =
    bluetoothEnabled && firmwareMode ? firmwareMode : selectedBluetoothMode;

  return (
    <Panel title="Controller Test Bench" className="controller-panel">
      <div className="controller-toolbar">
        <label className="controller-toolbar__mode" htmlFor="controller-bluetooth-mode">
          <span className="stat-label">Mode</span>
          <select
            id="controller-bluetooth-mode"
            value={displayedBluetoothMode}
            disabled={!canChangeBluetoothMode}
            onChange={(event) =>
              setSelectedBluetoothMode(event.target.value as ControllerModel)
            }
          >
            <option value="LeftJoyCon">Joy-Con Left</option>
            <option value="RightJoyCon">Joy-Con Right</option>
            <option value="ProController">Switch Pro Controller</option>
          </select>
        </label>
        <button
          className={bluetoothEnabled ? "button-danger" : "button-accent"}
          disabled={loading || !isConnected}
          onClick={() =>
            bluetoothEnabled
              ? onSetBluetoothEnabled(false)
              : onTurnOnBluetoothWithMode(selectedBluetoothMode)
          }
        >
          {bluetoothEnabled ? "Off" : "On"}
        </button>
      </div>
      <ControllerStage
        profile={profile}
        controller={controller}
        latestSnapshot={latestSnapshot}
        captureEnabled={captureEnabled}
        observedPressedCodes={observedPressedCodes}
        directTapOverlay={directTapOverlay}
        directTapAvailable={directTapAvailable}
        onTapControllerButton={onTapControllerButton}
      />
    </Panel>
  );
}

function controllerModelFromStatus(mode: number): ControllerModel | null {
  if (mode === 0) {
    return "LeftJoyCon";
  }
  if (mode === 1) {
    return "RightJoyCon";
  }
  if (mode === 2) {
    return "ProController";
  }
  return null;
}
