use crate::bridge_protocol::{
    BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_L, BTN_LJC_LEFT, BTN_LJC_MINUS, BTN_LJC_RIGHT,
    BTN_LJC_SL, BTN_LJC_SR, BTN_LJC_STICK, BTN_LJC_UP, BTN_LJC_ZL, BTN_RJC_A, BTN_RJC_B,
    BTN_RJC_HOME, BTN_RJC_PLUS, BTN_RJC_R, BTN_RJC_SL, BTN_RJC_SR, BTN_RJC_STICK, BTN_RJC_X,
    BTN_RJC_Y, BTN_RJC_ZR,
};
use crate::controller_mapper::map_input_to_controller;
use crate::model::{InputSnapshot, LatestInputState};
use crate::profiles::{
    default_left_joycon_profile, default_pro_controller_profile, default_right_joycon_profile,
};

const STICK_EXTENT: i16 = 32767;

#[test]
fn mapper_uses_default_left_joycon_bindings() {
    let profile = default_left_joycon_profile();
    let input = LatestInputState::from_snapshot(
        InputSnapshot {
            pressed_codes: vec![
                "KeyW".to_string(),
                "KeyL".to_string(),
                "KeyQ".to_string(),
                "KeyE".to_string(),
                "KeyU".to_string(),
                "KeyO".to_string(),
                "ShiftLeft".to_string(),
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
        BTN_LJC_DOWN | BTN_LJC_MINUS | BTN_LJC_SL | BTN_LJC_SR | BTN_LJC_STICK | BTN_LJC_CAPTURE
    );
    assert_eq!((controller.lx, controller.ly), (STICK_EXTENT, 0));
}

#[test]
fn mapper_maps_lkij_to_single_left_joycon_face_buttons() {
    let profile = default_left_joycon_profile();

    assert_eq!(state_for(&profile, &["KeyL"]).buttons, BTN_LJC_DOWN);
    assert_eq!(state_for(&profile, &["KeyK"]).buttons, BTN_LJC_LEFT);
    assert_eq!(state_for(&profile, &["KeyI"]).buttons, BTN_LJC_RIGHT);
    assert_eq!(state_for(&profile, &["KeyJ"]).buttons, BTN_LJC_UP);
}

#[test]
fn mapper_maps_requested_utility_keys() {
    let profile = default_left_joycon_profile();

    assert_eq!(state_for(&profile, &["KeyQ"]).buttons, BTN_LJC_MINUS);
    assert_eq!(state_for(&profile, &["KeyO"]).buttons, BTN_LJC_CAPTURE);
    assert_eq!(state_for(&profile, &["KeyE"]).buttons, BTN_LJC_SL);
    assert_eq!(state_for(&profile, &["KeyU"]).buttons, BTN_LJC_SR);
}

#[test]
fn mapper_maps_wasd_to_left_stick() {
    let profile = default_left_joycon_profile();

    let keyboard_up = state_for(&profile, &["KeyW"]);
    assert_eq!(
        (keyboard_up.buttons, keyboard_up.lx, keyboard_up.ly),
        (0, STICK_EXTENT, 0)
    );

    let keyboard_left = state_for(&profile, &["KeyA"]);
    assert_eq!(
        (keyboard_left.buttons, keyboard_left.lx, keyboard_left.ly),
        (0, 0, STICK_EXTENT)
    );

    let keyboard_down = state_for(&profile, &["KeyS"]);
    assert_eq!(
        (keyboard_down.buttons, keyboard_down.lx, keyboard_down.ly),
        (0, -STICK_EXTENT, 0)
    );

    let keyboard_right = state_for(&profile, &["KeyD"]);
    assert_eq!(
        (keyboard_right.buttons, keyboard_right.lx, keyboard_right.ly),
        (0, 0, -STICK_EXTENT)
    );

    let neutral_y = state_for(&profile, &["KeyW", "KeyS"]);
    assert_eq!((neutral_y.lx, neutral_y.ly), (0, 0));
}

#[test]
fn mapper_uses_default_right_joycon_bindings() {
    let profile = default_right_joycon_profile();
    let controller = state_for(
        &profile,
        &["KeyW", "KeyK", "KeyQ", "KeyE", "KeyU", "KeyO", "ShiftRight"],
    );

    assert_eq!(
        controller.buttons,
        BTN_RJC_A | BTN_RJC_PLUS | BTN_RJC_SL | BTN_RJC_SR | BTN_RJC_STICK | BTN_RJC_HOME
    );
    assert_eq!((controller.lx, controller.ly), (0, 0));
    assert_eq!((controller.rx, controller.ry), (-STICK_EXTENT, 0));
    assert_eq!(state_for(&profile, &["Home"]).buttons, BTN_RJC_HOME);
}

#[test]
fn mapper_maps_lkij_to_right_joycon_face_buttons() {
    let profile = default_right_joycon_profile();

    assert_eq!(state_for(&profile, &["KeyI"]).buttons, BTN_RJC_Y);
    assert_eq!(state_for(&profile, &["KeyJ"]).buttons, BTN_RJC_B);
    assert_eq!(state_for(&profile, &["KeyK"]).buttons, BTN_RJC_A);
    assert_eq!(state_for(&profile, &["KeyL"]).buttons, BTN_RJC_X);
}

#[test]
fn mapper_maps_wasd_to_right_hand_rotated_right_stick() {
    let profile = default_right_joycon_profile();

    let keyboard_up = state_for(&profile, &["KeyW"]);
    assert_eq!(
        (keyboard_up.buttons, keyboard_up.rx, keyboard_up.ry),
        (0, -STICK_EXTENT, 0)
    );

    let keyboard_left = state_for(&profile, &["KeyA"]);
    assert_eq!(
        (keyboard_left.buttons, keyboard_left.rx, keyboard_left.ry),
        (0, 0, -STICK_EXTENT)
    );

    let keyboard_down = state_for(&profile, &["KeyS"]);
    assert_eq!(
        (keyboard_down.buttons, keyboard_down.rx, keyboard_down.ry),
        (0, STICK_EXTENT, 0)
    );

    let keyboard_right = state_for(&profile, &["KeyD"]);
    assert_eq!(
        (keyboard_right.buttons, keyboard_right.rx, keyboard_right.ry),
        (0, 0, STICK_EXTENT)
    );
}

#[test]
fn mapper_uses_default_pro_controller_bindings() {
    let profile = default_pro_controller_profile();
    let controller = state_for(
        &profile,
        &[
            "KeyW",
            "KeyL",
            "KeyE",
            "KeyU",
            "F9",
            "F10",
            "Digit3",
            "Digit2",
            "Digit8",
            "Digit9",
            "ArrowUp",
            "ArrowRight",
            "ShiftLeft",
        ],
    );

    assert_eq!(
        controller.buttons,
        BTN_RJC_A
            | BTN_LJC_MINUS
            | BTN_RJC_PLUS
            | BTN_LJC_L
            | BTN_LJC_ZL
            | BTN_RJC_R
            | BTN_RJC_ZR
            | BTN_LJC_CAPTURE
            | BTN_RJC_HOME
            | BTN_LJC_STICK
            | BTN_LJC_UP
            | BTN_LJC_RIGHT
    );
    assert_eq!((controller.lx, controller.ly), (0, STICK_EXTENT));
    assert_eq!((controller.rx, controller.ry), (0, 0));
    assert_eq!(state_for(&profile, &["F9"]).buttons, BTN_LJC_CAPTURE);
    assert_eq!(state_for(&profile, &["F10"]).buttons, BTN_RJC_HOME);
}

#[test]
fn mapper_maps_mouse_move_to_pro_right_stick() {
    let profile = default_pro_controller_profile();

    let controller = mouse_state_for(&profile, 12.0, -8.0, true, true);
    assert_eq!((controller.lx, controller.ly), (0, 0));
    assert_eq!((controller.rx, controller.ry), (12288, 8192));

    let small_delta = mouse_state_for(&profile, 1.0, -1.0, true, true);
    assert_eq!((small_delta.rx, small_delta.ry), (1024, 1024));

    let without_alt = mouse_state_for(&profile, 12.0, -8.0, true, false);
    assert_eq!((without_alt.rx, without_alt.ry), (0, 0));

    let right_alt = mouse_state_for_codes(&profile, 12.0, -8.0, true, &["AltRight"]);
    assert_eq!((right_alt.rx, right_alt.ry), (0, 0));

    let without_pointer_lock = mouse_state_for(&profile, 12.0, -8.0, false, true);
    assert_eq!(
        (without_pointer_lock.rx, without_pointer_lock.ry),
        (12288, 8192)
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

fn state_for(
    profile: &crate::model::Profile,
    key_codes: &[&str],
) -> crate::bridge_protocol::ControllerStatePayload {
    let input = LatestInputState::from_snapshot(
        InputSnapshot {
            pressed_codes: key_codes
                .iter()
                .map(|key_code| key_code.to_string())
                .collect(),
            mouse_buttons: vec![],
            mouse_delta_x: 0.0,
            mouse_delta_y: 0.0,
            pointer_locked: false,
            capture_enabled: true,
            timestamp_ms: 1,
        },
        true,
    );

    map_input_to_controller(&input, profile)
}

fn mouse_state_for(
    profile: &crate::model::Profile,
    mouse_delta_x: f32,
    mouse_delta_y: f32,
    pointer_locked: bool,
    alt_held: bool,
) -> crate::bridge_protocol::ControllerStatePayload {
    let key_codes = if alt_held { &["AltLeft"][..] } else { &[][..] };
    mouse_state_for_codes(
        profile,
        mouse_delta_x,
        mouse_delta_y,
        pointer_locked,
        key_codes,
    )
}

fn mouse_state_for_codes(
    profile: &crate::model::Profile,
    mouse_delta_x: f32,
    mouse_delta_y: f32,
    pointer_locked: bool,
    key_codes: &[&str],
) -> crate::bridge_protocol::ControllerStatePayload {
    let input = LatestInputState::from_snapshot(
        InputSnapshot {
            pressed_codes: key_codes
                .iter()
                .map(|key_code| key_code.to_string())
                .collect(),
            mouse_buttons: vec![],
            mouse_delta_x,
            mouse_delta_y,
            pointer_locked,
            capture_enabled: true,
            timestamp_ms: 1,
        },
        true,
    );

    map_input_to_controller(&input, profile)
}
