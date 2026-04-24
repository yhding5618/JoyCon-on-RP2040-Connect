import { Panel } from "./Panel";
import { ControllerStage } from "./ControllerStage";
import type { InputSnapshot } from "../models/input";
import type { Profile } from "../models/profile";
import type { ControllerStateUi } from "../models/ui";

type ControllerPreviewProps = {
  profile: Profile;
  controller: ControllerStateUi;
  latestSnapshot: InputSnapshot;
  captureEnabled: boolean;
  observedPressedCodes: string[];
  directTapOverlay: string[];
  directTapAvailable: boolean;
  onTapControllerButton: (button: string, durationMs?: number) => void;
};

export function ControllerPreview({
  profile,
  controller,
  latestSnapshot,
  captureEnabled,
  observedPressedCodes,
  directTapOverlay,
  directTapAvailable,
  onTapControllerButton,
}: ControllerPreviewProps) {
  return (
    <Panel
      title="Controller Test Bench"
      copy="Captured physical keys light the virtual keyboard and mapped controller controls; direct controller taps are available only while capture is off."
    >
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
