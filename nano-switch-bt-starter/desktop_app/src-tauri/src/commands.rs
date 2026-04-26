use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::thread;
use std::time::Duration;

use tauri::State;

use crate::app_state::sync_runtime_metrics;
use crate::app_state::ManagedAppState;
use crate::automation::{
    build_automation_steps, controller_button_bits, state_with_buttons, validate_loop_count,
    AutomationStep, AutomationStepType,
};
use crate::bridge_protocol::{
    ControllerStatePayload, CONTROLLER_MODE_LEFT_JOYCON, CONTROLLER_MODE_PRO_CONTROLLER,
    CONTROLLER_MODE_RIGHT_JOYCON,
};
use crate::controller_mapper::map_input_to_controller;
use crate::diagnostics::{
    note_events, note_input_update, note_serial_error, note_serial_frames, note_status, push_log,
};
use crate::errors::AppError;
use crate::model::{
    AppStateSnapshot, AutomationAction, AutomationState, ConnectionState, ControllerModel,
    InputSnapshot, LatestInputState,
};
use crate::profiles::default_profile_for_model;
use crate::serial_worker::{WorkerCommand, WorkerReply};

#[tauri::command]
pub fn get_app_state_snapshot(state: State<'_, ManagedAppState>) -> AppStateSnapshot {
    state.snapshot()
}

#[tauri::command]
pub fn clear_command_log(state: State<'_, ManagedAppState>) -> AppStateSnapshot {
    state.update(|app_state| {
        app_state.diagnostics.command_log.clear();
        push_log(&mut app_state.diagnostics, "command log cleared");
        app_state.clone()
    })
}

#[tauri::command]
pub async fn set_capture_enabled(
    enabled: bool,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    if enabled && state.snapshot().automation.running {
        return Err("capture is disabled while automation is running".to_string());
    }

    let released = if !enabled {
        release_controller_state_if_needed(&state)
            .await
            .ok()
            .flatten()
    } else {
        None
    };

    Ok(state.update(|app_state| {
        app_state.input.capture_enabled = enabled;
        app_state.input.window_focused = enabled;
        if !enabled {
            app_state.input.release_all();
            app_state.controller = Default::default();
            if let Some((tx_frames, rx_frames)) = &released {
                note_serial_frames(&mut app_state.diagnostics, tx_frames, rx_frames);
            }
            push_log(
                &mut app_state.diagnostics,
                "capture disabled and state released",
            );
        } else {
            push_log(&mut app_state.diagnostics, "capture enabled");
        }

        sync_runtime_metrics(app_state);
        app_state.clone()
    }))
}

#[tauri::command]
pub fn push_input_snapshot(
    snapshot: InputSnapshot,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let snapshot =
        validate_input_snapshot(snapshot.normalized()).map_err(|error| error.to_string())?;

    Ok(state.update(|app_state| {
        if app_state.automation.running {
            app_state.input.capture_enabled = false;
            app_state.input.release_all();
            sync_runtime_metrics(app_state);
            return app_state.clone();
        }

        app_state.input = LatestInputState::from_snapshot(snapshot, app_state.input.window_focused);
        app_state.controller =
            map_input_to_controller(&app_state.input, &app_state.profile.active_profile);
        note_input_update(
            &mut app_state.diagnostics,
            &app_state.input,
            &app_state.controller,
        );
        sync_runtime_metrics(app_state);
        app_state.clone()
    }))
}

#[tauri::command]
pub async fn start_automation(
    sequence: Vec<AutomationAction>,
    loop_count: u32,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    validate_loop_count(loop_count)?;

    let state = state.inner().clone();
    let snapshot = state.snapshot();
    if snapshot.serial.connection_state != ConnectionState::Connected {
        return Err("connect serial before starting automation".to_string());
    }
    if snapshot.automation.running {
        return Err("automation is already running".to_string());
    }

    let steps =
        build_automation_steps(&sequence, snapshot.profile.active_profile.controller_model)?;
    let released = release_controller_state_if_needed(&state).await?;
    let token = state.begin_automation_token();

    let start_snapshot = state.update(|app_state| {
        app_state.input.capture_enabled = false;
        app_state.input.release_all();
        app_state.controller = ControllerStatePayload::default();
        app_state.automation = AutomationState {
            running: true,
            loop_count,
            current_loop: 1,
            current_action_index: Some(0),
            last_error: None,
        };
        if let Some((tx_frames, rx_frames)) = &released {
            note_serial_frames(&mut app_state.diagnostics, tx_frames, rx_frames);
        }
        sync_runtime_metrics(app_state);
        push_log(
            &mut app_state.diagnostics,
            format!(
                "automation started: {} actions x {loop_count} loops",
                steps.len()
            ),
        );
        app_state.clone()
    });

    thread::spawn(move || run_automation(state, steps, loop_count, token));

    Ok(start_snapshot)
}

