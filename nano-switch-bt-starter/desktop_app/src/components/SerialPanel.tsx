import { Panel } from "./Panel";
import type { SerialSessionState } from "../models/ui";

type SerialPanelProps = {
  serial: SerialSessionState;
  loading: boolean;
  onRefreshPorts: () => void;
  onSelectPort: (port: string) => void;
  onConnect: () => void;
  onDisconnect: () => void;
};

export function SerialPanel({
  serial,
  loading,
  onRefreshPorts,
  onSelectPort,
  onConnect,
  onDisconnect,
}: SerialPanelProps) {
  const isConnected = serial.connectionState === "Connected";

  return (
    <Panel
      title="Serial"
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
        <div className="serial-connect-row">
          <div className="serial-connect-row__port">
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
          <button
            className={isConnected ? undefined : "button-accent"}
            disabled={loading || (!isConnected && !serial.selectedPort)}
            onClick={isConnected ? onDisconnect : onConnect}
          >
            {isConnected ? "Disconnect" : "Connect"}
          </button>
        </div>
      </div>
    </Panel>
  );
}
