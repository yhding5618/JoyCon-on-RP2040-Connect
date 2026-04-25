import {
  defaultProfileForControllerModel,
} from "./defaults";
import type {
  ControllerModel,
  LogicalAction,
  Profile,
} from "../models/profile";

export type ControllerButtonId =
  | "a"
  | "b"
  | "x"
  | "y"
  | "up"
  | "down"
  | "left"
  | "right"
  | "l"
  | "r"
  | "zl"
  | "zr"
  | "sl"
  | "sr"
  | "minus"
  | "plus"
  | "stick"
  | "leftStick"
  | "rightStick"
  | "capture"
  | "home";

export type VirtualControlKind =
  | "button"
  | "stick"
  | "dpad"
  | "shoulder"
  | "system";

export type VirtualControllerOrientation =
  | "singleJoyConHorizontal"
  | "proController";

export type VirtualControlSpec = {
  id: ControllerButtonId;
  label: string;
  kind: VirtualControlKind;
  slot: string;
  tapId?: string;
  actions?: LogicalAction[];
};

export type VirtualControllerLayout = {
  model: ControllerModel;
  title: string;
  className: string;
  orientation: VirtualControllerOrientation;
  controls: VirtualControlSpec[];
};

export type VirtualKeySpec = {
  code: string;
  label: string;
  width?: number;
  spacer?: boolean;
};

const gap = (id: string, width = 0.45): VirtualKeySpec => ({
  code: `__gap_${id}`,
  label: "",
  width,
  spacer: true,
});

export const CONTROLLER_BUTTON_BITS: Record<
  ControllerModel,
  Partial<Record<ControllerButtonId, number>>
> = {
  LeftJoyCon: {
    down: 1 << 0,
    a: 1 << 0,
    up: 1 << 1,
    y: 1 << 1,
    right: 1 << 2,
    x: 1 << 2,
    left: 1 << 3,
    b: 1 << 3,
    sl: 1 << 4,
    sr: 1 << 5,
    l: 1 << 6,
    r: 1 << 6,
    zl: 1 << 7,
    zr: 1 << 7,
    minus: 1 << 8,
    plus: 1 << 8,
    stick: 1 << 9,
    leftStick: 1 << 9,
    capture: 1 << 10,
    home: 1 << 10,
  },
  RightJoyCon: {
    y: 1 << 16,
    x: 1 << 17,
    b: 1 << 18,
    a: 1 << 19,
    sr: 1 << 20,
    sl: 1 << 21,
    r: 1 << 22,
    l: 1 << 22,
    zr: 1 << 23,
    zl: 1 << 23,
    plus: 1 << 24,
    minus: 1 << 24,
    stick: 1 << 25,
    rightStick: 1 << 25,
    home: 1 << 26,
    capture: 1 << 26,
  },
  ProController: {
    down: 1 << 0,
    up: 1 << 1,
    right: 1 << 2,
    left: 1 << 3,
    y: 1 << 16,
    x: 1 << 17,
    b: 1 << 18,
    a: 1 << 19,
    l: 1 << 6,
    r: 1 << 22,
    zl: 1 << 7,
    zr: 1 << 23,
    minus: 1 << 8,
    plus: 1 << 24,
    leftStick: 1 << 9,
    stick: 1 << 9,
    capture: 1 << 10,
    home: 1 << 26,
  },
};

export const VIRTUAL_CONTROLLER_LAYOUTS: Record<
  ControllerModel,
  VirtualControllerLayout
