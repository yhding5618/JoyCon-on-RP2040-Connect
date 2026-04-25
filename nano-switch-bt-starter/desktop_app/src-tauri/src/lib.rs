pub mod app_state;
pub mod bridge_protocol;
pub mod commands;
pub mod controller_mapper;
pub mod diagnostics;
pub mod errors;
pub mod model;
pub mod profiles;
pub mod serial_worker;

#[cfg(test)]
mod tests;

use app_state::ManagedAppState;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_log::Builder::default().build())
        .manage(ManagedAppState::new())
        .invoke_handler(tauri::generate_handler![
            commands::get_app_state_snapshot,
            commands::clear_command_log,
            commands::push_input_snapshot,
            commands::set_capture_enabled,
            commands::list_serial_ports,
            commands::select_serial_port,
            commands::connect_serial,
            commands::disconnect_serial,
            commands::get_status,
            commands::get_events,
            commands::virtual_cable_unplug,
            commands::clear_bonds,
            commands::set_controller_mode,
            commands::set_bluetooth_enabled,
            commands::tap_controller_button,
            commands::tap_left_joycon_button
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
