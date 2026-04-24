import { Panel } from "./Panel";
import type { ControllerModel } from "../models/profile";
import type { SerialSessionState } from "../models/ui";

type SerialPanelProps = {
  serial: SerialSessionState;
  controllerModel: ControllerModel;
  loading: boolean;
  onRefreshPorts: () => void;
  onSelectPort: (port: string) => void;
  onConnect: () => void;
  onDisconnect: () => void;
  onGetStatus: () => void;
  onVirtualCableUnplug: () => void;
  onClearBonds: () => void;
  onSetControllerMode: (mode: ControllerModel) => void;
  onSetBluetoothEnabled: (enabled: boolean) => void;
};

export function SerialPanel({
  serial,
  controllerModel,
  loading,
  onRefreshPorts,
  onSelectPort,
  onConnect,
  onDisconnect,
  onGetStatus,
  onVirtualCableUnplug,
  onClearBonds,
  onSetControllerMode,
  onSetBluetoothEnabled,
}: SerialPanelProps) {
  const isConnected = serial.connectionState === "Connected";
  const bluetoothEnabled = serial.lastStatus?.bluetoothEnabled === 1;
  const firmwareMode = serial.lastStatus
    ? controllerModelFromStatus(serial.lastStatus.controllerMode)
    : null;
  const canSwitchMode = !loading && isConnected && !bluetoothEnabled;

  return (
    <Panel
      title="Serial"
      copy="Port enumeration and discrete status/event commands now go through the Rust serial worker."
      actions={
        <button
          className="button-accent"
          disabled={loading}
          onClick={onRefreshPorts}
        >
          Refresh
        </button>
      }
    >
      <div className="panel-grid">
        <div>
          <label className="muted" htmlFor="port-select">
            Port
          </label>
          <select
            id="port-select"
            value={serial.selectedPort ?? ""}
            onChange={(event) => onSelectPort(event.target.value)}
            disabled={loading}
          >
            <option value="">
              {serial.availablePorts.length > 0 ? "Select a serial port" : "No ports loaded"}
            </option>
            {serial.availablePorts.map((port) => (
              <option key={port.portName} value={port.portName}>
                {port.displayName}
              </option>
            ))}
          </select>
        </div>

        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Connection</p>
            <p className="stat-value">{serial.connectionState}</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Baud</p>
            <p className="stat-value">{serial.baudRate}</p>
          </div>
        </div>

        <div className="button-row">
          <button
            className="button-accent"
            disabled={loading || !serial.selectedPort}
            onClick={onConnect}
          >
            Connect
          </button>
          <button disabled={loading} onClick={onDisconnect}>
            Disconnect
          </button>
          <button disabled={loading} onClick={onGetStatus}>
            Refresh Status
          </button>
          <button disabled={loading} onClick={onVirtualCableUnplug}>
            Unplug Virtual Cable
          </button>
          <button
            className="button-danger"
            disabled={loading}
            onClick={onClearBonds}
          >
            Clear Bonds
          </button>
        </div>

        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Bluetooth Power</p>
            <p className="stat-value">{bluetoothEnabled ? "On" : "Off"}</p>
            <div className="button-row">
              <button
                className={bluetoothEnabled ? "button-danger" : "button-accent"}
                disabled={loading || !isConnected}
                onClick={() => onSetBluetoothEnabled(!bluetoothEnabled)}
              >
                {bluetoothEnabled ? "Turn Off" : "Turn On"}
              </button>
            </div>
          </div>
          <div className="stat-card">
            <label className="stat-label" htmlFor="controller-mode-select">
              Controller Mode
            </label>
            <select
              id="controller-mode-select"
              value={controllerModel}
              disabled={!canSwitchMode}
              onChange={(event) =>
                onSetControllerMode(event.target.value as ControllerModel)
              }
            >
              <option value="LeftJoyCon">Joy-Con Left</option>
              <option value="RightJoyCon">Joy-Con Right</option>
              <option value="ProController">Switch Pro Controller</option>
            </select>
            <p className="muted">
              {bluetoothEnabled
                ? "Turn Bluetooth off before changing modes."
                : "Mode changes apply to the next Bluetooth identity."}
            </p>
          </div>
        </div>

        {serial.lastStatus ? (
          <div className="mini-grid">
            <div className="stat-card">
              <p className="stat-label">Flags</p>
              <p className="stat-value">0x{serial.lastStatus.flags.toString(16)}</p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Input Mode</p>
              <p className="stat-value">
                0x{serial.lastStatus.inputReportMode.toString(16)}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Battery</p>
              <p className="stat-value">{serial.lastStatus.batteryLevel}</p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Bonds</p>
              <p className="stat-value">{serial.lastStatus.bondDeviceCount}</p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Controller Mode</p>
              <p className="stat-value">
                {firmwareMode ? controllerModelLabel(firmwareMode) : "Unknown"}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Bluetooth</p>
              <p className="stat-value">
                {serial.lastStatus.bluetoothEnabled ? "On" : "Off"}
              </p>
            </div>
          </div>
        ) : null}
      </div>
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

function controllerModelLabel(model: ControllerModel): string {
  if (model === "LeftJoyCon") {
    return "Joy-Con Left";
  }
  if (model === "RightJoyCon") {
    return "Joy-Con Right";
  }
  return "Switch Pro Controller";
}
