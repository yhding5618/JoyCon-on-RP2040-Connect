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
    KeyW: "MoveUp",
    KeyS: "MoveDown",
    KeyA: "MoveLeft",
    KeyD: "MoveRight",
    KeyQ: "L",
    KeyE: "ZL",
    KeyZ: "SL",
    KeyC: "SR",
    Tab: "Minus",
    ShiftLeft: "Stick",
    F12: "Capture",
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
