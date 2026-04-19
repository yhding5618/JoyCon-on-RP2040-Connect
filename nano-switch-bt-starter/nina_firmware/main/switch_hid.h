#ifndef SWITCH_HID_H
#define SWITCH_HID_H

#include "esp_err.h"

#include "switch_bridge_protocol.h"

esp_err_t switch_hid_init(void);
void switch_hid_apply_state(const sb_controller_state_t *state);
void switch_hid_get_status(sb_status_payload_t *status);
void switch_hid_tick(void);
esp_err_t switch_hid_virtual_cable_unplug(void);
esp_err_t switch_hid_clear_all_bonds(void);

#endif
