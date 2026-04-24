import { useMemo } from "react";
import { VirtualController } from "./VirtualController";
import { VirtualKeyboard } from "./VirtualKeyboard";
import type { InputSnapshot } from "../models/input";
import type { LogicalAction, Profile } from "../models/profile";
import type { ControllerStateUi } from "../models/ui";
import { buildKeyActionIndex } from "../lib/virtualControllerSpec";

type ControllerStageProps = {
  profile: Profile;
  controller: ControllerStateUi;
  latestSnapshot: InputSnapshot;
  captureEnabled: boolean;
  observedPressedCodes: string[];
  directTapOverlay: string[];
  directTapAvailable: boolean;
  onTapControllerButton: (button: string, durationMs?: number) => void;
};

export function ControllerStage({
  profile,
  controller,
  latestSnapshot,
  captureEnabled,
  observedPressedCodes,
  directTapOverlay,
  directTapAvailable,
  onTapControllerButton,
}: ControllerStageProps) {
  const observedPressedSet = useMemo(
    () => new Set(observedPressedCodes),
    [observedPressedCodes],
  );
  const directTapOverlaySet = useMemo(
    () => new Set(directTapOverlay),
    [directTapOverlay],
  );
  const activeActions = useMemo(() => {
    const index = buildKeyActionIndex(profile);
    const actions = new Set<LogicalAction>();

    for (const code of observedPressedCodes) {
      for (const action of index.get(code) ?? []) {
        actions.add(action);
      }
    }

    return actions;
  }, [observedPressedCodes, profile]);

  return (
    <div className="controller-stage">
      <VirtualController
        model={profile.controllerModel}
        controller={controller}
        activeActions={activeActions}
        directTapOverlay={directTapOverlaySet}
        directTapAvailable={directTapAvailable}
        captureEnabled={captureEnabled}
        onTapControllerButton={onTapControllerButton}
      />
      <VirtualKeyboard
        model={profile.controllerModel}
        profile={profile}
        observedPressedCodes={observedPressedSet}
        captureEnabled={captureEnabled}
      />
      <div className="controller-stage__snapshot" aria-label="Current input snapshot">
        <span>{latestSnapshot.pressedCodes.length} captured keys</span>
        <span>
          Mouse {latestSnapshot.mouseDeltaX.toFixed(1)},{" "}
          {latestSnapshot.mouseDeltaY.toFixed(1)}
        </span>
      </div>
    </div>
  );
}
