#include "switch_hid.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SWITCH_ACK_SIMPLE 0x80u
#define SWITCH_ACK_DEVICE_INFO 0x82u
#define SWITCH_ACK_TRIGGER_BUTTONS 0x83u
#define SWITCH_ACK_SPI_FLASH_READ 0x90u
#define SWITCH_ACK_IMU_REG_READ 0xC0u
#define SWITCH_ACK_PLAYER_LIGHTS 0xB0u
#define SWITCH_ACK_REGULATED_VOLTAGE 0xD0u

#define SWITCH_CONNECTION_INFO_JOYCON_WIRELESS 0x0Eu

#define SWITCH_OUTPUT_SUBCOMMAND 0x01u
#define SWITCH_OUTPUT_RUMBLE_ONLY 0x10u
#define SWITCH_OUTPUT_NFC_IR 0x11u
#define SWITCH_OUTPUT_UNKNOWN 0x12u

#define SWITCH_REPORT_SUBCOMMAND_REPLY 0x21u
#define SWITCH_REPORT_STANDARD_FULL 0x30u
#define SWITCH_REPORT_STANDARD_NFC_IR 0x31u
#define SWITCH_REPORT_STANDARD_ALT0 0x32u
#define SWITCH_REPORT_STANDARD_ALT1 0x33u
#define SWITCH_REPORT_SIMPLE_HID 0x3Fu

#define SWITCH_SUBCMD_GET_ONLY_CONTROLLER_STATE 0x00u
#define SWITCH_SUBCMD_GET_DEVICE_INFO 0x02u
#define SWITCH_SUBCMD_SET_INPUT_REPORT_MODE 0x03u
#define SWITCH_SUBCMD_TRIGGER_BUTTONS_ELAPSED 0x04u
#define SWITCH_SUBCMD_SET_SHIPMENT_LOW_POWER 0x08u
#define SWITCH_SUBCMD_SPI_FLASH_READ 0x10u
#define SWITCH_SUBCMD_SET_PLAYER_LIGHTS 0x30u
#define SWITCH_SUBCMD_GET_PLAYER_LIGHTS 0x31u
#define SWITCH_SUBCMD_ENABLE_IMU 0x40u
#define SWITCH_SUBCMD_READ_IMU_REG 0x43u
#define SWITCH_SUBCMD_ENABLE_VIBRATION 0x48u
#define SWITCH_SUBCMD_GET_REGULATED_VOLTAGE 0x50u

#define SWITCH_CONTROLLER_TYPE_LEFT_JOYCON 0x01u

#define SWITCH_INPUT_COMMON_BYTES 12u
#define SWITCH_SUBCOMMAND_REPLY_BYTES 48u
#define SWITCH_STANDARD_REPORT_BYTES 48u
#define SWITCH_SIMPLE_REPORT_BYTES 11u
#define SWITCH_SPI_READ_MAX_BYTES 0x1Du
#define SWITCH_MAX_BOND_DEVICES 16

#define SWITCH_HID_STATUS_SUCCESS 0x00u
#define SWITCH_HID_STATUS_ERROR 0x01u

typedef enum {
  SWITCH_HID_EVENT_INIT = 0x00u,
  SWITCH_HID_EVENT_DEINIT = 0x01u,
  SWITCH_HID_EVENT_REGISTER_APP = 0x02u,
  SWITCH_HID_EVENT_UNREGISTER_APP = 0x03u,
  SWITCH_HID_EVENT_OPEN = 0x04u,
  SWITCH_HID_EVENT_CLOSE = 0x05u,
  SWITCH_HID_EVENT_SEND_REPORT = 0x06u,
  SWITCH_HID_EVENT_REPORT_ERR = 0x07u,
  SWITCH_HID_EVENT_GET_REPORT = 0x08u,
  SWITCH_HID_EVENT_SET_REPORT = 0x09u,
  SWITCH_HID_EVENT_SET_PROTOCOL = 0x0Au,
  SWITCH_HID_EVENT_INTR_DATA = 0x0Bu,
  SWITCH_HID_EVENT_VC_UNPLUG = 0x0Cu,
  SWITCH_HID_EVENT_API_ERR = 0x0Du,
} switch_hid_event_t;

static const char *kLocalName = "Joy-Con (L)";
static const uint8_t kPeripheralMinorClassGamepad = 0x02;
static const uint8_t kNintendoBaseMacPrefix[3] = {0xD4u, 0xF0u, 0x57u};
static const uint8_t kJoyConSdpSubclass = 0x08u;

