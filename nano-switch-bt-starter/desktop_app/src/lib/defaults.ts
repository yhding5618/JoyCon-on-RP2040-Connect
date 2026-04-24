import type { InputSnapshot } from "../models/input";
import type { Profile } from "../models/profile";

export const defaultInputSnapshot: InputSnapshot = {
  pressedCodes: [],
  mouseButtons: [],
  mouseDeltaX: 0,
  mouseDeltaY: 0,
  pointerLocked: false,
  captureEnabled: false,
  timestampMs: 0,
};

export const defaultLeftJoyConProfile: Profile = {
  id: "default-left-joycon",
  name: "Default Left Joy-Con",
  controllerModel: "LeftJoyCon",
  bindings: {
    KeyW: "MoveRight",
    KeyS: "MoveLeft",
    KeyA: "MoveUp",
    KeyD: "MoveDown",
    KeyL: "A",
    KeyK: "B",
    KeyI: "X",
    KeyJ: "Y",
    KeyQ: "Minus",
    KeyE: "SL",
    KeyU: "SR",
    KeyO: "Capture",
    ShiftLeft: "Stick",
  },
  mouse: {
    enabled: false,
    sensitivityX: 1,
    sensitivityY: 1,
    invertY: false,
    deadzone: 0.05,
    smoothing: 0.2,
    decayMs: 40,
  },
  outputRateHz: 125,
};

export const defaultRightJoyConProfile: Profile = {
  id: "default-right-joycon",
  name: "Default Right Joy-Con",
  controllerModel: "RightJoyCon",
  bindings: {
    KeyW: "MoveLeft",
    KeyS: "MoveRight",
    KeyA: "MoveDown",
    KeyD: "MoveUp",
    KeyI: "Y",
    KeyJ: "B",
    KeyK: "A",
    KeyL: "X",
    KeyQ: "Plus",
    KeyE: "SL",
    KeyU: "SR",
    KeyO: "Home",
    ShiftRight: "Stick",
  },
  mouse: {
    enabled: false,
    sensitivityX: 1,
    sensitivityY: 1,
    invertY: false,
    deadzone: 0.05,
    smoothing: 0.2,
    decayMs: 40,
  },
  outputRateHz: 125,
};

export const defaultProControllerProfile: Profile = {
  id: "default-pro-controller",
  name: "Default Pro Controller",
  controllerModel: "ProController",
  bindings: {
    KeyW: "MoveUp",
    KeyS: "MoveDown",
    KeyA: "MoveLeft",
    KeyD: "MoveRight",
    KeyL: "A",
    KeyK: "B",
    KeyI: "X",
    KeyJ: "Y",
    KeyQ: "Minus",
    KeyE: "L",
    KeyU: "R",
    KeyO: "Capture",
    Enter: "Plus",
    ShiftLeft: "Stick",
  },
  mouse: {
    enabled: false,
    sensitivityX: 1,
    sensitivityY: 1,
    invertY: false,
    deadzone: 0.05,
    smoothing: 0.2,
    decayMs: 40,
  },
  outputRateHz: 125,
};

export function defaultProfileForControllerModel(
  model: Profile["controllerModel"],
): Profile {
  if (model === "RightJoyCon") {
    return defaultRightJoyConProfile;
  }
  if (model === "ProController") {
    return defaultProControllerProfile;
  }
  return defaultLeftJoyConProfile;
}
