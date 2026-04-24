use std::collections::HashSet;

use crate::bridge_protocol::{
    ControllerStatePayload, BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_L, BTN_LJC_LEFT, BTN_LJC_MINUS,
    BTN_LJC_RIGHT, BTN_LJC_SL, BTN_LJC_SR, BTN_LJC_STICK, BTN_LJC_UP, BTN_LJC_ZL,
};
use crate::model::{LatestInputState, LogicalAction, Profile};

pub fn map_input_to_controller(
    input: &LatestInputState,
    profile: &Profile,
) -> ControllerStatePayload {
    if !input.capture_enabled || !input.window_focused {
        return ControllerStatePayload::default();
    }

    let mut state = ControllerStatePayload::default();
    let pressed_codes = input
        .pressed_codes
        .iter()
        .map(String::as_str)
        .collect::<HashSet<_>>();

    for (key_code, action) in &profile.bindings {
        if pressed_codes.contains(key_code.as_str()) {
            apply_action(&mut state, *action);
        }
    }

    state
}

fn apply_action(state: &mut ControllerStatePayload, action: LogicalAction) {
    match action {
        LogicalAction::MoveDown => state.buttons |= BTN_LJC_DOWN,
        LogicalAction::MoveUp => state.buttons |= BTN_LJC_UP,
        LogicalAction::MoveRight => state.buttons |= BTN_LJC_RIGHT,
        LogicalAction::MoveLeft => state.buttons |= BTN_LJC_LEFT,
        LogicalAction::SL => state.buttons |= BTN_LJC_SL,
        LogicalAction::SR => state.buttons |= BTN_LJC_SR,
        LogicalAction::L => state.buttons |= BTN_LJC_L,
        LogicalAction::ZL => state.buttons |= BTN_LJC_ZL,
        LogicalAction::Minus => state.buttons |= BTN_LJC_MINUS,
        LogicalAction::Stick => state.buttons |= BTN_LJC_STICK,
        LogicalAction::Capture => state.buttons |= BTN_LJC_CAPTURE,
    }
}