> = {
  LeftJoyCon: {
    model: "LeftJoyCon",
    title: "Virtual Left Joy-Con",
    className:
      "virtual-controller virtual-controller--joycon virtual-controller--left-joycon virtual-controller--single-horizontal",
    orientation: "singleJoyConHorizontal",
    controls: [
      { id: "sl", label: "SL", kind: "shoulder", slot: "rail-sl", tapId: "sl", actions: ["SL"] },
      { id: "sr", label: "SR", kind: "shoulder", slot: "rail-sr", tapId: "sr", actions: ["SR"] },
      { id: "zl", label: "ZL", kind: "shoulder", slot: "end-zl", tapId: "zl", actions: ["ZL"] },
      { id: "l", label: "L", kind: "shoulder", slot: "end-l", tapId: "l", actions: ["L"] },
      { id: "minus", label: "-", kind: "system", slot: "minus", tapId: "minus", actions: ["Minus"] },
      { id: "capture", label: "Cap", kind: "system", slot: "capture", tapId: "capture", actions: ["Capture"] },
      {
        id: "stick",
        label: "Stick",
        kind: "stick",
        slot: "stick",
        tapId: "stick",
        actions: ["Stick", "MoveUp", "MoveDown", "MoveLeft", "MoveRight"],
      },
      { id: "x", label: "X", kind: "button", slot: "face-x", tapId: "x", actions: ["X"] },
      { id: "y", label: "Y", kind: "button", slot: "face-y", tapId: "y", actions: ["Y"] },
      { id: "a", label: "A", kind: "button", slot: "face-a", tapId: "a", actions: ["A"] },
      { id: "b", label: "B", kind: "button", slot: "face-b", tapId: "b", actions: ["B"] },
    ],
  },
  RightJoyCon: {
    model: "RightJoyCon",
    title: "Virtual Right Joy-Con",
    className:
      "virtual-controller virtual-controller--joycon virtual-controller--right-joycon virtual-controller--single-horizontal",
    orientation: "singleJoyConHorizontal",
    controls: [
      { id: "sl", label: "SL", kind: "shoulder", slot: "rail-sl", tapId: "sl", actions: ["SL"] },
      { id: "sr", label: "SR", kind: "shoulder", slot: "rail-sr", tapId: "sr", actions: ["SR"] },
      { id: "zr", label: "ZR", kind: "shoulder", slot: "end-zr", tapId: "zr", actions: ["ZR"] },
      { id: "r", label: "R", kind: "shoulder", slot: "end-r", tapId: "r", actions: ["R"] },
      { id: "plus", label: "+", kind: "system", slot: "plus", tapId: "plus", actions: ["Plus"] },
      { id: "home", label: "Home", kind: "system", slot: "home", tapId: "home", actions: ["Home"] },
      {
        id: "stick",
        label: "Stick",
        kind: "stick",
        slot: "stick",
        tapId: "stick",
        actions: ["Stick", "MoveUp", "MoveDown", "MoveLeft", "MoveRight"],
      },
      { id: "x", label: "X", kind: "button", slot: "face-x", tapId: "x", actions: ["X"] },
      { id: "y", label: "Y", kind: "button", slot: "face-y", tapId: "y", actions: ["Y"] },
      { id: "a", label: "A", kind: "button", slot: "face-a", tapId: "a", actions: ["A"] },
      { id: "b", label: "B", kind: "button", slot: "face-b", tapId: "b", actions: ["B"] },
    ],
  },
  ProController: {
    model: "ProController",
    title: "Virtual Switch Pro Controller",
    className: "virtual-controller virtual-controller--pro-controller",
    orientation: "proController",
    controls: [
      { id: "zl", label: "ZL", kind: "shoulder", slot: "zl", tapId: "zl", actions: ["ZL"] },
      { id: "l", label: "L", kind: "shoulder", slot: "l", tapId: "l", actions: ["L"] },
      { id: "zr", label: "ZR", kind: "shoulder", slot: "zr", tapId: "zr", actions: ["ZR"] },
      { id: "r", label: "R", kind: "shoulder", slot: "r", tapId: "r", actions: ["R"] },
      { id: "minus", label: "-", kind: "system", slot: "minus", tapId: "minus", actions: ["Minus"] },
      { id: "plus", label: "+", kind: "system", slot: "plus", tapId: "plus", actions: ["Plus"] },
      { id: "capture", label: "Cap", kind: "system", slot: "capture", tapId: "capture", actions: ["Capture"] },
      { id: "home", label: "Home", kind: "system", slot: "home", tapId: "home", actions: ["Home"] },
      {
        id: "leftStick",
        label: "L Stick",
        kind: "stick",
        slot: "left-stick",
        tapId: "stick",
        actions: ["Stick", "MoveUp", "MoveDown", "MoveLeft", "MoveRight"],
      },
      { id: "up", label: "Up", kind: "dpad", slot: "dpad-up", actions: ["DpadUp"] },
      { id: "left", label: "Lt", kind: "dpad", slot: "dpad-left", actions: ["DpadLeft"] },
      { id: "right", label: "Rt", kind: "dpad", slot: "dpad-right", actions: ["DpadRight"] },
      { id: "down", label: "Dn", kind: "dpad", slot: "dpad-down", actions: ["DpadDown"] },
      { id: "x", label: "X", kind: "button", slot: "face-x", tapId: "x", actions: ["X"] },
      { id: "y", label: "Y", kind: "button", slot: "face-y", tapId: "y", actions: ["Y"] },
      { id: "a", label: "A", kind: "button", slot: "face-a", tapId: "a", actions: ["A"] },
      { id: "b", label: "B", kind: "button", slot: "face-b", tapId: "b", actions: ["B"] },
      { id: "rightStick", label: "R Stick", kind: "stick", slot: "right-stick" },
    ],
  },
};

