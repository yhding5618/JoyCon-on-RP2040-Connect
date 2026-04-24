use std::sync::{Arc, RwLock};
use std::thread;
use std::time::Duration;

use crate::bridge_protocol::ControllerStatePayload;
use crate::diagnostics::{note_serial_error, note_serial_frames, push_log};
use crate::model::{
    ActiveProfileState, AppStateSnapshot, ConnectionState, DiagnosticsState, LatestInputState,
    SerialSessionState,
};
use crate::profiles::default_right_joycon_profile;
use crate::serial_worker::{SerialWorkerHandle, WorkerCommand, WorkerReply};

const DEFAULT_OUTPUT_RATE_HZ: u16 = 125;
const MAX_OUTPUT_RATE_HZ: u16 = 250;
const OUTPUT_ERROR_LIMIT: u8 = 3;
const OUTPUT_ERROR_BACKOFF_MS: u64 = 100;

pub struct ManagedAppState {
    inner: Arc<RwLock<AppStateSnapshot>>,
    worker: SerialWorkerHandle,
}

impl ManagedAppState {
    pub fn new() -> Self {
        let inner = Arc::new(RwLock::new(initial_state()));
        let worker = SerialWorkerHandle::spawn();
        spawn_output_loop(inner.clone(), worker.clone());

        Self { inner, worker }
    }

    pub fn snapshot(&self) -> AppStateSnapshot {
        self.inner.read().expect("app state lock poisoned").clone()
    }

    pub fn update<T>(&self, update: impl FnOnce(&mut AppStateSnapshot) -> T) -> T {
        let mut state = self.inner.write().expect("app state lock poisoned");
        update(&mut state)
    }

    pub fn request_worker(&self, command: WorkerCommand) -> Result<WorkerReply, String> {
        self.worker.request(command)
    }
}

fn spawn_output_loop(inner: Arc<RwLock<AppStateSnapshot>>, worker: SerialWorkerHandle) {
    thread::spawn(move || {
        let mut was_streaming = false;
        let mut consecutive_output_errors = 0u8;

        loop {
            let (connected, capture_enabled, controller, output_rate_hz) = {
                let snapshot = inner.read().expect("app state lock poisoned");
                (
                    snapshot.serial.connection_state == ConnectionState::Connected,
                    snapshot.input.capture_enabled,
                    snapshot.controller,
                    snapshot.profile.active_profile.output_rate_hz,
                )
            };

            let send_neutral = connected && was_streaming && !capture_enabled;
            let send_live_state = connected && capture_enabled;

            if send_live_state || send_neutral {
                let payload = if send_live_state {
                    controller
                } else {
                    ControllerStatePayload::default()
                };

                match worker.request(WorkerCommand::SendState(payload)) {
                    Ok(WorkerReply::StateSent {
                        tx_frames,
                        rx_frames,
                    }) => {
                        let mut state = inner.write().expect("app state lock poisoned");
                        note_serial_frames(&mut state.diagnostics, &tx_frames, &rx_frames);
                        state.serial.last_connect_error = None;
                        consecutive_output_errors = 0;
                    }
                    Ok(_) => {}
                    Err(error) => {
                        consecutive_output_errors = consecutive_output_errors.saturating_add(1);
                        let should_halt =
                            send_neutral || consecutive_output_errors >= OUTPUT_ERROR_LIMIT;

                        let mut state = inner.write().expect("app state lock poisoned");
                        state.serial.last_connect_error = Some(error.clone());
                        note_serial_error(&mut state.diagnostics, error.clone());

                        if should_halt {
                            state.serial.connection_state = ConnectionState::Error;
                            state.input.capture_enabled = false;
                            state.input.release_all();
                            state.controller = ControllerStatePayload::default();
                            sync_runtime_metrics(&mut state);
                            push_log(
                                &mut state.diagnostics,
                                format!(
                                    "output loop halted after {consecutive_output_errors} serial write failures"
                                ),
                            );
                        } else {
                            push_log(
                                &mut state.diagnostics,
                                format!(
                                    "output frame failed ({consecutive_output_errors}/{OUTPUT_ERROR_LIMIT}); backing off"
                                ),
                            );
                        }
                    }
                }
            } else {
                consecutive_output_errors = 0;
            }

            was_streaming = send_live_state;

            if consecutive_output_errors > 0 {
                thread::sleep(Duration::from_millis(OUTPUT_ERROR_BACKOFF_MS));
            }

            let rate_hz = effective_output_rate_hz(output_rate_hz);
            let sleep_ms = u64::max(1, 1000 / u64::from(rate_hz));
            thread::sleep(Duration::from_millis(sleep_ms));
        }
    });
}

pub fn sync_runtime_metrics(app_state: &mut AppStateSnapshot) {
    app_state.diagnostics.input_rate_hz = if app_state.input.capture_enabled {
        60.0
    } else {
        0.0
    };

    app_state.diagnostics.output_rate_hz = if app_state.input.capture_enabled
        && app_state.serial.connection_state == ConnectionState::Connected
    {
        f32::from(effective_output_rate_hz(
            app_state.profile.active_profile.output_rate_hz,
        ))
    } else {
        0.0
    };
}

fn effective_output_rate_hz(output_rate_hz: u16) -> u16 {
    if output_rate_hz == 0 {
        DEFAULT_OUTPUT_RATE_HZ
    } else {
        output_rate_hz.min(MAX_OUTPUT_RATE_HZ)
    }
}

fn initial_state() -> AppStateSnapshot {
    let profile = default_right_joycon_profile();

    AppStateSnapshot {
        serial: SerialSessionState {
            available_ports: Vec::new(),
            selected_port: None,
            baud_rate: 115_200,
            connection_state: ConnectionState::Disconnected,
            last_connect_error: None,
            last_status: None,
        },
        profile: ActiveProfileState {
            active_profile_id: profile.id.clone(),
            active_profile: profile,
        },
        input: LatestInputState {
            pressed_codes: Vec::new(),
            mouse_buttons: Vec::new(),
            mouse_delta_x: 0.0,
            mouse_delta_y: 0.0,
            pointer_locked: false,
            capture_enabled: false,
            timestamp_ms: 0,
            window_focused: true,
        },
        controller: ControllerStatePayload::default(),
        diagnostics: DiagnosticsState {
            tx_count: 0,
            rx_count: 0,
            input_rate_hz: 0.0,
            output_rate_hz: 0.0,
            recent_logs: vec!["App scaffold initialized".to_string()],
            last_serial_error: None,
            last_status: None,
            last_frame_tx: None,
            last_frame_rx: None,
            last_events: Vec::new(),
        },
    }
}
