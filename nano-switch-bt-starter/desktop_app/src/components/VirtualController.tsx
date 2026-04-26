import { useMemo, type MouseEvent } from "react";
import proControllerSvgMarkup from "../assets/Nintendo_Switch_Pro_Controller-marked.svg?raw";
import type { ControllerModel, LogicalAction } from "../models/profile";
import type { ControllerStateUi } from "../models/ui";
import {
  CONTROLLER_BUTTON_BITS,
  VIRTUAL_CONTROLLER_LAYOUTS,
  type ControllerButtonId,
  type VirtualControlSpec,
} from "../lib/virtualControllerSpec";

const PRO_CONTROLLER_SVG_CONTROL_CLASSES = [
  "a-button",
  "b-button",
  "x-button",
  "y-button",
  "minus-button",
  "plus-button",
  "capture-button",
  "home-button",
  "l-button",
  "r-button",
  "left-joystick",
  "right-joystick",
  "d-pad-up",
  "d-pad-down",
  "d-pad-left",
  "d-pad-right",
] as const;

type ProControllerSvgControlClass =
  (typeof PRO_CONTROLLER_SVG_CONTROL_CLASSES)[number];

const PRO_CONTROLLER_SVG_TAP_IDS: Partial<
  Record<ProControllerSvgControlClass, string>
> = {
  "a-button": "a",
  "b-button": "b",
  "x-button": "x",
  "y-button": "y",
  "minus-button": "minus",
  "plus-button": "plus",
  "capture-button": "capture",
  "home-button": "home",
  "l-button": "l",
  "r-button": "r",
  "left-joystick": "stick",
  "d-pad-up": "up",
  "d-pad-down": "down",
  "d-pad-left": "left",
  "d-pad-right": "right",
};

const PRO_CONTROLLER_SVG_CONTROL_SELECTOR =
  PRO_CONTROLLER_SVG_CONTROL_CLASSES.map((className) => `.${className}`).join(",");

type VirtualControllerProps = {
  model: ControllerModel;
  controller: ControllerStateUi;
  activeActions: Set<LogicalAction>;
  directTapOverlay: Set<string>;
  directTapAvailable: boolean;
  captureEnabled: boolean;
  automationRunning: boolean;
  onTapControllerButton: (button: string, durationMs?: number) => void;
};

export function VirtualController({
  model,
  controller,
  activeActions,
  directTapOverlay,
  directTapAvailable,
  captureEnabled,
  automationRunning,
  onTapControllerButton,
}: VirtualControllerProps) {
  const layout = VIRTUAL_CONTROLLER_LAYOUTS[model];
  const canDirectTap = directTapAvailable && !captureEnabled && !automationRunning;
  const disabledReason = automationRunning
    ? "Direct tap disabled while automation is running"
    : captureEnabled
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

      {model === "ProController" ? (
        <ProControllerSvg
          controller={controller}
          activeActions={activeActions}
          directTapOverlay={directTapOverlay}
          canDirectTap={canDirectTap}
          disabledReason={disabledReason}
          onTapControllerButton={onTapControllerButton}
        />
      ) : (
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
      )}
    </section>
  );
}

function ProControllerSvg({
  controller,
  activeActions,
  directTapOverlay,
  canDirectTap,
  disabledReason,
  onTapControllerButton,
}: {
  controller: ControllerStateUi;
  activeActions: Set<LogicalAction>;
  directTapOverlay: Set<string>;
  canDirectTap: boolean;
  disabledReason: string;
  onTapControllerButton: (button: string, durationMs?: number) => void;
}) {
  const sanitizedMarkup = useMemo(
    () => sanitizeSvgMarkup(proControllerSvgMarkup),
    [],
  );
  const activeClasses = proControllerSvgActiveClasses(
    controller,
    activeActions,
    directTapOverlay,
  );
  const className = [
    "virtual-controller",
    "virtual-controller--pro-controller",
    "virtual-controller--svg-controller",
    canDirectTap ? "is-direct-tap-ready" : "is-direct-tap-disabled",
    ...activeClasses.map((activeClass) => `is-${activeClass}-active`),
  ].join(" ");

  function handleClick(event: MouseEvent<HTMLDivElement>) {
    const controlClass = findProControllerSvgControlClass(event.target);
    if (!controlClass || !canDirectTap) {
      return;
    }

    const tapId = PRO_CONTROLLER_SVG_TAP_IDS[controlClass];
    if (tapId) {
      onTapControllerButton(tapId, 120);
    }
  }

  return (
    <div
      className={className}
      data-model="ProController"
      data-orientation="proController"
      role="img"
      aria-label="Virtual Switch Pro Controller"
      title={canDirectTap ? "Tap a marked controller control" : disabledReason}
      onClick={handleClick}
      dangerouslySetInnerHTML={{ __html: sanitizedMarkup }}
    />
  );
}

