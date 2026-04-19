#include "switch_hid.h"

#include <string.h>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"
#include "esp_timer.h"

static const char *kLocalName = "Nano Switch Starter";
static const uint8_t kPeripheralMinorClassGeneric = 0x00;

static const uint8_t kStarterGamepadDescriptor[] = {
    0x05, 0x01,        0x09, 0x05,        0xA1, 0x01,        0x85, 0x01,
    0x05, 0x09,        0x19, 0x01,        0x29, 0x10,        0x15, 0x00,
    0x25, 0x01,        0x95, 0x10,        0x75, 0x01,        0x81, 0x02,
    0x05, 0x01,        0x09, 0x39,        0x15, 0x00,        0x25, 0x07,
    0x35, 0x00,        0x46, 0x3B, 0x01,  0x65, 0x14,        0x75, 0x04,
    0x95, 0x01,        0x81, 0x42,        0x65, 0x00,        0x75, 0x04,
    0x95, 0x01,        0x81, 0x03,        0x09, 0x30,        0x09, 0x31,
    0x09, 0x32,        0x09, 0x35,        0x15, 0x00,        0x26, 0xFF, 0x00,
    0x75, 0x08,        0x95, 0x04,        0x81, 0x02,        0xC0,
};

static const esp_hidd_app_param_t kStarterApp = {
    .name = "Nano Switch Starter",
    .description = "Starter bridge for Switch controller emulation",
    .provider = "Codex",
    .subclass = ESP_HID_CLASS_GPD,
    .desc_list = (uint8_t *)kStarterGamepadDescriptor,
    .desc_list_len = sizeof(kStarterGamepadDescriptor),
};

static const esp_hidd_qos_param_t kQos = {
    .service_type = 0,
    .token_rate = 0,
    .token_bucket_size = 0,
    .peak_bandwidth = 0,
    .access_latency = 0,
    .delay_variation = 0,
};

static sb_controller_state_t s_state;
static sb_status_payload_t s_status = {
    .flags = SB_STATUS_FLAG_BRIDGE_READY,
    .protocol_mode = 0,
    .input_report_mode = 0x30,
    .battery_level = 8,
    .last_host_report_id = 0,
    .last_error = 0,
    .reserved = 0,
};
static uint64_t s_last_report_us = 0;

static uint8_t scale_axis(int16_t value) {
  const int32_t shifted = (int32_t)value + 32768;
  if (shifted <= 0) {
    return 0;
  }
  if (shifted >= 65535) {
    return 255;
  }
  return (uint8_t)(shifted >> 8);
}

static void build_input_report(uint8_t report[7]) {
  const uint16_t buttons = (uint16_t)(s_state.buttons & 0xFFFFu);

  report[0] = (uint8_t)(buttons & 0xFFu);
  report[1] = (uint8_t)((buttons >> 8) & 0xFFu);
  report[2] = (uint8_t)(s_state.hat > 8u ? 8u : s_state.hat);
  report[3] = scale_axis(s_state.lx);
  report[4] = scale_axis(s_state.ly);
  report[5] = scale_axis(s_state.rx);
  report[6] = scale_axis(s_state.ry);
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  (void)event;
  (void)param;
}

static void hid_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
  switch (event) {
    case ESP_HIDD_INIT_EVT:
      if (param->init.status == ESP_HIDD_SUCCESS) {
        esp_bt_hid_device_register_app((esp_hidd_app_param_t *)&kStarterApp,
                                       (esp_hidd_qos_param_t *)&kQos,
                                       (esp_hidd_qos_param_t *)&kQos);
      } else {
        s_status.last_error = (uint8_t)param->init.status;
      }
      break;

    case ESP_HIDD_REGISTER_APP_EVT: {
      if (param->register_app.status == ESP_HIDD_SUCCESS) {
        esp_bt_cod_t cod = {
            .major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL,
            .minor = kPeripheralMinorClassGeneric,
            .service = ESP_BT_COD_SRVC_RENDERING,
            .reserved_2 = 0,
            .reserved_8 = 0,
        };

        s_status.flags |= SB_STATUS_FLAG_HID_READY | SB_STATUS_FLAG_BT_READY;
        if (param->register_app.in_use) {
          s_status.flags |= SB_STATUS_FLAG_VIRTUAL_CABLE;
        }

        esp_bt_dev_set_device_name(kLocalName);
        esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
      } else {
        s_status.last_error = (uint8_t)param->register_app.status;
      }
      break;
    }

    case ESP_HIDD_OPEN_EVT:
      if (param->open.status == ESP_HIDD_SUCCESS &&
          param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED) {
        s_status.flags |= SB_STATUS_FLAG_CONNECTED | SB_STATUS_FLAG_VIRTUAL_CABLE;
      }
      break;

    case ESP_HIDD_CLOSE_EVT:
      s_status.flags &= (uint8_t)~SB_STATUS_FLAG_CONNECTED;
      break;

    case ESP_HIDD_SET_PROTOCOL_EVT:
      s_status.protocol_mode = (uint8_t)param->set_protocol.protocol_mode;
      break;

    case ESP_HIDD_SET_REPORT_EVT:
      s_status.last_host_report_id = param->set_report.report_id;
      break;

    case ESP_HIDD_INTR_DATA_EVT:
      s_status.last_host_report_id = param->intr_data.report_id;
      break;

    case ESP_HIDD_SEND_REPORT_EVT:
      if (param->send_report.status != ESP_HIDD_SUCCESS) {
        s_status.last_error = param->send_report.reason;
      }
      break;

    case ESP_HIDD_VC_UNPLUG_EVT:
      s_status.flags &= (uint8_t)~SB_STATUS_FLAG_VIRTUAL_CABLE;
      break;

    default:
      break;
  }
}

esp_err_t switch_hid_init(void) {
  esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  err = esp_bt_controller_init(&bt_cfg);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK) {
    return err;
  }

  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    return err;
  }

  err = esp_bt_gap_register_callback(gap_callback);
  if (err != ESP_OK) {
    return err;
  }

  err = esp_bt_hid_device_register_callback(hid_callback);
  if (err != ESP_OK) {
    return err;
  }

  return esp_bt_hid_device_init();
}

void switch_hid_apply_state(const sb_controller_state_t *state) {
  if (state == NULL) {
    return;
  }

  s_state = *state;
  if (state->battery_level <= 8u) {
    s_status.battery_level = state->battery_level;
  }
}

void switch_hid_get_status(sb_status_payload_t *status) {
  if (status == NULL) {
    return;
  }

  *status = s_status;
}

void switch_hid_tick(void) {
  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) == 0u ||
      (s_status.flags & SB_STATUS_FLAG_HID_READY) == 0u) {
    return;
  }

  const uint64_t now = esp_timer_get_time();
  if ((now - s_last_report_us) < 16666u) {
    return;
  }

  s_last_report_us = now;

  uint8_t report[7];
  build_input_report(report);
  esp_bt_hid_device_send_report(ESP_HIDD_REPORT_TYPE_INTRDATA, 0x01, sizeof(report), report);
}

esp_err_t switch_hid_virtual_cable_unplug(void) {
  return esp_bt_hid_device_virtual_cable_unplug();
}
