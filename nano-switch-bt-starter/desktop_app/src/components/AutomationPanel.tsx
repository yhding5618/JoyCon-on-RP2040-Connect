import { useMemo, useState } from "react";
import type {
  AutomationAction,
  AutomationActionType,
  AutomationState,
} from "../models/ui";

const defaultTapDurationMs = 120;
const maxDurationMs = 60_000;

const actionTypes: Array<{
  value: AutomationActionType;
  label: string;
}> = [
  { value: "hold", label: "Hold" },
  { value: "delay", label: "Delay" },
  { value: "release", label: "Release" },
  { value: "tap", label: "Tap" },
];

const buttonOptions = [
  { id: "a", label: "A" },
  { id: "b", label: "B" },
  { id: "x", label: "X" },
  { id: "y", label: "Y" },
  { id: "up", label: "Up" },
  { id: "down", label: "Down" },
  { id: "left", label: "Left" },
  { id: "right", label: "Right" },
  { id: "l", label: "L" },
  { id: "zl", label: "ZL" },
  { id: "r", label: "R" },
  { id: "zr", label: "ZR" },
  { id: "minus", label: "-" },
  { id: "plus", label: "+" },
  { id: "stick", label: "L3" },
  { id: "capture", label: "Cap" },
  { id: "home", label: "Home" },
];

type AutomationPanelProps = {
  automation: AutomationState;
  loading: boolean;
  onStart: (sequence: AutomationAction[], loopCount: number) => void;
  onStop: () => void;
};

export function AutomationPanel({
  automation,
  loading,
  onStart,
  onStop,
}: AutomationPanelProps) {
  const [sequence, setSequence] = useState<AutomationAction[]>([
    { actionType: "tap", buttons: ["a"], durationMs: defaultTapDurationMs },
  ]);
  const [loopCountText, setLoopCountText] = useState("1");

  const isRunning = automation.running;
  const loopCount = Number(loopCountText);
  const validationError = useMemo(
    () => validateSequence(sequence, loopCount),
    [sequence, loopCount],
  );
  const controlsDisabled = loading || isRunning;
  const canStart = !controlsDisabled && validationError === null;

  function updateAction(index: number, next: AutomationAction) {
    setSequence((current) =>
      current.map((action, actionIndex) =>
        actionIndex === index ? normalizeDraftAction(next) : action,
      ),
    );
  }

  function addAction() {
    setSequence((current) => [
      ...current,
      { actionType: "tap", buttons: ["a"], durationMs: defaultTapDurationMs },
    ]);
  }

  function removeAction(index: number) {
    setSequence((current) =>
      current.length === 1
        ? current
        : current.filter((_, actionIndex) => actionIndex !== index),
    );
  }

  function moveAction(index: number, direction: -1 | 1) {
    setSequence((current) => {
      const nextIndex = index + direction;
      if (nextIndex < 0 || nextIndex >= current.length) {
        return current;
      }

      const next = [...current];
      [next[index], next[nextIndex]] = [next[nextIndex], next[index]];
      return next;
    });
  }

  function start() {
    if (!canStart) {
      return;
    }

    onStart(sequence.map(normalizeActionForStart), loopCount);
  }

  return (
    <section className="automation-panel" aria-label="Automation">
      <div className="automation-panel__header">
        <div>
          <span className="automation-panel__progress">
            {isRunning
              ? `Loop ${automation.currentLoop} / ${automation.loopCount}`
              : "Idle"}
          </span>
        </div>
        <div className="automation-panel__actions">
          <label className="automation-loop-control" htmlFor="automation-loop-count">
            <span className="stat-label">Loops</span>
            <input
              id="automation-loop-count"
              type="number"
              min="1"
              step="1"
              value={loopCountText}
              disabled={controlsDisabled}
              onChange={(event) => setLoopCountText(event.target.value)}
            />
          </label>
          {isRunning ? (
            <button
              type="button"
              className="button-danger"
              disabled={loading}
              onClick={onStop}
            >
              Stop
            </button>
          ) : (
            <button
              type="button"
              className="button-accent"
              disabled={!canStart}
              onClick={start}
            >
              Start
            </button>
          )}
        </div>
      </div>

      {automation.lastError ? (
        <div className="automation-panel__error">{automation.lastError}</div>
      ) : null}
      {validationError && !isRunning ? (
        <div className="automation-panel__error">{validationError}</div>
      ) : null}

      <div className="automation-sequence">
        {sequence.map((action, index) => {
          const actionRequiresButtons = action.actionType !== "delay";
          const actionUsesDuration =
            action.actionType === "delay" || action.actionType === "tap";
          const active =
            isRunning && automation.currentActionIndex === index;

          return (
            <div
              className={`automation-row ${active ? "is-active" : ""}`}
              key={index}
            >
              <span className="automation-row__index">{index + 1}</span>
              <select
                value={action.actionType}
                disabled={controlsDisabled}
                aria-label={`Action ${index + 1} type`}
                onChange={(event) =>
                  updateAction(index, {
                    ...action,
                    actionType: event.target.value as AutomationActionType,
                  })
                }
              >
                {actionTypes.map((type) => (
                  <option value={type.value} key={type.value}>
                    {type.label}
                  </option>
                ))}
              </select>
              <select
                value={actionRequiresButtons ? action.buttons[0] ?? "" : ""}
                disabled={controlsDisabled || !actionRequiresButtons}
                aria-label={`Action ${index + 1} button`}
                onChange={(event) =>
                  updateAction(index, {
                    ...action,
                    buttons: event.target.value ? [event.target.value] : [],
                  })
                }
              >
                <option value="">Button</option>
                {buttonOptions.map((button) => (
                  <option value={button.id} key={button.id}>
                    {button.label}
                  </option>
                ))}
              </select>
              <input
                type="number"
                min="1"
                max={maxDurationMs}
                step="10"
                value={actionUsesDuration ? action.durationMs ?? "" : ""}
                disabled={controlsDisabled || !actionUsesDuration}
                aria-label={`Action ${index + 1} duration in milliseconds`}
                onChange={(event) =>
                  updateAction(index, {
                    ...action,
                    durationMs: numberOrNull(event.target.value),
                  })
                }
              />

              <div className="automation-row__controls">
                <button
                  type="button"
                  aria-label={`Move action ${index + 1} up`}
                  title="Move up"
                  disabled={controlsDisabled || index === 0}
                  onClick={() => moveAction(index, -1)}
                >
                  ↑
                </button>
                <button
                  type="button"
                  aria-label={`Move action ${index + 1} down`}
                  title="Move down"
                  disabled={controlsDisabled || index === sequence.length - 1}
                  onClick={() => moveAction(index, 1)}
                >
                  ↓
                </button>
                <button
                  type="button"
                  className="button-danger"
                  aria-label={`Delete action ${index + 1}`}
                  title="Delete"
                  disabled={controlsDisabled || sequence.length === 1}
                  onClick={() => removeAction(index)}
                >
                  ×
                </button>
              </div>
            </div>
          );
        })}
      </div>

      <div className="automation-panel__footer">
        <button type="button" disabled={controlsDisabled} onClick={addAction}>
          Add Action
        </button>
      </div>
    </section>
  );
}

