import type { MutableRefObject } from "react";
import { Panel } from "./Panel";
import type { InputSnapshot } from "../models/input";
import type { LatestInputState } from "../models/ui";

type CapturePanelProps = {
  input: LatestInputState;
  latestSnapshot: InputSnapshot;
  captureEnabled: boolean;
  onSetCaptureEnabled: (enabled: boolean) => void;
  onRequestPointerLock: () => void;
  onReleasePointerLock: () => void;
  onReleaseAll: () => void;
  surfaceRef: MutableRefObject<HTMLDivElement | null>;
};

export function CapturePanel({
  input,
  latestSnapshot,
  captureEnabled,
  onSetCaptureEnabled,
  onRequestPointerLock,
  onReleasePointerLock,
  onReleaseAll,
  surfaceRef,
}: CapturePanelProps) {
  return (
    <Panel
      title="Capture"
      copy="Keyboard input is live while the app is focused. Mouse input uses pointer lock inside the capture surface."
      actions={
        <button
          className={captureEnabled ? "button-danger" : "button-accent"}
          onClick={() => onSetCaptureEnabled(!captureEnabled)}
        >
          {captureEnabled ? "Stop Capture" : "Start Capture"}
        </button>
      }
    >
      <div className="panel-grid">
        <div
          className={`capture-surface ${
            input.pointerLocked ? "capture-surface-active" : ""
          }`}
          ref={(node) => {
            surfaceRef.current = node;
          }}
          role="presentation"
          tabIndex={0}
        >
          <p className="stat-label">Pointer Lock Surface</p>
          <p className="panel-copy">
            Click the button below, then interact inside this panel.
          </p>
          <div className="button-row">
            <button onClick={onRequestPointerLock}>Pointer Lock</button>
            <button onClick={onReleasePointerLock}>Release Pointer</button>
            <button className="button-danger" onClick={onReleaseAll}>
              Release All Inputs
            </button>
          </div>
        </div>

        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Focused</p>
            <p className="stat-value">{input.windowFocused ? "yes" : "no"}</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Pointer Locked</p>
            <p className="stat-value">{input.pointerLocked ? "yes" : "no"}</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Keys Pressed</p>
            <p className="stat-value">{input.pressedCodes.length}</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Mouse Delta</p>
            <p className="stat-value">
              {latestSnapshot.mouseDeltaX.toFixed(1)},{" "}
              {latestSnapshot.mouseDeltaY.toFixed(1)}
            </p>
          </div>
        </div>

        <div>
          <p className="stat-label">Pressed Codes</p>
          <div className="chip-row">
            {input.pressedCodes.length > 0 ? (
              input.pressedCodes.map((code) => (
                <span className="mono-chip" key={code}>
                  {code}
                </span>
              ))
            ) : (
              <span className="muted">No active keys</span>
            )}
          </div>
        </div>
      </div>
    </Panel>
  );
}