static const uint8_t kSwitchJoyConDescriptor[] = {
    0x05, 0x01, 0x09, 0x05, 0xA1, 0x01, 0x06, 0x01, 0xFF, 0x85, 0x21, 0x09, 0x21, 0x75,
    0x08, 0x95, 0x30, 0x81, 0x02, 0x85, 0x30, 0x09, 0x30, 0x75, 0x08, 0x95, 0x30, 0x81,
    0x02, 0x85, 0x31, 0x09, 0x31, 0x75, 0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x32,
    0x09, 0x32, 0x75, 0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x33, 0x09, 0x33, 0x75,
    0x08, 0x96, 0x69, 0x01, 0x81, 0x02, 0x85, 0x3F, 0x05, 0x09, 0x19, 0x01, 0x29, 0x10,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x10, 0x81, 0x02, 0x05, 0x01, 0x09, 0x39,
    0x15, 0x00, 0x25, 0x07, 0x75, 0x04, 0x95, 0x01, 0x81, 0x42, 0x05, 0x09, 0x75, 0x04,
    0x95, 0x01, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x33, 0x09, 0x34,
    0x16, 0x00, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10, 0x95, 0x04, 0x81, 0x02,
    0x06, 0x01, 0xFF, 0x85, 0x01, 0x09, 0x01, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0x85,
    0x10, 0x09, 0x10, 0x75, 0x08, 0x95, 0x30, 0x91, 0x02, 0x85, 0x11, 0x09, 0x11, 0x75,
    0x08, 0x95, 0x30, 0x91, 0x02, 0x85, 0x12, 0x09, 0x12, 0x75, 0x08, 0x95, 0x30, 0x91,
    0x02, 0xC0,
};

static const esp_hidd_app_param_t kSwitchJoyConApp = {
    .name = "Wireless Gamepad",
    .description = "Gamepad",
    .provider = "Nintendo",
    .subclass = kJoyConSdpSubclass,
    .desc_list = (uint8_t *)kSwitchJoyConDescriptor,
    .desc_list_len = sizeof(kSwitchJoyConDescriptor),
};

static const esp_hidd_qos_param_t kQos = {
    .service_type = 0,
    .token_rate = 0,
    .token_bucket_size = 0,
    .peak_bandwidth = 0,
    .access_latency = 0,
    .delay_variation = 0,
};

static const uint8_t kFallbackAddressBE[6] = {0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C};
static const uint8_t kNeutralRumble[8] = {0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40};
static const uint8_t kSerialNumber[16] = {
    'C', 'D', 'X', 'L', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1', 0x00,
};
static const uint8_t kFactoryImuCalibration[24] = {
    0xB0, 0xFF, 0xB9, 0xFE, 0xE0, 0x00, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x0E, 0x00, 0xDF, 0xFF, 0xD0, 0xFF, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34,
};
static const uint8_t kLeftStickCalibration[9] = {0xF7, 0x44, 0x42, 0x9F, 0x07, 0x8A, 0x10, 0x95, 0x47};
static const uint8_t kRightStickCalibration[9] = {0x9F, 0x07, 0x8A, 0x10, 0x95, 0x47, 0xF7, 0x44, 0x42};
static const uint8_t kBodyColor[3] = {0x0A, 0xB9, 0xE6};
static const uint8_t kButtonColor[3] = {0x1E, 0x1E, 0x1E};
static const uint8_t kHorizontalOffsets[6] = {0x5E, 0x01, 0x00, 0x00, 0xF1, 0x0F};
static const uint8_t kStickParameters[18] = {
    0x19, 0xD0, 0x4C, 0xAE, 0x40, 0xE1, 0xEE, 0xE2, 0x2E,
    0xEE, 0xE2, 0x2E, 0xB4, 0x4A, 0xAB, 0x96, 0x64, 0x49,
};

static sb_controller_state_t s_state;
static sb_status_payload_t s_status = {
    .flags = SB_STATUS_FLAG_BRIDGE_READY,
    .protocol_mode = 0,
    .input_report_mode = SWITCH_REPORT_STANDARD_FULL,
    .battery_level = 8,
    .last_host_report_id = 0,
    .last_error = 0,
    .last_subcommand = 0,
    .last_hid_event = 0,
    .last_hid_status = 0,
    .last_hid_conn_status = 0,
    .last_hid_report_type = 0,
    .last_hid_report_id = 0,
    .last_gap_event = 0,
    .last_gap_status = 0,
    .last_gap_reason = 0,
    .bond_device_count = 0,
};
static uint64_t s_last_report_us = 0;
static uint8_t s_report_timer = 0;
static uint8_t s_player_lights = 0;
static bool s_imu_enabled = false;
static bool s_vibration_enabled = false;
static bool s_shipment_low_power = false;

static void configure_gap_identity(void) {
  esp_bt_cod_t cod = {
      .major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL,
      .minor = kPeripheralMinorClassGamepad,
      .service = ESP_BT_COD_SRVC_LMTD_DISCOVER,
      .reserved_2 = 0,
      .reserved_8 = 0,
  };

  esp_bt_dev_set_device_name(kLocalName);
  esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
}

static void note_hid_api_error(uint8_t event, uint8_t status) {
  s_status.last_hid_event = event;
  s_status.last_hid_status = status;
  s_status.last_error = status;
}

