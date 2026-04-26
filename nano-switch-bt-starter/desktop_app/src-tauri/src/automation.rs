use crate::bridge_protocol::{
    ControllerStatePayload, BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_L, BTN_LJC_LEFT, BTN_LJC_MINUS,
    BTN_LJC_RIGHT, BTN_LJC_SL, BTN_LJC_SR, BTN_LJC_STICK, BTN_LJC_UP, BTN_LJC_ZL, BTN_RJC_A,
    BTN_RJC_B, BTN_RJC_HOME, BTN_RJC_PLUS, BTN_RJC_R, BTN_RJC_SL, BTN_RJC_SR, BTN_RJC_STICK,
    BTN_RJC_X, BTN_RJC_Y, BTN_RJC_ZR,
};
use crate::model::{AutomationAction, AutomationActionType, ControllerModel};

pub const DEFAULT_TAP_DURATION_MS: u64 = 120;
pub const MAX_AUTOMATION_DURATION_MS: u64 = 60_000;
pub const MAX_AUTOMATION_ACTIONS: usize = 128;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AutomationStepType {
    Hold,
    Delay,
    Release,
    Tap,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AutomationStep {
    pub step_type: AutomationStepType,
    pub buttons: u32,
    pub duration_ms: u64,
}

pub fn build_automation_steps(
    sequence: &[AutomationAction],
    controller_model: ControllerModel,
) -> Result<Vec<AutomationStep>, String> {
    if sequence.is_empty() {
        return Err("automation sequence must contain at least one action".to_string());
    }

    if sequence.len() > MAX_AUTOMATION_ACTIONS {
        return Err(format!(
            "automation sequence cannot exceed {MAX_AUTOMATION_ACTIONS} actions"
        ));
    }

    sequence
        .iter()
        .enumerate()
        .map(|(index, action)| automation_step_for_action(index, action, controller_model))
        .collect()
}

pub fn validate_loop_count(loop_count: u32) -> Result<(), String> {
    if loop_count == 0 {
        return Err("automation loop count must be at least 1".to_string());
    }

    Ok(())
}

pub fn automation_sent_states(
    steps: &[AutomationStep],
    loop_count: u32,
) -> Vec<ControllerStatePayload> {
    let mut states = Vec::new();
    let mut held_buttons = 0u32;

    for _ in 0..loop_count {
        for step in steps {
            append_step_states(&mut states, &mut held_buttons, step);
        }

        if held_buttons != 0 {
            held_buttons = 0;
            states.push(ControllerStatePayload::default());
        }
    }

    states
}

pub fn append_step_states(
    states: &mut Vec<ControllerStatePayload>,
    held_buttons: &mut u32,
    step: &AutomationStep,
) {
    match step.step_type {
        AutomationStepType::Hold => {
            *held_buttons |= step.buttons;
            states.push(state_with_buttons(*held_buttons));
        }
        AutomationStepType::Release => {
            *held_buttons &= !step.buttons;
            states.push(state_with_buttons(*held_buttons));
        }
        AutomationStepType::Tap => {
            states.push(state_with_buttons(*held_buttons | step.buttons));
            states.push(state_with_buttons(*held_buttons));
        }
        AutomationStepType::Delay => {}
    }
}

pub fn controller_button_bits(
    button: &str,
    controller_model: ControllerModel,
) -> Result<u32, String> {
    match controller_model {
        ControllerModel::RightJoyCon => match button {
            "a" => Ok(BTN_RJC_A),
            "b" => Ok(BTN_RJC_B),
            "x" => Ok(BTN_RJC_X),
            "y" => Ok(BTN_RJC_Y),
            "sl" => Ok(BTN_RJC_SL),
            "sr" => Ok(BTN_RJC_SR),
            "r" | "l" => Ok(BTN_RJC_R),
            "zr" | "zl" => Ok(BTN_RJC_ZR),
            "plus" | "minus" => Ok(BTN_RJC_PLUS),
            "stick" => Ok(BTN_RJC_STICK),
            "home" | "capture" => Ok(BTN_RJC_HOME),
            _ => Err(format!("unsupported Right Joy-Con button: {button}")),
        },
        ControllerModel::ProController => match button {
            "a" => Ok(BTN_RJC_A),
            "b" => Ok(BTN_RJC_B),
            "x" => Ok(BTN_RJC_X),
            "y" => Ok(BTN_RJC_Y),
            "down" => Ok(BTN_LJC_DOWN),
            "up" => Ok(BTN_LJC_UP),
            "right" => Ok(BTN_LJC_RIGHT),
            "left" => Ok(BTN_LJC_LEFT),
            "l" => Ok(BTN_LJC_L),
            "r" => Ok(BTN_RJC_R),
            "zl" => Ok(BTN_LJC_ZL),
            "zr" => Ok(BTN_RJC_ZR),
            "minus" => Ok(BTN_LJC_MINUS),
            "plus" => Ok(BTN_RJC_PLUS),
            "stick" => Ok(BTN_LJC_STICK),
            "capture" => Ok(BTN_LJC_CAPTURE),
            "home" => Ok(BTN_RJC_HOME),
            _ => Err(format!("unsupported Pro Controller button: {button}")),
        },
        _ => match button {
            "a" | "down" => Ok(BTN_LJC_DOWN),
            "y" | "up" => Ok(BTN_LJC_UP),
            "x" | "right" => Ok(BTN_LJC_RIGHT),
            "b" | "left" => Ok(BTN_LJC_LEFT),
            "sl" => Ok(BTN_LJC_SL),
            "sr" => Ok(BTN_LJC_SR),
            "l" | "r" => Ok(BTN_LJC_L),
            "zl" | "zr" => Ok(BTN_LJC_ZL),
            "minus" | "plus" => Ok(BTN_LJC_MINUS),
            "stick" => Ok(BTN_LJC_STICK),
            "capture" | "home" => Ok(BTN_LJC_CAPTURE),
            _ => Err(format!("unsupported Left Joy-Con button: {button}")),
        },
    }
}

pub fn state_with_buttons(buttons: u32) -> ControllerStatePayload {
    ControllerStatePayload {
        buttons,
        ..ControllerStatePayload::default()
    }
}

fn automation_step_for_action(
    index: usize,
    action: &AutomationAction,
    controller_model: ControllerModel,
) -> Result<AutomationStep, String> {
    match action.action_type {
        AutomationActionType::Hold => Ok(AutomationStep {
            step_type: AutomationStepType::Hold,
            buttons: buttons_for_action(index, action, controller_model)?,
            duration_ms: 0,
        }),
        AutomationActionType::Release => Ok(AutomationStep {
            step_type: AutomationStepType::Release,
            buttons: buttons_for_action(index, action, controller_model)?,
            duration_ms: 0,
        }),
        AutomationActionType::Tap => Ok(AutomationStep {
            step_type: AutomationStepType::Tap,
            buttons: buttons_for_action(index, action, controller_model)?,
            duration_ms: action_duration_ms(
                index,
                action.duration_ms.unwrap_or(DEFAULT_TAP_DURATION_MS),
            )?,
        }),
        AutomationActionType::Delay => Ok(AutomationStep {
            step_type: AutomationStepType::Delay,
            buttons: 0,
            duration_ms: action_duration_ms(
                index,
                action
                    .duration_ms
                    .ok_or_else(|| format!("action {} delay duration is required", index + 1))?,
            )?,
        }),
    }
}

fn buttons_for_action(
    index: usize,
    action: &AutomationAction,
    controller_model: ControllerModel,
) -> Result<u32, String> {
    if action.buttons.is_empty() {
        return Err(format!("action {} requires at least one button", index + 1));
    }

    action.buttons.iter().try_fold(0u32, |buttons, button| {
        controller_button_bits(button, controller_model).map(|bits| buttons | bits)
    })
}

fn action_duration_ms(index: usize, duration_ms: u64) -> Result<u64, String> {
    if duration_ms == 0 {
        return Err(format!(
            "action {} duration must be at least 1 ms",
            index + 1
        ));
    }

    if duration_ms > MAX_AUTOMATION_DURATION_MS {
        return Err(format!(
            "action {} duration cannot exceed {MAX_AUTOMATION_DURATION_MS} ms",
            index + 1
        ));
    }

    Ok(duration_ms)
}