#[tauri::command]
pub async fn stop_automation(
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    state.cancel_automation_token();
    let released = release_controller_state_if_needed(&state).await?;

    Ok(state.update(|app_state| {
        app_state.automation.running = false;
        app_state.automation.current_loop = 0;
        app_state.automation.current_action_index = None;
        app_state.input.capture_enabled = false;
        app_state.input.release_all();
        app_state.controller = ControllerStatePayload::default();
        if let Some((tx_frames, rx_frames)) = &released {
            note_serial_frames(&mut app_state.diagnostics, tx_frames, rx_frames);
        }
        sync_runtime_metrics(app_state);
        push_log(&mut app_state.diagnostics, "automation stopped");
        app_state.clone()
    }))
}

#[tauri::command]
pub async fn list_serial_ports(
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let ports = match state
        .request_worker_async(WorkerCommand::RefreshPorts)
        .await?
    {
        WorkerReply::Ports(ports) => ports,
        _ => return Err("unexpected serial worker response for port listing".to_string()),
    };

    Ok(state.update(|app_state| {
        app_state.serial.available_ports = ports;
        if let Some(selected_port) = &app_state.serial.selected_port {
            let still_exists = app_state
                .serial
                .available_ports
                .iter()
                .any(|port| &port.port_name == selected_port);
            if !still_exists {
                app_state.serial.selected_port = None;
            }
        }

        push_log(
            &mut app_state.diagnostics,
            format!(
                "serial ports refreshed: {}",
                app_state.serial.available_ports.len()
            ),
        );
        sync_runtime_metrics(app_state);
        app_state.clone()
    }))
}

#[tauri::command]
pub fn select_serial_port(
    port: Option<String>,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    Ok(state.update(|app_state| {
        app_state.serial.selected_port = port.filter(|value| !value.is_empty());
        push_log(
            &mut app_state.diagnostics,
            match &app_state.serial.selected_port {
                Some(port) => format!("selected serial port: {port}"),
                None => "cleared selected serial port".to_string(),
            },
        );
        app_state.clone()
    }))
}

#[tauri::command]
pub async fn connect_serial(
    port: String,
    baud: u32,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    state.update(|app_state| {
        app_state.serial.connection_state = ConnectionState::Connecting;
        app_state.serial.last_connect_error = None;
    });

    let reply = match state
        .request_worker_control_async(WorkerCommand::Connect {
            port: port.clone(),
            baud,
        })
        .await
    {
        Ok(reply) => reply,
        Err(error) => {
            let message = error.clone();
            state.update(|app_state| {
                app_state.serial.connection_state = ConnectionState::Error;
                app_state.serial.last_connect_error = Some(message.clone());
                note_serial_error(&mut app_state.diagnostics, message.clone());
                app_state.clone()
            });
            return Err(error);
        }
    };

    match reply {
        WorkerReply::Connected {
            port,
            baud,
            status,
            tx_frames,
            rx_frames,
        } => Ok(state.update(|app_state| {
            app_state.serial.selected_port = Some(port.clone());
            app_state.serial.baud_rate = baud;
            app_state.serial.connection_state = ConnectionState::Connected;
            app_state.serial.last_connect_error = None;
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);

            if let Some(status) = status {
                apply_status_update(app_state, status);
            }

            sync_runtime_metrics(app_state);
            push_log(
                &mut app_state.diagnostics,
                format!("connected to {port} at {baud} baud"),
            );
            app_state.clone()
        })),
        _ => Err("unexpected serial worker response for connect".to_string()),
    }
}

