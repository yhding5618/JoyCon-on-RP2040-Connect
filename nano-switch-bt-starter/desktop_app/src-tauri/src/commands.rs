use std::thread;
use std::time::Duration;

use tauri::State;

use crate::app_state::sync_runtime_metrics;
use crate::app_state::ManagedAppState;
use crate::bridge_protocol::{
    ControllerStatePayload, BTN_LJC_CAPTURE, BTN_LJC_DOWN, BTN_LJC_L, BTN_LJC_LEFT, BTN_LJC_MINUS,
    BTN_LJC_RIGHT, BTN_LJC_SL, BTN_LJC_SR, BTN_LJC_STICK, BTN_LJC_UP, BTN_LJC_ZL, BTN_RJC_A,
    BTN_RJC_B, BTN_RJC_HOME, BTN_RJC_PLUS, BTN_RJC_R, BTN_RJC_SL, BTN_RJC_SR, BTN_RJC_STICK,
    BTN_RJC_X, BTN_RJC_Y, BTN_RJC_ZR, CONTROLLER_MODE_LEFT_JOYCON, CONTROLLER_MODE_PRO_CONTROLLER,
    CONTROLLER_MODE_RIGHT_JOYCON,
};
use crate::controller_mapper::map_input_to_controller;
use crate::diagnostics::{
    note_events, note_input_update, note_serial_error, note_serial_frames, note_status, push_log,
};
use crate::errors::AppError;
use crate::model::{
    AppStateSnapshot, ConnectionState, ControllerModel, InputSnapshot, LatestInputState,
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

    let controller_model = state.snapshot().profile.active_profile.controller_model;
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

fn controller_button_bits(button: &str, controller_model: ControllerModel) -> Result<u32, String> {
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
