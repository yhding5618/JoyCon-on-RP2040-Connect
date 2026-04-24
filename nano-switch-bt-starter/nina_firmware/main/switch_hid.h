#ifndef SWITCH_HID_H
#define SWITCH_HID_H

#include "esp_err.h"

#include "switch_bridge_protocol.h"

esp_err_t switch_hid_init(void);
esp_err_t switch_hid_set_controller_mode(sb_controller_mode_t mode);
esp_err_t switch_hid_set_bluetooth_enabled(bool enabled);
void switch_hid_apply_state(const sb_controller_state_t *state);
void switch_hid_get_status(sb_status_payload_t *status);
size_t switch_hid_copy_event_log(sb_event_entry_t *entries,
                                 size_t max_entries,
                                 uint16_t *first_sequence,
                                 bool *overflowed);
void switch_hid_tick(void);
esp_err_t switch_hid_virtual_cable_unplug(void);
esp_err_t switch_hid_clear_all_bonds(void);

#endif