#[tauri::command]
pub async fn disconnect_serial(
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let released = release_controller_state_if_needed(&state)
        .await
        .ok()
        .flatten();
    let _ = state
        .request_worker_control_async(WorkerCommand::Disconnect)
        .await?;

    Ok(state.update(|app_state| {
        app_state.serial.connection_state = ConnectionState::Disconnected;
        app_state.serial.last_connect_error = None;
        app_state.input.capture_enabled = false;
        app_state.input.window_focused = false;
        app_state.input.release_all();
        app_state.controller = Default::default();
        if let Some((tx_frames, rx_frames)) = &released {
            note_serial_frames(&mut app_state.diagnostics, tx_frames, rx_frames);
        }
        sync_runtime_metrics(app_state);
        push_log(&mut app_state.diagnostics, "serial session disconnected");
        app_state.clone()
    }))
}

#[tauri::command]
pub async fn get_status(state: State<'_, ManagedAppState>) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let reply = state
        .request_worker_control_async(WorkerCommand::GetStatus)
        .await?;

    match reply {
        WorkerReply::Status {
            status,
            tx_frames,
            rx_frames,
        } => Ok(state.update(|app_state| {
            apply_status_update(app_state, status);
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
            sync_runtime_metrics(app_state);
            push_log(&mut app_state.diagnostics, "status refreshed");
            app_state.clone()
        })),
        _ => Err("unexpected serial worker response for status".to_string()),
    }
}

#[tauri::command]
pub async fn get_events(state: State<'_, ManagedAppState>) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let reply = state
        .request_worker_control_async(WorkerCommand::GetEvents)
        .await?;

    match reply {
        WorkerReply::Events {
            events,
            tx_frames,
            rx_frames,
        } => Ok(state.update(|app_state| {
            note_events(&mut app_state.diagnostics, &events);
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
            sync_runtime_metrics(app_state);
            push_log(
                &mut app_state.diagnostics,
                format!("event dump received: {} entries", events.len()),
            );
            app_state.clone()
        })),
        _ => Err("unexpected serial worker response for events".to_string()),
    }
}

#[tauri::command]
pub async fn virtual_cable_unplug(
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let reply = state
        .request_worker_control_async(WorkerCommand::VirtualCableUnplug)
        .await?;
    apply_command_ack(&state, reply, "virtual cable unplug requested")
}

#[tauri::command]
pub async fn clear_bonds(state: State<'_, ManagedAppState>) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    let reply = state
        .request_worker_control_async(WorkerCommand::ClearBonds)
        .await?;
    apply_command_ack(&state, reply, "clear bonds requested")
}

#[tauri::command]
pub async fn set_controller_mode(
    mode: ControllerModel,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    if state.snapshot().automation.running {
        return Err("stop automation before changing controller mode".to_string());
    }

    let released = release_controller_state_if_needed(&state).await?;
    state.update(|app_state| {
        app_state.input.capture_enabled = false;
        app_state.input.release_all();
        app_state.controller = ControllerStatePayload::default();
        if let Some((release_tx, release_rx)) = &released {
            note_serial_frames(&mut app_state.diagnostics, release_tx, release_rx);
        }
        sync_runtime_metrics(app_state);
    });

    let protocol_mode = controller_model_to_protocol_mode(mode);
    let reply = state
        .request_worker_control_async(WorkerCommand::SetControllerMode(protocol_mode))
        .await?;
    apply_command_ack(
        &state,
        reply,
        format!("controller mode requested: {mode:?}"),
    )
}

#[tauri::command]
pub async fn set_bluetooth_enabled(
    enabled: bool,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    let state = state.inner().clone();
    if state.snapshot().automation.running {
        return Err("stop automation before changing Bluetooth state".to_string());
    }

    let released = if enabled {
        None
    } else {
        release_controller_state_if_needed(&state).await?
    };
    if !enabled {
        state.update(|app_state| {
            app_state.input.capture_enabled = false;
            app_state.input.release_all();
            app_state.controller = ControllerStatePayload::default();
            sync_runtime_metrics(app_state);
        });
    }

    let reply = state
        .request_worker_control_async(WorkerCommand::SetBluetoothEnabled(enabled))
        .await?;
    match reply {
        WorkerReply::CommandAck {
            status,
            tx_frames,
            rx_frames,
        } => Ok(state.update(|app_state| {
            if let Some((release_tx, release_rx)) = &released {
                note_serial_frames(&mut app_state.diagnostics, release_tx, release_rx);
            }
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
            if let Some(status) = status {
                apply_status_update(app_state, status);
            }
            if !enabled {
                app_state.input.capture_enabled = false;
                app_state.input.release_all();
                app_state.controller = ControllerStatePayload::default();
            }
            sync_runtime_metrics(app_state);
            push_log(
                &mut app_state.diagnostics,
                if enabled {
                    "bluetooth enabled"
                } else {
                    "bluetooth disabled"
                },
            );
            app_state.clone()
        })),
        _ => Err("unexpected serial worker response for bluetooth command".to_string()),
    }
}

