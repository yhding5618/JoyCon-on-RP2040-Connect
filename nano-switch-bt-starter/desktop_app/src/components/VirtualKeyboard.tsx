import type { ControllerModel, Profile } from "../models/profile";
import {
  actionLabel,
  buildKeyActionIndex,
  VIRTUAL_KEYBOARD_ROWS,
} from "../lib/virtualControllerSpec";

type VirtualKeyboardProps = {
  model: ControllerModel;
  profile: Profile;
  observedPressedCodes: Set<string>;
  captureEnabled: boolean;
};

export function VirtualKeyboard({
  model,
  profile,
  observedPressedCodes,
  captureEnabled,
}: VirtualKeyboardProps) {
  const keyActionIndex = buildKeyActionIndex(profile);

  return (
    <section className="controller-stage__keyboard" aria-label="Virtual keyboard">
      <div className="stage-section-header">
        <h2>Virtual Keyboard</h2>
        <span className={captureEnabled ? "capture-pill is-on" : "capture-pill"}>
          F8 capture {captureEnabled ? "on" : "off"}
        </span>
      </div>

      <div className="virtual-keyboard-scroll" data-controller-model={model}>
        <div className="virtual-keyboard-shell" aria-hidden="true">
          <div className="virtual-keyboard virtual-keyboard--alto-white virtual-keyboard--no-numpad">
            {VIRTUAL_KEYBOARD_ROWS.map((row, rowIndex) => (
              <div className="virtual-keyboard__row" key={rowIndex}>
                {row.map((key) => {
                  const width = key.width ?? 1;

                  if (key.spacer) {
                    return (
                      <span
                        className="virtual-keyboard__spacer"
                        key={key.code}
                        style={{ flexGrow: width, flexBasis: `${width * 42}px` }}
                      />
                    );
                  }

                  const actions = keyActionIndex.get(key.code) ?? [];
                  const mappedLabel = actions.map(actionLabel).join(", ");
                  const active = observedPressedCodes.has(key.code);
                  const className = [
                    "virtual-key",
                    active ? "is-active" : "",
                    mappedLabel ? "is-mapped" : "",
                    key.code === "F8" ? "is-capture-toggle" : "",
                  ]
                    .filter(Boolean)
                    .join(" ");

                  return (
                    <div
                      className={className}
                      key={key.code}
                      data-key-code={key.code}
                      style={{ flexGrow: width, flexBasis: `${width * 42}px` }}
                    >
                      <span className="virtual-key__label">{key.label}</span>
                      {mappedLabel ? (
                        <span className="virtual-key__mapping">{mappedLabel}</span>
                      ) : null}
                    </div>
                  );
                })}
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
