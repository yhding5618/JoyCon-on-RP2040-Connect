export type ControllerModel = "LeftJoyCon" | "RightJoyCon" | "ProController";

export type LogicalAction =
  | "MoveUp"
  | "MoveDown"
  | "MoveLeft"
  | "MoveRight"
  | "DpadUp"
  | "DpadDown"
  | "DpadLeft"
  | "DpadRight"
  | "A"
  | "B"
  | "X"
  | "Y"
  | "L"
  | "ZL"
  | "R"
  | "ZR"
  | "SL"
  | "SR"
  | "Minus"
  | "Plus"
  | "Stick"
  | "Capture"
  | "Home";

export type MouseSettings = {
  enabled: boolean;
  sensitivityX: number;
  sensitivityY: number;
  invertY: boolean;
  deadzone: number;
  smoothing: number;
  decayMs: number;
};

export type Profile = {
  id: string;
  name: string;
  controllerModel: ControllerModel;
  bindings: Record<string, LogicalAction>;
  mouse: MouseSettings;
  outputRateHz: number;
};