function sanitizeSvgMarkup(markup: string): string {
  const parser = new DOMParser();
  const document = parser.parseFromString(markup, "image/svg+xml");

  document.querySelectorAll("script, foreignObject").forEach((element) => {
    element.remove();
  });

  document.querySelectorAll("*").forEach((element) => {
    for (const attribute of Array.from(element.attributes)) {
      const name = attribute.name.toLowerCase();
      const value = attribute.value.trim().toLowerCase();
      const isJavascriptLink =
        (name === "href" || name.endsWith(":href")) &&
        value.startsWith("javascript:");

      if (name.startsWith("on") || isJavascriptLink) {
        element.removeAttribute(attribute.name);
      }
    }
  });

  const svg = document.documentElement;
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("focusable", "false");

  return new XMLSerializer().serializeToString(svg);
}

function proControllerSvgActiveClasses(
  controller: ControllerStateUi,
  activeActions: Set<LogicalAction>,
  directTapOverlay: Set<string>,
): ProControllerSvgControlClass[] {
  const active = new Set<ProControllerSvgControlClass>();
  const buttonBits = CONTROLLER_BUTTON_BITS.ProController;
  const buttonActive = (
    id: ControllerButtonId,
    actions: LogicalAction[] = [],
    tapId = id,
  ) =>
    directTapOverlay.has(tapId) ||
    ((buttonBits[id] ?? 0) & controller.buttons) !== 0 ||
    actions.some((action) => activeActions.has(action));

  if (buttonActive("a", ["A"])) active.add("a-button");
  if (buttonActive("b", ["B"])) active.add("b-button");
  if (buttonActive("x", ["X"])) active.add("x-button");
  if (buttonActive("y", ["Y"])) active.add("y-button");
  if (buttonActive("minus", ["Minus"])) active.add("minus-button");
  if (buttonActive("plus", ["Plus"])) active.add("plus-button");
  if (buttonActive("capture", ["Capture"])) active.add("capture-button");
  if (buttonActive("home", ["Home"])) active.add("home-button");
  if (buttonActive("l", ["L"])) active.add("l-button");
  if (buttonActive("r", ["R"])) active.add("r-button");

  const threshold = 3000;
  const leftStickActive =
    buttonActive(
      "leftStick",
      ["Stick", "MoveUp", "MoveDown", "MoveLeft", "MoveRight"],
      "stick",
    ) ||
    Math.abs(controller.lx) > threshold ||
    Math.abs(controller.ly) > threshold;
  const rightStickActive =
    Math.abs(controller.rx) > threshold || Math.abs(controller.ry) > threshold;

  if (leftStickActive) active.add("left-joystick");
  if (rightStickActive) active.add("right-joystick");

  const dpadUpActive =
    buttonActive("up", ["DpadUp"]) ||
    controller.hat === 0 ||
    controller.hat === 1 ||
    controller.hat === 7;
  const dpadRightActive =
    buttonActive("right", ["DpadRight"]) ||
    controller.hat === 1 ||
    controller.hat === 2 ||
    controller.hat === 3;
  const dpadDownActive =
    buttonActive("down", ["DpadDown"]) ||
    controller.hat === 3 ||
    controller.hat === 4 ||
    controller.hat === 5;
  const dpadLeftActive =
    buttonActive("left", ["DpadLeft"]) ||
    controller.hat === 5 ||
    controller.hat === 6 ||
    controller.hat === 7;

  if (dpadUpActive) {
    active.add("d-pad-up");
  }
  if (dpadRightActive) {
    active.add("d-pad-right");
  }
  if (dpadDownActive) {
    active.add("d-pad-down");
  }
  if (dpadLeftActive) {
    active.add("d-pad-left");
  }

  return [...active];
}

function findProControllerSvgControlClass(
  target: EventTarget | null,
): ProControllerSvgControlClass | null {
  if (!(target instanceof Element)) {
    return null;
  }

  const control = target.closest(PRO_CONTROLLER_SVG_CONTROL_SELECTOR);
  if (!control) {
    return null;
  }

  return (
    PRO_CONTROLLER_SVG_CONTROL_CLASSES.find((className) =>
      control.classList.contains(className),
    ) ?? null
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
