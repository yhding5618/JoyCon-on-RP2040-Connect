use std::collections::BTreeMap;

use crate::model::{ControllerModel, LogicalAction, MouseSettings, Profile};

pub fn default_left_joycon_profile() -> Profile {
    let mut bindings = BTreeMap::new();
    bindings.insert("KeyW".to_string(), LogicalAction::MoveUp);
    bindings.insert("KeyS".to_string(), LogicalAction::MoveDown);
    bindings.insert("KeyA".to_string(), LogicalAction::MoveLeft);
    bindings.insert("KeyD".to_string(), LogicalAction::MoveRight);
    bindings.insert("KeyQ".to_string(), LogicalAction::L);
    bindings.insert("KeyE".to_string(), LogicalAction::ZL);
    bindings.insert("KeyZ".to_string(), LogicalAction::SL);
    bindings.insert("KeyC".to_string(), LogicalAction::SR);
    bindings.insert("Tab".to_string(), LogicalAction::Minus);
    bindings.insert("ShiftLeft".to_string(), LogicalAction::Stick);
    bindings.insert("F12".to_string(), LogicalAction::Capture);

    Profile {
        id: "default-left-joycon".to_string(),
        name: "Default Left Joy-Con".to_string(),
        controller_model: ControllerModel::LeftJoyCon,
        bindings,
        mouse: MouseSettings {
            enabled: false,
            sensitivity_x: 1.0,
            sensitivity_y: 1.0,
            invert_y: false,
            deadzone: 0.05,
            smoothing: 0.2,
            decay_ms: 40,
        },
        output_rate_hz: 125,
    }
}