function normalizeDraftAction(action: AutomationAction): AutomationAction {
  if (action.actionType === "delay") {
    return {
      actionType: "delay",
      buttons: action.buttons,
      durationMs: action.durationMs ?? defaultTapDurationMs,
    };
  }

  if (action.actionType === "tap") {
    return {
      ...action,
      durationMs: action.durationMs ?? defaultTapDurationMs,
    };
  }

  return {
    ...action,
    durationMs: null,
  };
}

function normalizeActionForStart(action: AutomationAction): AutomationAction {
  if (action.actionType === "delay") {
    return {
      actionType: "delay",
      buttons: [],
      durationMs: action.durationMs ?? defaultTapDurationMs,
    };
  }

  if (action.actionType === "tap") {
    return {
      ...action,
      durationMs: action.durationMs ?? defaultTapDurationMs,
    };
  }

  return {
    ...action,
    durationMs: null,
  };
}

function validateSequence(
  sequence: AutomationAction[],
  loopCount: number,
): string | null {
  if (!Number.isInteger(loopCount) || loopCount < 1) {
    return "Loop count must be a positive integer.";
  }

  if (sequence.length === 0) {
    return "Add at least one action.";
  }

  const invalidActionIndex = sequence.findIndex((action) => {
    if (action.actionType !== "delay" && action.buttons.length === 0) {
      return true;
    }

    if (action.actionType === "delay" || action.actionType === "tap") {
      const durationMs = action.durationMs;
      return (
        durationMs === null ||
        !Number.isInteger(durationMs) ||
        durationMs < 1 ||
        durationMs > maxDurationMs
      );
    }

    return false;
  });

  if (invalidActionIndex >= 0) {
    return `Action ${invalidActionIndex + 1} is incomplete.`;
  }

  return null;
}

function numberOrNull(value: string): number | null {
  if (value.trim() === "") {
    return null;
  }

  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}
