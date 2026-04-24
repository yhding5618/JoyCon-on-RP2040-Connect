import { Panel } from "./Panel";
import type { DiagnosticsState } from "../models/ui";

type DiagnosticsPanelProps = {
  diagnostics: DiagnosticsState;
  error: string | null;
};

export function DiagnosticsPanel({
  diagnostics,
  error,
}: DiagnosticsPanelProps) {
  return (
    <Panel
      title="Diagnostics"
      copy="Protocol tests are in Rust already. Serial diagnostics will populate here once the worker lands."
    >
      <div className="panel-grid">
        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Input Rate</p>
            <p className="stat-value">{diagnostics.inputRateHz.toFixed(1)} Hz</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Output Rate</p>
            <p className="stat-value">{diagnostics.outputRateHz.toFixed(1)} Hz</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Frames TX / RX</p>
            <p className="stat-value">
              {diagnostics.txCount} / {diagnostics.rxCount}
            </p>
          </div>
        </div>

        {error ? <p className="danger">{error}</p> : null}
        {diagnostics.lastSerialError ? (
          <p className="warning">{diagnostics.lastSerialError}</p>
        ) : null}

        {diagnostics.lastStatus ? (
          <div className="mini-grid">
            <div className="stat-card">
              <p className="stat-label">Status Flags</p>
              <p className="stat-value">
                0x{diagnostics.lastStatus.flags.toString(16)}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Last HID Event</p>
              <p className="stat-value">
                0x{diagnostics.lastStatus.lastHidEvent.toString(16)}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Last GAP Event</p>
              <p className="stat-value">
                0x{diagnostics.lastStatus.lastGapEvent.toString(16)}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Last Error</p>
              <p className="stat-value">
                0x{diagnostics.lastStatus.lastError.toString(16)}
              </p>
            </div>
          </div>
        ) : null}

        {(diagnostics.lastFrameTx || diagnostics.lastFrameRx) ? (
          <div className="mini-grid">
            <div className="stat-card">
              <p className="stat-label">Last TX Frame</p>
              <p className="stat-value">
                {diagnostics.lastFrameTx
                  ? `0x${diagnostics.lastFrameTx.messageType.toString(16)} seq ${diagnostics.lastFrameTx.sequence}`
                  : "none"}
              </p>
            </div>
            <div className="stat-card">
              <p className="stat-label">Last RX Frame</p>
              <p className="stat-value">
                {diagnostics.lastFrameRx
                  ? `0x${diagnostics.lastFrameRx.messageType.toString(16)} seq ${diagnostics.lastFrameRx.sequence}`
                  : "none"}
              </p>
            </div>
          </div>
        ) : null}

        <div>
          <p className="stat-label">Last Events</p>
          <ul className="log-list">
            {diagnostics.lastEvents.length > 0 ? (
              diagnostics.lastEvents.slice(0, 8).map((entry) => (
                <li className="stat-card" key={`${entry.sequence}-${entry.timestampMs}`}>
                  <strong>
                    {entry.sourceName} / {entry.eventName}
                  </strong>
                  <div className="muted">
                    seq {entry.sequence} at {entry.timestampMs} ms
                  </div>
                  <div>{entry.details}</div>
                </li>
              ))
            ) : (
              <li className="muted">No event dump received yet.</li>
            )}
          </ul>
        </div>

        <div>
          <p className="stat-label">Recent Log</p>
          <ul className="log-list">
            {diagnostics.recentLogs.length > 0 ? (
              diagnostics.recentLogs.map((entry, index) => (
                <li className="stat-card" key={`${entry}-${index}`}>
                  {entry}
                </li>
              ))
            ) : (
              <li className="muted">No diagnostics yet.</li>
            )}
          </ul>
        </div>
      </div>
    </Panel>
  );
}
