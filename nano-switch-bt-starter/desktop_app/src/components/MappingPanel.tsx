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
  return (
    <Panel
      title="Mapping"
      copy="The MVP stays honest about firmware reality: Left Joy-Con is the only enabled model for now."
    >
      <div className="panel-grid">
        <div className="mini-grid">
          <div className="stat-card">
            <p className="stat-label">Controller Model</p>
            <p className="stat-value">{controllerModel}</p>
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

        <div className="binding-row">
          {Object.entries(profile.bindings).map(([keyCode, action]) => (
            <div className="binding-item" key={keyCode}>
              <span className="mono-chip">{keyCode}</span>
              <span>{action}</span>
            </div>
          ))}
        </div>
      </div>
    </Panel>
  );
}
