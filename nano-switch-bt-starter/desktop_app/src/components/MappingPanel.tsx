import { Panel } from "./Panel";
import type { ControllerModel, Profile } from "../models/profile";

type MappingPanelProps = {
  profile: Profile;
  controllerModel: ControllerModel;
};

export function MappingPanel({
  profile,
  controllerModel,
}: MappingPanelProps) {
  const summary = mappingSummary(controllerModel);

  return (
    <Panel
      title="Mapping"
      copy={summary.copy}
    >
      <div className="panel-grid">
        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Controller Model</p>
            <p className="stat-value">{summary.label}</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Output Rate</p>
            <p className="stat-value">{profile.outputRateHz} Hz</p>
          </div>
          <div className="stat-card">
            <p className="stat-label">Mouse</p>
            <p className="stat-value">
              {profile.mouse.enabled ? "experimental" : "disabled"}
            </p>
          </div>
        </div>

        <div className="stat-card">
          <p className="stat-label">Orientation</p>
          <p>{summary.orientation}</p>
        </div>

        <div className="binding-row">
          {Object.entries(profile.bindings).map(([keyCode, action]) => (
            <div className="binding-item" key={keyCode}>
              <span className="mono-chip">{formatKeyCode(keyCode)}</span>
              <span>{action}</span>
            </div>
          ))}
        </div>
      </div>
    </Panel>
  );
}

function mappingSummary(controllerModel: ControllerModel): {
  label: string;
  orientation: string;
  copy: string;
} {
  if (controllerModel === "LeftJoyCon") {
    return {
      label: "Joy-Con Left",
      orientation: "Single Joy-Con, vertical left-hand rotation. WASD is rotated onto the left stick; L/K/I/J map to A/B/X/Y through the Switch's single-Joy-Con face-button layout.",
      copy: "Left Joy-Con mode uses the sideways single-controller mapping requested for L/K/I/J and WASD.",
    };
  }

  if (controllerModel === "RightJoyCon") {
    return {
      label: "Joy-Con Right",
      orientation: "Single Joy-Con, vertical right-hand rotation. WASD targets the right stick with the opposite rotation; I/J/K/L map to Y/B/A/X.",
      copy: "Right Joy-Con mode mirrors the stick rotation and uses the requested rotated face-button layout.",
    };
  }

  return {
    label: "Switch Pro Controller",
    orientation: "Full controller mode. WASD uses the normal left-stick orientation; L/K/I/J map to A/B/X/Y, with both left and right side buttons available.",
    copy: "Pro Controller mode exposes both Joy-Con sides as one controller profile.",
  };
}

function formatKeyCode(keyCode: string): string {
  if (keyCode.startsWith("Key")) {
    return keyCode.slice(3);
  }
  if (keyCode === "ShiftLeft") {
    return "L Shift";
  }
  if (keyCode === "ShiftRight") {
    return "R Shift";
  }
  return keyCode;
}
