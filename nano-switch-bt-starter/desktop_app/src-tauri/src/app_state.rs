use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex, RwLock,
};
use std::thread;
use std::time::Duration;

use crate::bridge_protocol::ControllerStatePayload;
use crate::diagnostics::{note_serial_error, note_serial_frames, push_log};
use crate::model::{
    ActiveProfileState, AppStateSnapshot, AutomationState, ConnectionState, DiagnosticsState,
    LatestInputState, SerialSessionState,
};
use crate::profiles::default_pro_controller_profile;
use crate::serial_worker::{SerialWorkerHandle, WorkerCommand, WorkerReply};

const DEFAULT_OUTPUT_RATE_HZ: u16 = 125;
const MAX_OUTPUT_RATE_HZ: u16 = 250;
const OUTPUT_ERROR_LIMIT: u8 = 3;
const OUTPUT_ERROR_BACKOFF_MS: u64 = 100;

pub struct ManagedAppState {
    inner: Arc<RwLock<AppStateSnapshot>>,
    worker: SerialWorkerHandle,
    control_in_flight: Arc<AtomicBool>,
    automation_cancel: Arc<Mutex<Option<Arc<AtomicBool>>>>,
}

impl ManagedAppState {
    pub fn new() -> Self {
        let inner = Arc::new(RwLock::new(initial_state()));
        let worker = SerialWorkerHandle::spawn();
        let control_in_flight = Arc::new(AtomicBool::new(false));
        let automation_cancel = Arc::new(Mutex::new(None));
        spawn_output_loop(inner.clone(), worker.clone(), control_in_flight.clone());

        Self {
            inner,
            worker,
            control_in_flight,
            automation_cancel,
        }
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

    pub fn request_worker_control(&self, command: WorkerCommand) -> Result<WorkerReply, String> {
        self.control_in_flight.store(true, Ordering::Release);
        let result = self.worker.request(command);
        self.control_in_flight.store(false, Ordering::Release);
        result
    }

    pub async fn request_worker_async(
        &self,
        command: WorkerCommand,
    ) -> Result<WorkerReply, String> {
        let worker = self.worker.clone();
        tauri::async_runtime::spawn_blocking(move || worker.request(command))
            .await
            .map_err(|error| format!("serial worker task failed: {error}"))?
    }

    pub async fn request_worker_control_async(
        &self,
        command: WorkerCommand,
    ) -> Result<WorkerReply, String> {
        let worker = self.worker.clone();
        let control_in_flight = self.control_in_flight.clone();
        control_in_flight.store(true, Ordering::Release);

        let result = tauri::async_runtime::spawn_blocking(move || worker.request(command)).await;
        self.control_in_flight.store(false, Ordering::Release);
        result.map_err(|error| format!("serial worker task failed: {error}"))?
    }

    pub fn begin_automation_token(&self) -> Arc<AtomicBool> {
        let token = Arc::new(AtomicBool::new(false));
        let mut current = self
            .automation_cancel
            .lock()
            .expect("automation token lock poisoned");
        *current = Some(token.clone());
        token
    }

    pub fn cancel_automation_token(&self) {
        if let Some(token) = self
            .automation_cancel
            .lock()
            .expect("automation token lock poisoned")
            .as_ref()
        {
            token.store(true, Ordering::Release);
        }
    }

    pub fn automation_token_is_current(&self, token: &Arc<AtomicBool>) -> bool {
        self.automation_cancel
            .lock()
            .expect("automation token lock poisoned")
            .as_ref()
            .is_some_and(|current| Arc::ptr_eq(current, token))
    }

    pub fn clear_automation_token(&self, token: &Arc<AtomicBool>) {
        let mut current = self
            .automation_cancel
            .lock()
            .expect("automation token lock poisoned");
        if current
            .as_ref()
            .is_some_and(|current_token| Arc::ptr_eq(current_token, token))
        {
            *current = None;
        }
    }
}

impl Clone for ManagedAppState {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
            worker: self.worker.clone(),
            control_in_flight: self.control_in_flight.clone(),
            automation_cancel: self.automation_cancel.clone(),
        }
    }
}

fn spawn_output_loop(
    inner: Arc<RwLock<AppStateSnapshot>>,
    worker: SerialWorkerHandle,
    control_in_flight: Arc<AtomicBool>,
) {
    thread::spawn(move || {
        let mut last_sent_state: Option<ControllerStatePayload> = None;
        let mut consecutive_output_errors = 0u8;

        loop {
            let control_busy = control_in_flight.load(Ordering::Acquire);
            let (connected, capture_enabled, controller, output_rate_hz) = {
                let snapshot = inner.read().expect("app state lock poisoned");
                (
                    snapshot.serial.connection_state == ConnectionState::Connected,
                    snapshot.input.capture_enabled,
                    snapshot.controller,
                    snapshot.profile.active_profile.output_rate_hz,
                )
            };

            if !connected || !capture_enabled {
                last_sent_state = None;
                consecutive_output_errors = 0;
            } else {
                let should_send_live_state = !control_busy
                    && Some(controller) != last_sent_state
                    && (controller != ControllerStatePayload::default()
                        || last_sent_state.is_some());

                if should_send_live_state {
                    match worker.request(WorkerCommand::SendState(controller)) {
                        Ok(WorkerReply::StateSent {
                            tx_frames,
                            rx_frames,
                        }) => {
                            let mut state = inner.write().expect("app state lock poisoned");
                            note_serial_frames(&mut state.diagnostics, &tx_frames, &rx_frames);
                            state.serial.last_connect_error = None;
                            last_sent_state = Some(controller);
                            consecutive_output_errors = 0;
                        }
                        Ok(_) => {}
                        Err(error) => {
                            consecutive_output_errors = consecutive_output_errors.saturating_add(1);
                            let should_halt = consecutive_output_errors >= OUTPUT_ERROR_LIMIT;

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
            }

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
    let profile = default_pro_controller_profile();

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
        automation: AutomationState::default(),
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
            command_log: Vec::new(),
            recent_logs: vec!["App scaffold initialized".to_string()],
            last_serial_error: None,
            last_status: None,
            last_frame_tx: None,
            last_frame_rx: None,
            last_events: Vec::new(),
        },
    }
}
