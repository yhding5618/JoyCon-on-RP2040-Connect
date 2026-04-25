import { Panel } from "./Panel";
import type { CommandLogEntryUi } from "../models/ui";

type CommandLogPanelProps = {
  entries: CommandLogEntryUi[];
  onClear: () => void;
};

export function CommandLogPanel({ entries, onClear }: CommandLogPanelProps) {
  return (
    <Panel
      title="Command Log"
      className="command-log-panel"
      actions={
        <button disabled={entries.length === 0} onClick={onClear}>
          Clear
        </button>
      }
    >
      <ul className="command-log">
        {entries.length > 0 ? (
          entries.map((entry) => (
            <li
              className={`command-log__entry ${
                entry.direction === "[PC->RP2040]"
                  ? "command-log__entry--tx"
                  : "command-log__entry--rx"
              }`}
              key={entry.index}
            >
              <div className="command-log__main">
                <span className="command-log__direction">{entry.direction}</span>
                <strong>{entry.messageName}</strong>
              </div>
              <div className="command-log__meta">
                seq {entry.sequence} | len {entry.payloadLen} | type{" "}
                {hex8(entry.messageType)} | crc {hex16(entry.crc16)}
              </div>
              <div className="command-log__details">{entry.details}</div>
            </li>
          ))
        ) : (
          <li className="muted">No serial frames yet.</li>
        )}
      </ul>
    </Panel>
  );
}

function hex8(value: number): string {
  return `0x${value.toString(16).padStart(2, "0")}`;
}

function hex16(value: number): string {
  return `0x${value.toString(16).padStart(4, "0")}`;
}