static void note_hid_connection_open(uint8_t status, uint8_t conn_status) {
  s_status.last_hid_event = SWITCH_HID_EVENT_OPEN;
  s_status.last_hid_status = status;
  s_status.last_hid_conn_status = conn_status;
  if (status != SWITCH_HID_STATUS_SUCCESS) {
    s_status.last_error = status;
  }
  if (status == SWITCH_HID_STATUS_SUCCESS &&
      conn_status == (uint8_t)ESP_HIDD_CONN_STATE_CONNECTED) {
    s_status.flags |= SB_STATUS_FLAG_CONNECTED | SB_STATUS_FLAG_VIRTUAL_CABLE;
    s_last_report_us = 0;
  }
}

static void note_hid_connection_close(uint8_t status, uint8_t conn_status) {
  s_status.last_hid_event = SWITCH_HID_EVENT_CLOSE;
  s_status.last_hid_status = status;
  s_status.last_hid_conn_status = conn_status;
  if (status != SWITCH_HID_STATUS_SUCCESS) {
    s_status.last_error = status;
  }
  s_status.flags &= (uint8_t)~SB_STATUS_FLAG_CONNECTED;
}

static esp_err_t send_hid_report(esp_hidd_report_type_t report_type,
                                 uint8_t report_id,
                                 size_t report_len,
                                 const uint8_t *report_data) {
  if (report_data == NULL || report_len == 0u) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = esp_bt_hid_device_send_report(
      report_type, report_id, (uint16_t)report_len, (uint8_t *)report_data);

  s_status.last_hid_event = SWITCH_HID_EVENT_SEND_REPORT;
  s_status.last_hid_report_type = (uint8_t)report_type;
  s_status.last_hid_report_id = report_id;
  if (err != ESP_OK) {
    note_hid_api_error(SWITCH_HID_EVENT_SEND_REPORT, SWITCH_HID_STATUS_ERROR);
  }

  return err;
}

static esp_err_t configure_nintendo_like_base_mac(void) {
  uint8_t efuse_mac[6] = {0};
  uint8_t base_mac[6] = {0};
  esp_err_t err = esp_efuse_mac_get_default(efuse_mac);

  if (err != ESP_OK) {
    return err;
  }

  memcpy(base_mac, efuse_mac, sizeof(base_mac));
  memcpy(base_mac, kNintendoBaseMacPrefix, sizeof(kNintendoBaseMacPrefix));
  return esp_base_mac_addr_set(base_mac);
}

static void copy_bt_address_be(uint8_t out[6]) {
  const uint8_t *address = esp_bt_dev_get_address();
  if (address == NULL) {
    memcpy(out, kFallbackAddressBE, sizeof(kFallbackAddressBE));
    return;
  }
  memcpy(out, address, 6u);
}

static void copy_bt_address_le(uint8_t out[6]) {
  uint8_t address_be[6];
  copy_bt_address_be(address_be);
  for (size_t i = 0; i < 6u; ++i) {
    out[i] = address_be[5u - i];
  }
}

static void overlay_spi_range(uint32_t request_addr,
                              uint8_t *dst,
                              size_t dst_len,
                              uint32_t range_addr,
                              const uint8_t *range_data,
                              size_t range_len) {
  uint32_t request_end = 0;
  uint32_t range_end = 0;
  uint32_t overlap_start = 0;
  uint32_t overlap_end = 0;

  if (dst == NULL || range_data == NULL || dst_len == 0u || range_len == 0u) {
    return;
  }

  request_end = request_addr + (uint32_t)dst_len;
  range_end = range_addr + (uint32_t)range_len;
  if (request_end <= range_addr || range_end <= request_addr) {
    return;
  }

  overlap_start = request_addr > range_addr ? request_addr : range_addr;
  overlap_end = request_end < range_end ? request_end : range_end;

  memcpy(dst + (overlap_start - request_addr),
         range_data + (overlap_start - range_addr),
         (size_t)(overlap_end - overlap_start));
}