#[tauri::command]
pub async fn tap_controller_button(
    button: String,
    duration_ms: Option<u64>,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    tap_controller_button_impl(button, duration_ms, state.inner().clone()).await
}

#[tauri::command]
pub async fn tap_left_joycon_button(
    button: String,
    duration_ms: Option<u64>,
    state: State<'_, ManagedAppState>,
) -> Result<AppStateSnapshot, String> {
    tap_controller_button_impl(button, duration_ms, state.inner().clone()).await
}

async fn tap_controller_button_impl(
    button: String,
    duration_ms: Option<u64>,
    state: ManagedAppState,
) -> Result<AppStateSnapshot, String> {
    let snapshot = state.snapshot();
    if snapshot.input.capture_enabled {
        return Err("direct tap is disabled while capture is active".to_string());
    }
    if snapshot.automation.running {
        return Err("direct tap is disabled while automation is running".to_string());
    }

    let controller_model = snapshot.profile.active_profile.controller_model;
    let buttons = controller_button_bits(&button, controller_model)?;
    let duration = Duration::from_millis(duration_ms.unwrap_or(120).clamp(25, 2000));

    state.update(|app_state| {
        app_state.input.release_all();
        app_state.controller = ControllerStatePayload::default();
        sync_runtime_metrics(app_state);
    });

    let press = ControllerStatePayload {
        buttons,
        ..ControllerStatePayload::default()
    };

    let press_reply = state
        .request_worker_control_async(WorkerCommand::SendState(press))
        .await?;
    tauri::async_runtime::spawn_blocking(move || thread::sleep(duration))
        .await
        .map_err(|error| format!("direct tap sleep failed: {error}"))?;
    let release_reply = state
        .request_worker_control_async(WorkerCommand::SendState(ControllerStatePayload::default()))
        .await?;

    let snapshot = state.update(|app_state| -> Result<AppStateSnapshot, String> {
        apply_state_sent_reply(app_state, press_reply)?;
        apply_state_sent_reply(app_state, release_reply)?;
        push_log(
            &mut app_state.diagnostics,
            format!("test tap sent: {button} for {}ms", duration.as_millis()),
        );
        sync_runtime_metrics(app_state);
        Ok(app_state.clone())
    })?;

    Ok(snapshot)
}

enum AutomationFinish {
    Completed,
    Cancelled,
    Failed(String),
}

fn run_automation(
    state: ManagedAppState,
    steps: Vec<AutomationStep>,
    loop_count: u32,
    token: Arc<AtomicBool>,
) {
    let mut held_buttons = 0u32;
    let mut finish = AutomationFinish::Completed;

    'automation: for loop_index in 0..loop_count {
        if automation_cancelled(&state, &token) {
            finish = AutomationFinish::Cancelled;
            break;
        }

        for (action_index, step) in steps.iter().enumerate() {
            if automation_cancelled(&state, &token) {
                finish = AutomationFinish::Cancelled;
                break 'automation;
            }

            update_automation_progress(&state, &token, loop_index + 1, Some(action_index));

            let result = match step.step_type {
                AutomationStepType::Hold => {
                    held_buttons |= step.buttons;
                    send_automation_payload(&state, state_with_buttons(held_buttons)).map(|()| true)
                }
                AutomationStepType::Release => {
                    held_buttons &= !step.buttons;
                    send_automation_payload(&state, state_with_buttons(held_buttons)).map(|()| true)
                }
                AutomationStepType::Delay => Ok(wait_for_automation_duration(
                    step.duration_ms,
                    &state,
                    &token,
                )),
                AutomationStepType::Tap => {
                    send_automation_payload(&state, state_with_buttons(held_buttons | step.buttons))
                        .and_then(|()| {
                            if wait_for_automation_duration(step.duration_ms, &state, &token) {
                                send_automation_payload(&state, state_with_buttons(held_buttons))
                                    .map(|()| true)
                            } else {
                                Ok(false)
                            }
                        })
                }
            };

            match result {
                Ok(true) => {}
                Ok(false) => {
                    finish = AutomationFinish::Cancelled;
                    break 'automation;
                }
                Err(error) => {
                    finish = AutomationFinish::Failed(error);
                    break 'automation;
                }
            }
        }

        if automation_cancelled(&state, &token) {
            finish = AutomationFinish::Cancelled;
            break;
        }

        if held_buttons != 0 {
            held_buttons = 0;
            if let Err(error) = send_automation_payload(&state, ControllerStatePayload::default()) {
                finish = AutomationFinish::Failed(error);
                break;
            }
        }
    }

    finish_automation(state, &token, finish);
}

