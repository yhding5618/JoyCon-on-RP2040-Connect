use crate::bridge_protocol::{
    BTN_LJC_CAPTURE, BTN_LJC_L, BTN_LJC_MINUS, BTN_LJC_STICK, BTN_LJC_UP,
};
use crate::controller_mapper::map_input_to_controller;
use crate::model::{InputSnapshot, LatestInputState};
use crate::profiles::default_left_joycon_profile;

#[test]
fn mapper_uses_default_left_joycon_bindings() {
    let profile = default_left_joycon_profile();
    let input = LatestInputState::from_snapshot(
        InputSnapshot {
            pressed_codes: vec![
                "KeyW".to_string(),
                "KeyQ".to_string(),
                "Tab".to_string(),
                "ShiftLeft".to_string(),
                "F12".to_string(),
            ],
            mouse_buttons: vec![],
            mouse_delta_x: 0.0,
            mouse_delta_y: 0.0,
            pointer_locked: false,
            capture_enabled: true,
            timestamp_ms: 1,
        },
        true,
    );

    let controller = map_input_to_controller(&input, &profile);
    assert_eq!(
        controller.buttons,
        BTN_LJC_UP | BTN_LJC_L | BTN_LJC_MINUS | BTN_LJC_STICK | BTN_LJC_CAPTURE
    );
}

#[test]
fn mapper_returns_neutral_state_when_capture_is_disabled() {
    let profile = default_left_joycon_profile();
    let input = LatestInputState::from_snapshot(
        InputSnapshot {
            pressed_codes: vec!["KeyW".to_string()],
            mouse_buttons: vec![],
            mouse_delta_x: 0.0,
            mouse_delta_y: 0.0,
            pointer_locked: false,
            capture_enabled: false,
            timestamp_ms: 1,
        },
        true,
    );

    let controller = map_input_to_controller(&input, &profile);
    assert_eq!(controller.buttons, 0);
    assert_eq!(controller.hat, 8);
    assert_eq!(controller.battery_level, 8);
}