static void read_switch_spi_flash(uint32_t address, uint8_t *out, size_t len) {
  uint8_t patchram_addr_record[9] = {0x40, 0x06, 0x00, 0, 0, 0, 0, 0, 0};
  uint8_t device_type[1] = {SWITCH_CONTROLLER_TYPE_LEFT_JOYCON};
  uint8_t factory_unknown[1] = {0xA0};
  uint8_t color_info_present[1] = {0x01};
  uint8_t shipment_state[1] = {(uint8_t)(s_shipment_low_power ? 0x01u : 0x00u)};

  if (out == NULL || len == 0u) {
    return;
  }

  memset(out, 0xFF, len);

  copy_bt_address_le(&patchram_addr_record[3]);

  overlay_spi_range(address, out, len, 0x0012u, patchram_addr_record, sizeof(patchram_addr_record));
  overlay_spi_range(address, out, len, 0x5000u, shipment_state, sizeof(shipment_state));
  overlay_spi_range(address, out, len, 0x6000u, kSerialNumber, sizeof(kSerialNumber));
  overlay_spi_range(address, out, len, 0x6012u, device_type, sizeof(device_type));
  overlay_spi_range(address, out, len, 0x6013u, factory_unknown, sizeof(factory_unknown));
  overlay_spi_range(address, out, len, 0x601Bu, color_info_present, sizeof(color_info_present));
  overlay_spi_range(address, out, len, 0x6020u, kFactoryImuCalibration, sizeof(kFactoryImuCalibration));
  overlay_spi_range(address, out, len, 0x603Du, kLeftStickCalibration, sizeof(kLeftStickCalibration));
  overlay_spi_range(address, out, len, 0x6046u, kRightStickCalibration, sizeof(kRightStickCalibration));
  overlay_spi_range(address, out, len, 0x6050u, kBodyColor, sizeof(kBodyColor));
  overlay_spi_range(address, out, len, 0x6053u, kButtonColor, sizeof(kButtonColor));
  overlay_spi_range(address, out, len, 0x6080u, kHorizontalOffsets, sizeof(kHorizontalOffsets));
  overlay_spi_range(address, out, len, 0x6086u, kStickParameters, sizeof(kStickParameters));
  overlay_spi_range(address, out, len, 0x6098u, kStickParameters, sizeof(kStickParameters));
}

static uint8_t clamp_battery_level(uint8_t level) {
  return level > 8u ? 8u : level;
}

static uint8_t clamp_u8_count(int value) {
  if (value <= 0) {
    return 0u;
  }
  if (value >= 255) {
    return 0xFFu;
  }
  return (uint8_t)value;
}

static void refresh_bond_device_count(void) {
  s_status.bond_device_count = clamp_u8_count(esp_bt_gap_get_bond_device_num());
}

static void note_gap_event(esp_bt_gap_cb_event_t event, uint8_t status, uint8_t reason) {
  s_status.last_gap_event = (uint8_t)event;
  s_status.last_gap_status = status;
  s_status.last_gap_reason = reason;
  refresh_bond_device_count();
}

static uint8_t build_battery_and_connection(void) {
  uint8_t battery = clamp_battery_level(s_status.battery_level);
  battery &= (uint8_t)~1u;
  if ((s_state.misc & SB_MISC_CHARGING) != 0u && battery < 9u) {
    battery = (uint8_t)(battery + 1u);
  }
  return (uint8_t)((battery << 4) | SWITCH_CONNECTION_INFO_JOYCON_WIRELESS);
}

static uint16_t scale_axis_u12(int16_t value) {
  int32_t shifted = (int32_t)value + 32768;
  if (shifted <= 0) {
    return 0;
  }
  if (shifted >= 65535) {
    return 0x0FFFu;
  }
  return (uint16_t)(((uint32_t)shifted * 0x0FFFu) / 65535u);
}

static uint16_t scale_axis_u16(int16_t value) {
  int32_t shifted = (int32_t)value + 32768;
  if (shifted <= 0) {
    return 0u;
  }
  if (shifted >= 65535) {
    return 0xFFFFu;
  }
  return (uint16_t)shifted;
}