fn automation_cancelled(state: &ManagedAppState, token: &Arc<AtomicBool>) -> bool {
    token.load(Ordering::Acquire) || !state.automation_token_is_current(token)
}

fn wait_for_automation_duration(
    duration_ms: u64,
    state: &ManagedAppState,
    token: &Arc<AtomicBool>,
) -> bool {
    let mut remaining_ms = duration_ms;
    while remaining_ms > 0 {
        if automation_cancelled(state, token) {
            return false;
        }

        let chunk_ms = remaining_ms.min(10);
        thread::sleep(Duration::from_millis(chunk_ms));
        remaining_ms -= chunk_ms;
    }

    !automation_cancelled(state, token)
}

fn update_automation_progress(
    state: &ManagedAppState,
    token: &Arc<AtomicBool>,
    current_loop: u32,
    current_action_index: Option<usize>,
) {
    if !state.automation_token_is_current(token) {
        return;
    }

    state.update(|app_state| {
        if app_state.automation.running {
            app_state.automation.current_loop = current_loop;
            app_state.automation.current_action_index = current_action_index;
        }
    });
}

fn send_automation_payload(
    state: &ManagedAppState,
    payload: ControllerStatePayload,
) -> Result<(), String> {
    let reply = state.request_worker_control(WorkerCommand::SendState(payload));
    match reply {
        Ok(WorkerReply::StateSent {
            tx_frames,
            rx_frames,
        }) => {
            state.update(|app_state| {
                note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
                app_state.serial.last_connect_error = None;
                app_state.controller = payload;
                sync_runtime_metrics(app_state);
            });
            Ok(())
        }
        Ok(_) => record_automation_serial_error(
            state,
            "unexpected serial worker response for automation state frame".to_string(),
        ),
        Err(error) => record_automation_serial_error(state, error),
    }
}

fn record_automation_serial_error(state: &ManagedAppState, error: String) -> Result<(), String> {
    state.update(|app_state| {
        app_state.serial.last_connect_error = Some(error.clone());
        note_serial_error(&mut app_state.diagnostics, error.clone());
        sync_runtime_metrics(app_state);
    });
    Err(error)
}

fn finish_automation(state: ManagedAppState, token: &Arc<AtomicBool>, finish: AutomationFinish) {
    if !state.automation_token_is_current(token) {
        return;
    }

    if matches!(finish, AutomationFinish::Cancelled) {
        state.clear_automation_token(token);
        return;
    }

    let release_error = release_automation_state_if_needed(&state);

    state.update(|app_state| {
        app_state.automation.running = false;
        app_state.automation.current_loop = 0;
        app_state.automation.current_action_index = None;
        app_state.input.capture_enabled = false;
        app_state.input.release_all();
        app_state.controller = ControllerStatePayload::default();

        match finish {
            AutomationFinish::Completed => {
                app_state.automation.last_error = release_error.clone();
                if let Some(error) = &release_error {
                    push_log(
                        &mut app_state.diagnostics,
                        format!("automation completed, but release failed: {error}"),
                    );
                } else {
                    push_log(&mut app_state.diagnostics, "automation completed");
                }
            }
            AutomationFinish::Failed(error) => {
                let final_error = release_error
                    .as_ref()
                    .map(|release| format!("{error}; release failed: {release}"))
                    .unwrap_or(error);
                app_state.automation.last_error = Some(final_error.clone());
                push_log(
                    &mut app_state.diagnostics,
                    format!("automation failed: {final_error}"),
                );
            }
            AutomationFinish::Cancelled => {}
        }

        sync_runtime_metrics(app_state);
    });

    state.clear_automation_token(token);
}

