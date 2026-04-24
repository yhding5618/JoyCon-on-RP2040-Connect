import { Panel } from "./Panel";
import type { ControllerStateUi } from "../models/ui";

type ControllerPreviewProps = {
  controller: ControllerStateUi;
};

export function ControllerPreview({ controller }: ControllerPreviewProps) {
  return (
    <Panel
      title="Controller Preview"
      copy="This preview is already driven by Rust-side mapping so the UI is only reflecting backend state."
    >
      <div className="preview-grid">
        <div className="preview-box">
          <p className="stat-label">Buttons</p>
          <p className="stat-value mono-chip">
            0x{controller.buttons.toString(16).padStart(8, "0")}
          </p>
        </div>
        <div className="preview-box">
          <p className="stat-label">Left Stick</p>
          <p className="stat-value">
            {controller.lx}, {controller.ly}
          </p>
        </div>
        <div className="preview-box">
          <p className="stat-label">Right Stick</p>
          <p className="stat-value">
            {controller.rx}, {controller.ry}
          </p>
        </div>
        <div className="preview-box">
          <p className="stat-label">Hat / Misc</p>
          <p className="stat-value">
            {controller.hat} / {controller.misc}
          </p>
        </div>
        <div className="preview-box">
          <p className="stat-label">Battery</p>
          <p className="stat-value">{controller.batteryLevel}</p>
        </div>
      </div>
    </Panel>
  );
}
