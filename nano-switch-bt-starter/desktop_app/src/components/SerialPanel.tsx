import { Panel } from "./Panel";
import type { SerialSessionState } from "../models/ui";

type SerialPanelProps = {
  serial: SerialSessionState;
  loading: boolean;
  onRefreshPorts: () => void;
  onSelectPort: (port: string) => void;
  onConnect: () => void;
  onDisconnect: () => void;
  onGetStatus: () => void;
  onGetEvents: () => void;
  onVirtualCableUnplug: () => void;
  onClearBonds: () => void;
  onTapButton: (button: string) => void;
};

export function SerialPanel({
  serial,
  loading,
  onRefreshPorts,
  onSelectPort,
  onConnect,
  onDisconnect,
  onGetStatus,
  onGetEvents,
  onVirtualCableUnplug,
  onClearBonds,
  onTapButton,
}: SerialPanelProps) {
  const canSendTestTap = !loading && serial.connectionState === "Connected";

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
          <button disabled={loading} onClick={onGetEvents}>
            Dump Events
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
          </div>
        ) : null}

        <div>
          <p className="stat-label">Direct Test Tap</p>
          <div className="button-row">
            {["sl", "sr", "l", "zl", "up", "down", "left", "right", "minus"].map(
              (button) => (
                <button
                  disabled={!canSendTestTap}
                  key={button}
                  onClick={() => onTapButton(button)}
                >
                  {button.toUpperCase()}
                </button>
              ),
            )}
          </div>
        </div>
      </div>
    </Panel>
  );
}
