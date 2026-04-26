use crate::automation::{
    automation_sent_states, build_automation_steps, validate_loop_count, AutomationStepType,
    DEFAULT_TAP_DURATION_MS,
};
use crate::bridge_protocol::{
    ControllerStatePayload, BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_UP, BTN_RJC_A, BTN_RJC_B,
    BTN_RJC_X, BTN_RJC_Y,
};
use crate::model::{AutomationAction, AutomationActionType, ControllerModel};

#[test]
fn automation_builds_multi_button_steps() {
    let steps = build_automation_steps(
        &[
            action(AutomationActionType::Hold, &["a", "b"], None),
            action(AutomationActionType::Release, &["a"], None),
            action(AutomationActionType::Tap, &["x", "y"], Some(80)),
        ],
        ControllerModel::ProController,
    )
    .expect("sequence should validate");

    assert_eq!(steps[0].step_type, AutomationStepType::Hold);
    assert_eq!(steps[0].buttons, BTN_RJC_A | BTN_RJC_B);
    assert_eq!(steps[1].step_type, AutomationStepType::Release);
    assert_eq!(steps[1].buttons, BTN_RJC_A);
    assert_eq!(steps[2].step_type, AutomationStepType::Tap);
    assert_eq!(steps[2].buttons, BTN_RJC_X | BTN_RJC_Y);
    assert_eq!(steps[2].duration_ms, 80);
}

#[test]
fn automation_tap_uses_default_duration() {
    let steps = build_automation_steps(
        &[action(AutomationActionType::Tap, &["capture"], None)],
        ControllerModel::ProController,
    )
    .expect("tap should validate");

    assert_eq!(steps[0].buttons, BTN_LJC_CAPTURE);
    assert_eq!(steps[0].duration_ms, DEFAULT_TAP_DURATION_MS);
}

#[test]
fn automation_tap_preserves_existing_held_buttons() {
    let steps = build_automation_steps(
        &[
            action(AutomationActionType::Hold, &["a"], None),
            action(AutomationActionType::Tap, &["b"], Some(25)),
            action(AutomationActionType::Release, &["a"], None),
        ],
        ControllerModel::ProController,
    )
    .expect("sequence should validate");

    let states = automation_sent_states(&steps, 1);
    assert_eq!(
        button_states(&states),
        vec![BTN_RJC_A, BTN_RJC_A | BTN_RJC_B, BTN_RJC_A, 0]
    );
}

#[test]
fn automation_delay_does_not_emit_state() {
    let steps = build_automation_steps(
        &[AutomationAction {
            action_type: AutomationActionType::Delay,
            buttons: Vec::new(),
            duration_ms: Some(50),
        }],
        ControllerModel::ProController,
    )
    .expect("delay should validate");

    assert!(automation_sent_states(&steps, 1).is_empty());
}

#[test]
fn automation_loop_releases_held_buttons_each_loop() {
    let steps = build_automation_steps(
        &[
            action(AutomationActionType::Hold, &["a"], None),
            AutomationAction {
                action_type: AutomationActionType::Delay,
                buttons: Vec::new(),
                duration_ms: Some(1),
            },
        ],
        ControllerModel::ProController,
    )
    .expect("sequence should validate");

    let states = automation_sent_states(&steps, 2);
    assert_eq!(button_states(&states), vec![BTN_RJC_A, 0, BTN_RJC_A, 0]);
}

#[test]
fn automation_rejects_invalid_sequences() {
    assert!(build_automation_steps(&[], ControllerModel::ProController).is_err());
    assert!(validate_loop_count(0).is_err());

    let empty_button = action(AutomationActionType::Hold, &[], None);
    assert!(build_automation_steps(&[empty_button], ControllerModel::ProController).is_err());

    let zero_delay = AutomationAction {
        action_type: AutomationActionType::Delay,
        buttons: Vec::new(),
        duration_ms: Some(0),
    };
    assert!(build_automation_steps(&[zero_delay], ControllerModel::ProController).is_err());

    let unsupported = action(AutomationActionType::Tap, &["sl"], Some(30));
    assert!(build_automation_steps(&[unsupported], ControllerModel::ProController).is_err());
}

#[test]
fn automation_maps_dpad_buttons() {
    let steps = build_automation_steps(
        &[action(AutomationActionType::Hold, &["up", "down"], None)],
        ControllerModel::ProController,
    )
    .expect("dpad should validate");

    assert_eq!(steps[0].buttons, BTN_LJC_UP | BTN_LJC_DOWN);
}

fn action(
    action_type: AutomationActionType,
    buttons: &[&str],
    duration_ms: Option<u64>,
) -> AutomationAction {
    AutomationAction {
        action_type,
        buttons: buttons.iter().map(|button| button.to_string()).collect(),
        duration_ms,
    }
}

fn button_states(states: &[ControllerStatePayload]) -> Vec<u32> {
    states.iter().map(|state| state.buttons).collect()
}