static void write_le16(uint8_t *dst, uint16_t value) {
  if (dst == NULL) {
    return;
  }
  dst[0] = (uint8_t)(value & 0xFFu);
  dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void pack_switch_stick(uint16_t x, uint16_t y, uint8_t out[3]) {
  out[0] = (uint8_t)(x & 0xFFu);
  out[1] = (uint8_t)(((x >> 8) & 0x0Fu) | ((y & 0x0Fu) << 4));
  out[2] = (uint8_t)((y >> 4) & 0xFFu);
}

static uint32_t hat_to_left_dpad_bits(uint8_t hat) {
  switch (hat) {
    case 0u:
      return SB_BTN_LJC_UP | SB_BTN_LJC_RIGHT;
    case 1u:
      return SB_BTN_LJC_RIGHT | SB_BTN_LJC_UP;
    case 2u:
      return SB_BTN_LJC_RIGHT | SB_BTN_LJC_DOWN;
    case 3u:
      return SB_BTN_LJC_RIGHT | SB_BTN_LJC_DOWN;
    case 4u:
      return SB_BTN_LJC_DOWN | SB_BTN_LJC_LEFT;
    case 5u:
      return SB_BTN_LJC_DOWN | SB_BTN_LJC_LEFT;
    case 6u:
      return SB_BTN_LJC_LEFT | SB_BTN_LJC_UP;
    case 7u:
      return SB_BTN_LJC_LEFT | SB_BTN_LJC_UP;
    default:
      return 0u;
  }
}

static void build_common_input_report(uint8_t report[SWITCH_INPUT_COMMON_BYTES]) {
  uint32_t buttons = s_state.buttons | hat_to_left_dpad_bits(s_state.hat);
  uint8_t left_buttons = 0;
  uint8_t shared_buttons = 0;

  if ((buttons & SB_BTN_LJC_DOWN) != 0u) {
    left_buttons |= 0x01u;
  }
  if ((buttons & SB_BTN_LJC_UP) != 0u) {
    left_buttons |= 0x02u;
  }
  if ((buttons & SB_BTN_LJC_RIGHT) != 0u) {
    left_buttons |= 0x04u;
  }
  if ((buttons & SB_BTN_LJC_LEFT) != 0u) {
    left_buttons |= 0x08u;
  }
  if ((buttons & SB_BTN_LJC_SR) != 0u) {
    left_buttons |= 0x10u;
  }
  if ((buttons & SB_BTN_LJC_SL) != 0u) {
    left_buttons |= 0x20u;
  }
  if ((buttons & SB_BTN_LJC_L) != 0u) {
    left_buttons |= 0x40u;
  }
  if ((buttons & SB_BTN_LJC_ZL) != 0u) {
    left_buttons |= 0x80u;
  }

  if ((buttons & SB_BTN_LJC_MINUS) != 0u) {
    shared_buttons |= 0x01u;
  }
  if ((buttons & SB_BTN_LJC_STICK) != 0u) {
    shared_buttons |= 0x08u;
  }
  if ((buttons & SB_BTN_LJC_CAPTURE) != 0u) {
    shared_buttons |= 0x20u;
  }
  if ((s_state.misc & SB_MISC_CHARGING_GRIP) != 0u) {
    shared_buttons |= 0x80u;
  }

  memset(report, 0, SWITCH_INPUT_COMMON_BYTES);
  report[0] = s_report_timer++;
  report[1] = build_battery_and_connection();
  report[2] = 0x00u;
  report[3] = shared_buttons;
  report[4] = left_buttons;
  pack_switch_stick(scale_axis_u12(s_state.lx), scale_axis_u12(s_state.ly), &report[5]);
  pack_switch_stick(scale_axis_u12(s_state.rx), scale_axis_u12(s_state.ry), &report[8]);
  report[11] = 0x80u;
}

static void build_simple_input_report(uint8_t report[SWITCH_SIMPLE_REPORT_BYTES]) {
  uint32_t buttons = s_state.buttons | hat_to_left_dpad_bits(s_state.hat);
  uint16_t left_x = scale_axis_u16(s_state.lx);
  uint16_t left_y = scale_axis_u16(s_state.ly);
  uint16_t right_x = scale_axis_u16(s_state.rx);
  uint16_t right_y = scale_axis_u16(s_state.ry);

  memset(report, 0, SWITCH_SIMPLE_REPORT_BYTES);

  if ((buttons & SB_BTN_LJC_DOWN) != 0u) {
    report[0] |= 0x01u;
  }
  if ((buttons & SB_BTN_LJC_RIGHT) != 0u) {
    report[0] |= 0x02u;
  }
  if ((buttons & SB_BTN_LJC_LEFT) != 0u) {
    report[0] |= 0x04u;
  }
  if ((buttons & SB_BTN_LJC_UP) != 0u) {
    report[0] |= 0x08u;
  }
  if ((buttons & SB_BTN_LJC_SL) != 0u) {
    report[0] |= 0x10u;
  }
  if ((buttons & SB_BTN_LJC_SR) != 0u) {
    report[0] |= 0x20u;
  }

  if ((buttons & SB_BTN_LJC_MINUS) != 0u) {
    report[1] |= 0x01u;
  }
  if ((buttons & SB_BTN_LJC_STICK) != 0u) {
    report[1] |= 0x04u;
  }
  if ((buttons & SB_BTN_LJC_CAPTURE) != 0u) {
    report[1] |= 0x20u;
  }
  if ((buttons & SB_BTN_LJC_L) != 0u) {
    report[1] |= 0x40u;
  }
  if ((buttons & SB_BTN_LJC_ZL) != 0u) {
    report[1] |= 0x80u;
  }

  report[2] = (uint8_t)(s_state.hat <= 7u ? s_state.hat : 8u);
  write_le16(&report[3], left_x);
  write_le16(&report[5], left_y);
  write_le16(&report[7], right_x);
  write_le16(&report[9], right_y);
}

static esp_err_t send_subcommand_reply(uint8_t ack,
                                       uint8_t subcommand,
                                       const uint8_t *reply_data,
                                       size_t reply_len) {
  uint8_t report[SWITCH_SUBCOMMAND_REPLY_BYTES];

  if (reply_len > (SWITCH_SUBCOMMAND_REPLY_BYTES - SWITCH_INPUT_COMMON_BYTES - 2u)) {
    reply_len = SWITCH_SUBCOMMAND_REPLY_BYTES - SWITCH_INPUT_COMMON_BYTES - 2u;
  }

  memset(report, 0, sizeof(report));
  build_common_input_report(report);
  report[12] = ack;
  report[13] = subcommand;

  if (reply_len > 0u && reply_data != NULL) {
    memcpy(&report[14], reply_data, reply_len);
  }

  return send_hid_report(
      ESP_HIDD_REPORT_TYPE_INTRDATA, SWITCH_REPORT_SUBCOMMAND_REPLY, sizeof(report), report);
}

static void build_device_info_reply(uint8_t reply[11]) {
  uint8_t address_be[6];

  copy_bt_address_be(address_be);

  memset(reply, 0, 11u);
  reply[0] = 0x04u;
  reply[1] = 0x00u;
  reply[2] = SWITCH_CONTROLLER_TYPE_LEFT_JOYCON;
  reply[3] = 0x02u;
  memcpy(&reply[4], address_be, sizeof(address_be));
  reply[10] = 0x01u;
}

static void reply_spi_flash_read(const uint8_t *args, size_t args_len) {
  uint8_t reply[5u + SWITCH_SPI_READ_MAX_BYTES];
  uint8_t size = 0;
  uint32_t address = 0;

  if (args_len < 5u) {
    (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, SWITCH_SUBCMD_SPI_FLASH_READ, NULL, 0u);
    return;
  }

  address = (uint32_t)args[0] | ((uint32_t)args[1] << 8) | ((uint32_t)args[2] << 16) |
            ((uint32_t)args[3] << 24);
  size = args[4];
  if (size > SWITCH_SPI_READ_MAX_BYTES) {
    size = SWITCH_SPI_READ_MAX_BYTES;
  }

  memset(reply, 0, sizeof(reply));
  memcpy(reply, args, 5u);
  read_switch_spi_flash(address, &reply[5], size);
  (void)send_subcommand_reply(
      SWITCH_ACK_SPI_FLASH_READ, SWITCH_SUBCMD_SPI_FLASH_READ, reply, (size_t)size + 5u);
}

static uint16_t get_regulated_voltage_sample(void) {
  uint8_t battery = clamp_battery_level(s_status.battery_level);

  if (battery >= 8u) {
    return 0x0650u;
  }
  if (battery >= 6u) {
    return 0x0600u;
  }
  if (battery >= 4u) {
    return 0x05C0u;
  }
  return 0x0550u;
}

static void build_trigger_elapsed_reply(uint8_t reply[7]) {
  memset(reply, 0, 7u);
  reply[3] = 0x2Cu;
  reply[4] = 0x01u;
  reply[5] = 0x2Cu;
  reply[6] = 0x01u;
}

static void handle_subcommand(uint8_t subcommand, const uint8_t *args, size_t args_len) {
  uint8_t reply[35];

  s_status.last_subcommand = subcommand;

  switch (subcommand) {
    case SWITCH_SUBCMD_GET_ONLY_CONTROLLER_STATE:
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_GET_DEVICE_INFO:
      build_device_info_reply(reply);
      (void)send_subcommand_reply(SWITCH_ACK_DEVICE_INFO, subcommand, reply, 11u);
      break;

    case SWITCH_SUBCMD_SET_INPUT_REPORT_MODE:
      if (args_len >= 1u) {
        s_status.input_report_mode = args[0];
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_TRIGGER_BUTTONS_ELAPSED:
      build_trigger_elapsed_reply(reply);
      (void)send_subcommand_reply(SWITCH_ACK_TRIGGER_BUTTONS, subcommand, reply, 7u);
      break;

    case SWITCH_SUBCMD_SET_SHIPMENT_LOW_POWER:
      if (args_len >= 1u) {
        s_shipment_low_power = args[0] != 0u;
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_SPI_FLASH_READ:
      reply_spi_flash_read(args, args_len);
      break;

    case SWITCH_SUBCMD_SET_PLAYER_LIGHTS:
      if (args_len >= 1u) {
        s_player_lights = args[0];
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_GET_PLAYER_LIGHTS:
      reply[0] = s_player_lights;
      (void)send_subcommand_reply(SWITCH_ACK_PLAYER_LIGHTS, subcommand, reply, 1u);
      break;

    case SWITCH_SUBCMD_ENABLE_IMU:
      if (args_len >= 1u) {
        s_imu_enabled = args[0] != 0u;
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_READ_IMU_REG:
      memset(reply, 0, sizeof(reply));
      if (args_len >= 2u) {
        reply[0] = args[0];
        reply[1] = args[1];
      }
      (void)send_subcommand_reply(
          SWITCH_ACK_IMU_REG_READ, subcommand, reply, args_len >= 2u ? 2u : 0u);
      break;

    case SWITCH_SUBCMD_ENABLE_VIBRATION:
      if (args_len >= 1u) {
        s_vibration_enabled = args[0] != 0u;
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_GET_REGULATED_VOLTAGE: {
      uint16_t voltage = get_regulated_voltage_sample();
      reply[0] = (uint8_t)(voltage & 0xFFu);
      reply[1] = (uint8_t)((voltage >> 8) & 0xFFu);
      (void)send_subcommand_reply(SWITCH_ACK_REGULATED_VOLTAGE, subcommand, reply, 2u);
      break;
    }

    default:
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;
  }
}

static void handle_output_report(uint8_t report_id, const uint8_t *data, size_t len) {
  if (data == NULL) {
    return;
  }

  s_status.last_host_report_id = report_id;

  if (report_id == SWITCH_OUTPUT_RUMBLE_ONLY) {
    if (len >= (sizeof(kNeutralRumble) + 1u)) {
      s_vibration_enabled = memcmp(data + 1u, kNeutralRumble, sizeof(kNeutralRumble)) != 0;
    }
    return;
  }

  if (report_id == SWITCH_OUTPUT_SUBCOMMAND) {
    if (len >= 10u) {
      if (len >= 9u) {
        s_vibration_enabled = memcmp(&data[1], kNeutralRumble, sizeof(kNeutralRumble)) != 0;
      }
      handle_subcommand(data[9], &data[10], len - 10u);
    }
    return;
  }

  if (report_id == SWITCH_OUTPUT_NFC_IR || report_id == SWITCH_OUTPUT_UNKNOWN) {
    return;
  }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  esp_bt_pin_code_t pin_code = {0};

  if (param == NULL) {
    note_gap_event(event, 0u, 0u);
    return;
  }

  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      note_gap_event(event, (uint8_t)param->auth_cmpl.stat, 0u);
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
      note_gap_event(event, param->pin_req.min_16_digit ? 16u : 4u, 0u);
      if (param->pin_req.min_16_digit) {
        (void)esp_bt_gap_pin_reply(param->pin_req.bda, true, 16u, pin_code);
      } else {
        pin_code[0] = '0';
        pin_code[1] = '0';
        pin_code[2] = '0';
        pin_code[3] = '0';
        (void)esp_bt_gap_pin_reply(param->pin_req.bda, true, 4u, pin_code);
      }
      break;

#if (CONFIG_BT_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
      note_gap_event(event, 1u, 0u);
      (void)esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
      note_gap_event(event, 0u, 0u);
      break;

    case ESP_BT_GAP_KEY_REQ_EVT:
      note_gap_event(event, 0u, 0u);
      break;
#endif

    case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
      note_gap_event(event, (uint8_t)param->config_eir_data.stat, 0u);
      break;

    case ESP_BT_GAP_MODE_CHG_EVT:
      note_gap_event(event, (uint8_t)param->mode_chg.mode, 0u);
      break;

    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
      note_gap_event(event, (uint8_t)param->acl_conn_cmpl_stat.stat, 0u);
      break;

    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
      note_gap_event(event, 0u, (uint8_t)param->acl_disconn_cmpl_stat.reason);
      break;

    default:
      note_gap_event(event, 0u, 0u);
      break;
  }
}

static void hid_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
  s_status.last_hid_event = (uint8_t)event;

  switch (event) {
    case ESP_HIDD_INIT_EVT:
      s_status.last_hid_status = (uint8_t)param->init.status;
      if (param->init.status == ESP_HIDD_SUCCESS) {
        esp_bt_hid_device_register_app((esp_hidd_app_param_t *)&kSwitchJoyConApp,
                                       (esp_hidd_qos_param_t *)&kQos,
                                       (esp_hidd_qos_param_t *)&kQos);
      } else {
        s_status.last_error = (uint8_t)param->init.status;
      }
      break;

    case ESP_HIDD_REGISTER_APP_EVT:
      s_status.last_hid_status = (uint8_t)param->register_app.status;
      if (param->register_app.status == ESP_HIDD_SUCCESS) {
        s_status.flags |= SB_STATUS_FLAG_HID_READY | SB_STATUS_FLAG_BT_READY;
        if (param->register_app.in_use) {
          s_status.flags |= SB_STATUS_FLAG_VIRTUAL_CABLE;
          (void)esp_bt_hid_device_connect(param->register_app.bd_addr);
        }

        configure_gap_identity();
      } else {
        s_status.last_error = (uint8_t)param->register_app.status;
      }
      break;

    case ESP_HIDD_OPEN_EVT:
      note_hid_connection_open((uint8_t)param->open.status, (uint8_t)param->open.conn_status);
      break;

    case ESP_HIDD_CLOSE_EVT:
      note_hid_connection_close((uint8_t)param->close.status, (uint8_t)param->close.conn_status);
      break;

    case ESP_HIDD_SET_PROTOCOL_EVT:
      s_status.protocol_mode = (uint8_t)param->set_protocol.protocol_mode;
      break;

    case ESP_HIDD_SET_REPORT_EVT:
      s_status.last_hid_report_type = (uint8_t)param->set_report.report_type;
      s_status.last_hid_report_id = param->set_report.report_id;
      handle_output_report(
          param->set_report.report_id, param->set_report.data, (size_t)param->set_report.len);
      break;

    case ESP_HIDD_GET_REPORT_EVT:
      s_status.last_hid_report_type = (uint8_t)param->get_report.report_type;
      s_status.last_hid_report_id = param->get_report.report_id;
      break;

    case ESP_HIDD_INTR_DATA_EVT:
      s_status.last_hid_report_type = (uint8_t)ESP_HIDD_REPORT_TYPE_INTRDATA;
      s_status.last_hid_report_id = param->intr_data.report_id;
      handle_output_report(
          param->intr_data.report_id, param->intr_data.data, (size_t)param->intr_data.len);
      break;

    case ESP_HIDD_SEND_REPORT_EVT:
      s_status.last_hid_status = (uint8_t)param->send_report.status;
      s_status.last_hid_report_type = (uint8_t)param->send_report.report_type;
      s_status.last_hid_report_id = param->send_report.report_id;
      if (param->send_report.status != ESP_HIDD_SUCCESS) {
        s_status.last_error = param->send_report.reason;
      }
      break;

    case ESP_HIDD_REPORT_ERR_EVT:
      s_status.last_hid_status = (uint8_t)param->report_err.status;
      break;

    case ESP_HIDD_VC_UNPLUG_EVT:
      s_status.last_hid_status = (uint8_t)param->vc_unplug.status;
      s_status.last_hid_conn_status = (uint8_t)param->vc_unplug.conn_status;
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

  err = configure_nintendo_like_base_mac();
  if (err != ESP_OK) {
    return err;
  }

  {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
  }
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

#if (CONFIG_BT_SSP_ENABLED == true)
  {
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    err = esp_bt_gap_set_security_param(param_type, &iocap, sizeof(iocap));
  }
  if (err != ESP_OK) {
    return err;
  }
#endif

  {
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code = {0};
    err = esp_bt_gap_set_pin(pin_type, 0u, pin_code);
  }
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
  s_status.battery_level = clamp_battery_level(state->battery_level);
}

void switch_hid_get_status(sb_status_payload_t *status) {
  if (status == NULL) {
    return;
  }

  refresh_bond_device_count();
  *status = s_status;
}

void switch_hid_tick(void) {
  uint64_t now = 0;

  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) == 0u ||
      (s_status.flags & SB_STATUS_FLAG_HID_READY) == 0u) {
    return;
  }

  now = esp_timer_get_time();
  if ((now - s_last_report_us) < 16666u) {
    return;
  }

  s_last_report_us = now;

  {
    uint8_t report_id = SWITCH_REPORT_STANDARD_FULL;
    uint8_t report[SWITCH_STANDARD_REPORT_BYTES];
    size_t report_len = SWITCH_STANDARD_REPORT_BYTES;

    if (s_status.input_report_mode == SWITCH_REPORT_SIMPLE_HID) {
      report_id = SWITCH_REPORT_SIMPLE_HID;
      report_len = SWITCH_SIMPLE_REPORT_BYTES;
    } else if (s_status.input_report_mode == SWITCH_REPORT_STANDARD_ALT0 ||
        s_status.input_report_mode == SWITCH_REPORT_STANDARD_ALT1) {
      report_id = s_status.input_report_mode;
    }

    memset(report, 0, sizeof(report));
    if (report_id == SWITCH_REPORT_SIMPLE_HID) {
      build_simple_input_report(report);
    } else {
      build_common_input_report(report);
    }
    if (report_id != SWITCH_REPORT_SIMPLE_HID && s_imu_enabled) {
      memset(&report[12], 0, sizeof(report) - 12u);
    }
    (void)send_hid_report(ESP_HIDD_REPORT_TYPE_INTRDATA, report_id, report_len, report);
  }
}

esp_err_t switch_hid_virtual_cable_unplug(void) {
  return esp_bt_hid_device_virtual_cable_unplug();
}

esp_err_t switch_hid_clear_all_bonds(void) {
  esp_err_t result = ESP_OK;

  for (uint8_t attempt = 0; attempt < 6u; ++attempt) {
    esp_bd_addr_t devices[SWITCH_MAX_BOND_DEVICES];
    int device_count = esp_bt_gap_get_bond_device_num();

    refresh_bond_device_count();
    if (device_count <= 0) {
      return result;
    }

    if (device_count > SWITCH_MAX_BOND_DEVICES) {
      device_count = SWITCH_MAX_BOND_DEVICES;
    }

    memset(devices, 0, sizeof(devices));
    result = esp_bt_gap_get_bond_device_list(&device_count, devices);
    if (result != ESP_OK) {
      s_status.last_error = 0xFEu;
      return result;
    }

    for (int i = 0; i < device_count; ++i) {
      esp_err_t remove_err = esp_bt_gap_remove_bond_device(devices[i]);
      if (remove_err != ESP_OK) {
        result = remove_err;
        s_status.last_error = 0xFDu;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(150));
  }

  refresh_bond_device_count();
  return result;
}
