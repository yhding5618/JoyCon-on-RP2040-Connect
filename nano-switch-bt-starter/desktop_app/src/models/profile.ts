export type ControllerModel = "LeftJoyCon" | "RightJoyCon" | "ProController";

export type LogicalAction =
  | "MoveUp"
  | "MoveDown"
  | "MoveLeft"
  | "MoveRight"
  | "L"
  | "ZL"
  | "SL"
  | "SR"
  | "Minus"
  | "Stick"
  | "Capture";

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
