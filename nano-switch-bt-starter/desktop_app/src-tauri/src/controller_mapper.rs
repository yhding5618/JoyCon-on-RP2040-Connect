use std::collections::HashSet;

use crate::bridge_protocol::{
    ControllerStatePayload, BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_L, BTN_LJC_LEFT, BTN_LJC_MINUS,
    BTN_LJC_RIGHT, BTN_LJC_SL, BTN_LJC_SR, BTN_LJC_STICK, BTN_LJC_UP, BTN_LJC_ZL, BTN_RJC_A,
    BTN_RJC_B, BTN_RJC_HOME, BTN_RJC_PLUS, BTN_RJC_R, BTN_RJC_SL, BTN_RJC_SR, BTN_RJC_STICK,
    BTN_RJC_X, BTN_RJC_Y, BTN_RJC_ZR,
};
use crate::model::{ControllerModel, LatestInputState, LogicalAction, Profile};

const KEYBOARD_STICK_EXTENT: i32 = 32767;

pub fn map_input_to_controller(
    input: &LatestInputState,
    profile: &Profile,
) -> ControllerStatePayload {
    if !input.capture_enabled || !input.window_focused {
        return ControllerStatePayload::default();
    }

    let mut state = ControllerStatePayload::default();
    let mut stick_x = 0;
    let mut stick_y = 0;
    let pressed_codes = input
        .pressed_codes
        .iter()
        .map(String::as_str)
        .collect::<HashSet<_>>();

    for (key_code, action) in &profile.bindings {
        if pressed_codes.contains(key_code.as_str()) {
            apply_action(
                &mut state,
                &mut stick_x,
                &mut stick_y,
                *action,
                profile.controller_model,
            );
        }
    }

    match profile.controller_model {
        ControllerModel::LeftJoyCon | ControllerModel::ProController => {
            state.lx = clamp_stick_axis(stick_x);
            state.ly = clamp_stick_axis(stick_y);
        }
        ControllerModel::RightJoyCon => {
            state.rx = clamp_stick_axis(stick_x);
            state.ry = clamp_stick_axis(stick_y);
        }
    }

    state
}

fn apply_action(
    state: &mut ControllerStatePayload,
    stick_x: &mut i32,
    stick_y: &mut i32,
    action: LogicalAction,
    controller_model: ControllerModel,
) {
    match action {
        LogicalAction::MoveDown => *stick_y -= KEYBOARD_STICK_EXTENT,
        LogicalAction::MoveUp => *stick_y += KEYBOARD_STICK_EXTENT,
        LogicalAction::MoveRight => *stick_x += KEYBOARD_STICK_EXTENT,
        LogicalAction::MoveLeft => *stick_x -= KEYBOARD_STICK_EXTENT,
        LogicalAction::A => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon | ControllerModel::ProController => BTN_RJC_A,
                _ => BTN_LJC_DOWN,
            }
        }
        LogicalAction::B => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon | ControllerModel::ProController => BTN_RJC_B,
                _ => BTN_LJC_LEFT,
            }
        }
        LogicalAction::X => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon | ControllerModel::ProController => BTN_RJC_X,
                _ => BTN_LJC_RIGHT,
            }
        }
        LogicalAction::Y => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon | ControllerModel::ProController => BTN_RJC_Y,
                _ => BTN_LJC_UP,
            }
        }
        LogicalAction::SL => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon => BTN_RJC_SL,
                _ => BTN_LJC_SL,
            }
        }
        LogicalAction::SR => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon => BTN_RJC_SR,
                _ => BTN_LJC_SR,
            }
        }
        LogicalAction::L => state.buttons |= BTN_LJC_L,
        LogicalAction::ZL => state.buttons |= BTN_LJC_ZL,
        LogicalAction::R => state.buttons |= BTN_RJC_R,
        LogicalAction::ZR => state.buttons |= BTN_RJC_ZR,
        LogicalAction::Minus => state.buttons |= BTN_LJC_MINUS,
        LogicalAction::Plus => state.buttons |= BTN_RJC_PLUS,
        LogicalAction::Stick => {
            state.buttons |= match controller_model {
                ControllerModel::RightJoyCon => BTN_RJC_STICK,
                _ => BTN_LJC_STICK,
            }
        }
        LogicalAction::Capture => state.buttons |= BTN_LJC_CAPTURE,
        LogicalAction::Home => state.buttons |= BTN_RJC_HOME,
    }
}

fn clamp_stick_axis(value: i32) -> i16 {
    value.clamp(-KEYBOARD_STICK_EXTENT, KEYBOARD_STICK_EXTENT) as i16
}