export const VIRTUAL_KEYBOARD_ROWS: VirtualKeySpec[][] = [
  [
    { code: "Escape", label: "Esc", width: 1.1 },
    gap("frow_a", 0.4),
    { code: "F1", label: "F1" },
    { code: "F2", label: "F2" },
    { code: "F3", label: "F3" },
    { code: "F4", label: "F4" },
    gap("frow_b", 0.25),
    { code: "F5", label: "F5" },
    { code: "F6", label: "F6" },
    { code: "F7", label: "F7" },
    { code: "F8", label: "F8 Capture", width: 1.5 },
    gap("frow_c", 0.25),
    { code: "F9", label: "F9" },
    { code: "F10", label: "F10" },
    { code: "F11", label: "F11" },
    { code: "F12", label: "F12" },
    gap("frow_d", 0.25),
    { code: "PrintScreen", label: "Prt" },
  ],
  [
    { code: "Backquote", label: "`" },
    { code: "Digit1", label: "1" },
    { code: "Digit2", label: "2" },
    { code: "Digit3", label: "3" },
    { code: "Digit4", label: "4" },
    { code: "Digit5", label: "5" },
    { code: "Digit6", label: "6" },
    { code: "Digit7", label: "7" },
    { code: "Digit8", label: "8" },
    { code: "Digit9", label: "9" },
    { code: "Digit0", label: "0" },
    { code: "Minus", label: "-" },
    { code: "Equal", label: "=" },
    { code: "Backspace", label: "Backspace", width: 1.85 },
    gap("nav_1", 0.25),
    { code: "Insert", label: "Ins" },
    { code: "Home", label: "Home" },
    { code: "PageUp", label: "PgUp" },
  ],
  [
    { code: "Tab", label: "Tab", width: 1.45 },
    { code: "KeyQ", label: "Q" },
    { code: "KeyW", label: "W" },
    { code: "KeyE", label: "E" },
    { code: "KeyR", label: "R" },
    { code: "KeyT", label: "T" },
    { code: "KeyY", label: "Y" },
    { code: "KeyU", label: "U" },
    { code: "KeyI", label: "I" },
    { code: "KeyO", label: "O" },
    { code: "KeyP", label: "P" },
    { code: "BracketLeft", label: "[" },
    { code: "BracketRight", label: "]" },
    { code: "Backslash", label: "\\", width: 1.4 },
    gap("nav_2", 0.25),
    { code: "Delete", label: "Del" },
    { code: "End", label: "End" },
    { code: "PageDown", label: "PgDn" },
  ],
  [
    { code: "CapsLock", label: "Caps", width: 1.75 },
    { code: "KeyA", label: "A" },
    { code: "KeyS", label: "S" },
    { code: "KeyD", label: "D" },
    { code: "KeyF", label: "F" },
    { code: "KeyG", label: "G" },
    { code: "KeyH", label: "H" },
    { code: "KeyJ", label: "J" },
    { code: "KeyK", label: "K" },
    { code: "KeyL", label: "L" },
    { code: "Semicolon", label: ";" },
    { code: "Quote", label: "'" },
    { code: "Enter", label: "Enter", width: 2.25 },
  ],
  [
    { code: "ShiftLeft", label: "Shift", width: 2.25 },
    { code: "KeyZ", label: "Z" },
    { code: "KeyX", label: "X" },
    { code: "KeyC", label: "C" },
    { code: "KeyV", label: "V" },
    { code: "KeyB", label: "B" },
    { code: "KeyN", label: "N" },
    { code: "KeyM", label: "M" },
    { code: "Comma", label: "," },
    { code: "Period", label: "." },
    { code: "Slash", label: "/" },
    { code: "ShiftRight", label: "Shift", width: 2.2 },
    gap("arrow_up", 0.25),
    { code: "ArrowUp", label: "Up" },
  ],
  [
    { code: "ControlLeft", label: "Ctrl", width: 1.25 },
    { code: "MetaLeft", label: "Win", width: 1.25 },
    { code: "AltLeft", label: "Alt", width: 1.25 },
    { code: "Space", label: "Space", width: 6.4 },
    { code: "AltRight", label: "Alt", width: 1.25 },
    { code: "MetaRight", label: "Win", width: 1.25 },
    { code: "ControlRight", label: "Ctrl", width: 1.25 },
    gap("arrows", 0.25),
    { code: "ArrowLeft", label: "Left" },
    { code: "ArrowDown", label: "Down" },
    { code: "ArrowRight", label: "Right" },
  ],
];

export function buildKeyActionIndex(
  profile: Profile,
): Map<string, LogicalAction[]> {
  const bindings =
    Object.keys(profile.bindings).length > 0
      ? profile.bindings
      : defaultProfileForControllerModel(profile.controllerModel).bindings;
  const index = new Map<string, LogicalAction[]>();

  for (const [keyCode, action] of Object.entries(bindings)) {
    const actions = index.get(keyCode) ?? [];
    actions.push(action);
    index.set(keyCode, actions);
  }

  return index;
}

export function actionLabel(action: LogicalAction): string {
  if (action === "MoveUp") return "Move Up";
  if (action === "MoveDown") return "Move Down";
  if (action === "MoveLeft") return "Move Left";
  if (action === "MoveRight") return "Move Right";
  if (action === "DpadUp") return "D-pad Up";
  if (action === "DpadDown") return "D-pad Down";
  if (action === "DpadLeft") return "D-pad Left";
  if (action === "DpadRight") return "D-pad Right";
  return action;
}