fn release_automation_state_if_needed(state: &ManagedAppState) -> Option<String> {
    if state.snapshot().controller == ControllerStatePayload::default() {
        return None;
    }

    send_automation_payload(state, ControllerStatePayload::default()).err()
}

fn validate_input_snapshot(snapshot: InputSnapshot) -> Result<InputSnapshot, AppError> {
    if snapshot.pressed_codes.len() > 64 {
        return Err(AppError::TooManyPressedKeys(snapshot.pressed_codes.len()));
    }

    if let Some(button) = snapshot
        .mouse_buttons
        .iter()
        .copied()
        .find(|button| *button > 7)
    {
        return Err(AppError::UnsupportedMouseButton(button));
    }

    Ok(snapshot)
}

fn controller_model_to_protocol_mode(model: ControllerModel) -> u8 {
    match model {
        ControllerModel::LeftJoyCon => CONTROLLER_MODE_LEFT_JOYCON,
        ControllerModel::RightJoyCon => CONTROLLER_MODE_RIGHT_JOYCON,
        ControllerModel::ProController => CONTROLLER_MODE_PRO_CONTROLLER,
    }
}

fn protocol_mode_to_controller_model(mode: u8) -> Option<ControllerModel> {
    match mode {
        CONTROLLER_MODE_LEFT_JOYCON => Some(ControllerModel::LeftJoyCon),
        CONTROLLER_MODE_RIGHT_JOYCON => Some(ControllerModel::RightJoyCon),
        CONTROLLER_MODE_PRO_CONTROLLER => Some(ControllerModel::ProController),
        _ => None,
    }
}

fn apply_status_update(app_state: &mut AppStateSnapshot, status: crate::model::StatusPayloadUi) {
    if let Some(model) = protocol_mode_to_controller_model(status.controller_mode) {
        if app_state.profile.active_profile.controller_model != model {
            let profile = default_profile_for_model(model);
            app_state.profile.active_profile_id = profile.id.clone();
            app_state.profile.active_profile = profile;
            app_state.input.release_all();
            app_state.controller = ControllerStatePayload::default();
        }
    }

    app_state.serial.last_status = Some(status);
    note_status(&mut app_state.diagnostics, &status);
}

fn apply_state_sent_reply(
    app_state: &mut AppStateSnapshot,
    reply: WorkerReply,
) -> Result<(), String> {
    match reply {
        WorkerReply::StateSent {
            tx_frames,
            rx_frames,
        } => {
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
            Ok(())
        }
        _ => Err("unexpected serial worker response for state frame".to_string()),
    }
}

fn apply_command_ack(
    state: &ManagedAppState,
    reply: WorkerReply,
    message: impl Into<String>,
) -> Result<AppStateSnapshot, String> {
    let message = message.into();
    match reply {
        WorkerReply::CommandAck {
            status,
            tx_frames,
            rx_frames,
        } => Ok(state.update(|app_state| {
            note_serial_frames(&mut app_state.diagnostics, &tx_frames, &rx_frames);
            if let Some(status) = status {
                apply_status_update(app_state, status);
            }
            sync_runtime_metrics(app_state);
            push_log(&mut app_state.diagnostics, message);
            app_state.clone()
        })),
        _ => Err("unexpected serial worker response for control command".to_string()),
    }
}

async fn release_controller_state_if_needed(
    state: &ManagedAppState,
) -> Result<
    Option<(
        Vec<crate::model::FrameMetaUi>,
        Vec<crate::model::FrameMetaUi>,
    )>,
    String,
> {
    let snapshot = state.snapshot();
    if snapshot.serial.connection_state != ConnectionState::Connected
        || snapshot.controller == ControllerStatePayload::default()
    {
        return Ok(None);
    }

    match state
        .request_worker_control_async(WorkerCommand::SendState(ControllerStatePayload::default()))
        .await?
    {
        WorkerReply::StateSent {
            tx_frames,
            rx_frames,
        } => Ok(Some((tx_frames, rx_frames))),
        _ => Err("unexpected serial worker response for release state".to_string()),
    }
}
