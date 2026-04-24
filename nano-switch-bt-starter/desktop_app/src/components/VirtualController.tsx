import type { ControllerModel, LogicalAction } from "../models/profile";
import type { ControllerStateUi } from "../models/ui";
import {
  CONTROLLER_BUTTON_BITS,
  VIRTUAL_CONTROLLER_LAYOUTS,
  type ControllerButtonId,
  type VirtualControlSpec,
} from "../lib/virtualControllerSpec";

type VirtualControllerProps = {
  model: ControllerModel;
  controller: ControllerStateUi;
  activeActions: Set<LogicalAction>;
  directTapOverlay: Set<string>;
  directTapAvailable: boolean;
  captureEnabled: boolean;
  onTapControllerButton: (button: string, durationMs?: number) => void;
};

export function VirtualController({
  model,
  controller,
  activeActions,
  directTapOverlay,
  directTapAvailable,
  captureEnabled,
  onTapControllerButton,
}: VirtualControllerProps) {
  const layout = VIRTUAL_CONTROLLER_LAYOUTS[model];
  const canDirectTap = directTapAvailable && !captureEnabled;
  const disabledReason = captureEnabled
    ? "Direct tap disabled while capture is active"
    : "Direct tap unavailable until serial is connected";

  return (
    <section className="controller-stage__controller" aria-label={layout.title}>
      <div className="stage-section-header">
        <h2>{layout.title}</h2>
        <span className={canDirectTap ? "direct-tap-pill is-on" : "direct-tap-pill"}>
          {canDirectTap ? "Direct tap ready" : disabledReason}
        </span>
      </div>

      <div
        className={layout.className}
        data-model={model}
        data-orientation={layout.orientation}
      >
        {layout.controls.map((control) => {
          const active = controlIsActive(
            model,
            control,
            controller,
            activeActions,
            directTapOverlay,
          );
          const tappable = Boolean(control.tapId && canDirectTap);
          const className = [
            "virtual-control",
            `virtual-control--${control.kind}`,
            `virtual-control--${control.slot}`,
            active ? "is-active" : "",
            control.tapId ? "has-direct-tap" : "",
            tappable ? "is-tappable" : "is-disabled",
            captureEnabled && control.tapId ? "is-capture-locked" : "",
          ]
            .filter(Boolean)
            .join(" ");

          return (
            <button
              key={`${model}-${control.id}-${control.slot}`}
              type="button"
              className={className}
              aria-pressed={active}
              aria-label={
                tappable
                  ? `Tap ${control.label}`
                  : `${control.label}. ${disabledReason}`
              }
              title={tappable ? `Tap ${control.label}` : disabledReason}
              disabled={!tappable}
              onClick={() => {
                if (control.tapId && tappable) {
                  onTapControllerButton(control.tapId, 120);
                }
              }}
            >
              {control.kind === "stick" ? (
                <StickVisual
                  controller={controller}
                  right={control.id === "rightStick" || model === "RightJoyCon"}
                />
              ) : null}
              <span className="virtual-control__label">{control.label}</span>
            </button>
          );
        })}
      </div>
    </section>
  );
}

function controlIsActive(
  model: ControllerModel,
  control: VirtualControlSpec,
  controller: ControllerStateUi,
  activeActions: Set<LogicalAction>,
  directTapOverlay: Set<string>,
): boolean {
  if (control.tapId && directTapOverlay.has(control.tapId)) {
    return true;
  }

  const mask = CONTROLLER_BUTTON_BITS[model]?.[control.id as ControllerButtonId];
  if (mask !== undefined && (controller.buttons & mask) !== 0) {
    return true;
  }

  if (control.actions?.some((action) => activeActions.has(action))) {
    return true;
  }

  const threshold = 3000;
  if (control.id === "leftStick" || (model !== "RightJoyCon" && control.id === "stick")) {
    return Math.abs(controller.lx) > threshold || Math.abs(controller.ly) > threshold;
  }
  if (control.id === "rightStick" || (model === "RightJoyCon" && control.id === "stick")) {
    return Math.abs(controller.rx) > threshold || Math.abs(controller.ry) > threshold;
  }

  return false;
}

function StickVisual({
  controller,
  right,
}: {
  controller: ControllerStateUi;
  right: boolean;
}) {
  const rawX = right ? controller.rx : controller.lx;
  const rawY = right ? controller.ry : controller.ly;
  const x = Math.max(-1, Math.min(1, rawX / 32767));
  const y = Math.max(-1, Math.min(1, rawY / 32767));

  return (
    <span className="virtual-stick__well" aria-hidden="true">
      <span
        className="virtual-stick__nub"
        style={{ transform: `translate(${x * 13}px, ${-y * 13}px)` }}
      />
    </span>
  );
}
