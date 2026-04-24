use std::collections::BTreeMap;

use crate::model::{ControllerModel, LogicalAction, MouseSettings, Profile};

pub fn default_left_joycon_profile() -> Profile {
    let mut bindings = BTreeMap::new();
    bindings.insert("KeyW".to_string(), LogicalAction::MoveRight);
    bindings.insert("KeyS".to_string(), LogicalAction::MoveLeft);
    bindings.insert("KeyA".to_string(), LogicalAction::MoveUp);
    bindings.insert("KeyD".to_string(), LogicalAction::MoveDown);
    bindings.insert("KeyL".to_string(), LogicalAction::A);
    bindings.insert("KeyK".to_string(), LogicalAction::B);
    bindings.insert("KeyI".to_string(), LogicalAction::X);
    bindings.insert("KeyJ".to_string(), LogicalAction::Y);
    bindings.insert("KeyQ".to_string(), LogicalAction::Minus);
    bindings.insert("KeyE".to_string(), LogicalAction::SL);
    bindings.insert("KeyU".to_string(), LogicalAction::SR);
    bindings.insert("KeyO".to_string(), LogicalAction::Capture);
    bindings.insert("ShiftLeft".to_string(), LogicalAction::Stick);

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

pub fn default_right_joycon_profile() -> Profile {
    let mut bindings = BTreeMap::new();
    bindings.insert("KeyW".to_string(), LogicalAction::MoveLeft);
    bindings.insert("KeyS".to_string(), LogicalAction::MoveRight);
    bindings.insert("KeyA".to_string(), LogicalAction::MoveDown);
    bindings.insert("KeyD".to_string(), LogicalAction::MoveUp);
    bindings.insert("KeyL".to_string(), LogicalAction::A);
    bindings.insert("KeyK".to_string(), LogicalAction::B);
    bindings.insert("KeyI".to_string(), LogicalAction::X);
    bindings.insert("KeyJ".to_string(), LogicalAction::Y);
    bindings.insert("KeyQ".to_string(), LogicalAction::Plus);
    bindings.insert("KeyE".to_string(), LogicalAction::SL);
    bindings.insert("KeyU".to_string(), LogicalAction::SR);
    bindings.insert("KeyO".to_string(), LogicalAction::Home);
    bindings.insert("ShiftRight".to_string(), LogicalAction::Stick);

    Profile {
        id: "default-right-joycon".to_string(),
        name: "Default Right Joy-Con".to_string(),
        controller_model: ControllerModel::RightJoyCon,
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
