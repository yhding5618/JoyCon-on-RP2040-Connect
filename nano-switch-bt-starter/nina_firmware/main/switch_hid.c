#include "switch_hid.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidd_api.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs.h"

#define SWITCH_ACK_SIMPLE 0x80u
#define SWITCH_ACK_DEVICE_INFO 0x82u
#define SWITCH_ACK_TRIGGER_BUTTONS 0x83u
#define SWITCH_ACK_SPI_FLASH_READ 0x90u
#define SWITCH_ACK_IMU_REG_READ 0xC0u
#define SWITCH_ACK_PLAYER_LIGHTS 0xB0u
#define SWITCH_ACK_REGULATED_VOLTAGE 0xD0u
#define SWITCH_ACK_NFC_IR_MCU_CONFIG 0xA0u

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
#define SWITCH_SUBCMD_SET_NFC_IR_MCU_CONFIG 0x21u
#define SWITCH_SUBCMD_SET_NFC_IR_MCU_STATE 0x22u
#define SWITCH_SUBCMD_SET_PLAYER_LIGHTS 0x30u
#define SWITCH_SUBCMD_GET_PLAYER_LIGHTS 0x31u
#define SWITCH_SUBCMD_ENABLE_IMU 0x40u
#define SWITCH_SUBCMD_READ_IMU_REG 0x43u
#define SWITCH_SUBCMD_ENABLE_VIBRATION 0x48u
#define SWITCH_SUBCMD_GET_REGULATED_VOLTAGE 0x50u

#define SWITCH_CONTROLLER_TYPE_LEFT_JOYCON 0x01u
#define SWITCH_CONTROLLER_TYPE_RIGHT_JOYCON 0x02u
#define SWITCH_CONTROLLER_TYPE_PRO 0x03u
#define SWITCH_CONTROLLER_TYPE_PRO_CONTROLLER SWITCH_CONTROLLER_TYPE_PRO

#define SWITCH_STICK_CENTER 0x0800u
#define SWITCH_SIMPLE_STICK_CENTER 0x8000u
#define SWITCH_INPUT_COMMON_BYTES 12u
#define SWITCH_SUBCOMMAND_REPLY_BYTES 48u
#define SWITCH_STANDARD_REPORT_BYTES 48u
#define SWITCH_SIMPLE_REPORT_BYTES 11u
#define SWITCH_DEVICE_INFO_REPLY_BYTES 12u
#define SWITCH_TRIGGER_ELAPSED_REPLY_BYTES 14u
#define SWITCH_NFC_IR_MCU_CONFIG_REPLY_BYTES 34u
#define SWITCH_SPI_READ_MAX_BYTES 0x1Du
#define SWITCH_MAX_BOND_DEVICES 16
#define SWITCH_EVENT_RING_SIZE SB_EVENT_LOG_MAX_ENTRIES
#define SWITCH_BD_ADDR_LEN 6u
#define SWITCH_RECONNECT_TASK_STACK 3072u

#define SWITCH_HID_STATUS_SUCCESS 0x00u
#define SWITCH_HID_STATUS_ERROR 0x01u

#define NINA_PAIR_SLOT_MAGIC 0x53575052u
#define NINA_PAIR_SLOT_VERSION 1u
#define NINA_MODE_COUNT 3u
#define NINA_NVS_NAMESPACE_MAX_LEN 15u

#define SWITCH_HID_USE_JOYCONTROL_DESCRIPTOR 1

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#define SWITCH_HAS_BT_CONFIG_FILE_PATH_UPDATE 1
#else
#define SWITCH_HAS_BT_CONFIG_FILE_PATH_UPDATE 0
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#define SWITCH_HAS_IFACE_MAC_ADDR_SET 1
#else
#define SWITCH_HAS_IFACE_MAC_ADDR_SET 0
#endif

#define BDSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define BDARG(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]

#if defined(__GNUC__)
#define SWITCH_HID_MAYBE_UNUSED __attribute__((unused))
#define SWITCH_HID_PACKED __attribute__((packed))
#else
#define SWITCH_HID_MAYBE_UNUSED
#define SWITCH_HID_PACKED
#endif

static const char *TAG = "switch_hid";

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

typedef enum {
  SWITCH_HID_API_EVENT_SEND_REPORT = 0x01u,
  SWITCH_HID_API_EVENT_REGISTER_APP = 0x02u,
  SWITCH_HID_API_EVENT_CONNECT = 0x03u,
  SWITCH_HID_API_EVENT_VC_UNPLUG = 0x04u,
  SWITCH_HID_API_EVENT_CLEAR_BONDS_BEGIN = 0x05u,
  SWITCH_HID_API_EVENT_CLEAR_BONDS_DONE = 0x06u,
  SWITCH_HID_API_EVENT_REMOVE_BOND = 0x07u,
  SWITCH_HID_API_EVENT_SAVE_HOST = 0x08u,
  SWITCH_HID_API_EVENT_LOAD_SETTINGS = 0x09u,
  SWITCH_HID_API_EVENT_RECONNECT_SKIP = 0x0Au,
  SWITCH_HID_API_EVENT_DISCONNECT = 0x0Bu,
  SWITCH_HID_API_EVENT_CLEAR_SAVED_HOST = 0x0Cu,
  SWITCH_HID_API_EVENT_SAVE_MODE = 0x0Du,
  SWITCH_HID_API_EVENT_SAVE_LOCAL_ADDR = 0x0Eu,
  SWITCH_HID_API_EVENT_SCHEDULE_RECONNECT = 0x0Fu,
} switch_hid_api_event_t;

typedef enum {
  SWITCH_RECONNECT_SKIP_NO_SAVED_HOST = 0x01u,
  SWITCH_RECONNECT_SKIP_NO_BONDS = 0x02u,
  SWITCH_RECONNECT_SKIP_BT_DISABLED = 0x03u,
  SWITCH_RECONNECT_SKIP_ALREADY_CONNECTED = 0x04u,
  SWITCH_RECONNECT_SKIP_STOPPING = 0x05u,
  SWITCH_RECONNECT_SKIP_TASK_CREATE_FAILED = 0x06u,
  SWITCH_RECONNECT_SKIP_STALE_TASK = 0x07u,
} switch_reconnect_skip_reason_t;

typedef enum {
  SWITCH_BT_IDENTITY_STAGE_AFTER_BASE_MAC = 0x01u,
  SWITCH_BT_IDENTITY_STAGE_AFTER_CONTROLLER_INIT = 0x02u,
  SWITCH_BT_IDENTITY_STAGE_AFTER_BLUEDROID_ENABLE = 0x03u,
  SWITCH_BT_IDENTITY_STAGE_AFTER_REGISTER_APP_EVT = 0x04u,
  SWITCH_BT_IDENTITY_STAGE_AFTER_GAP_IDENTITY = 0x05u,
  SWITCH_BT_IDENTITY_STAGE_AFTER_GAP_IDENTITY_SETTLED = 0x06u,
} switch_bt_identity_stage_t;

typedef enum {
  SWITCH_BT_IDENTITY_EVENT_STAGE = 0x01u,
  SWITCH_BT_IDENTITY_EVENT_BT_ADDR_0_1 = 0x02u,
  SWITCH_BT_IDENTITY_EVENT_BT_ADDR_2_3 = 0x03u,
  SWITCH_BT_IDENTITY_EVENT_BT_ADDR_4_5 = 0x04u,
  SWITCH_BT_IDENTITY_EVENT_BASE_MAC_0_1 = 0x05u,
  SWITCH_BT_IDENTITY_EVENT_BASE_MAC_2_3 = 0x06u,
  SWITCH_BT_IDENTITY_EVENT_BASE_MAC_4_5 = 0x07u,
  SWITCH_BT_IDENTITY_EVENT_COD_STATUS = 0x08u,
  SWITCH_BT_IDENTITY_EVENT_COD_MAJOR_MINOR = 0x09u,
  SWITCH_BT_IDENTITY_EVENT_COD_SERVICE = 0x0Au,
  SWITCH_BT_IDENTITY_EVENT_HID_SUBCLASS_DESC_LO = 0x0Bu,
  SWITCH_BT_IDENTITY_EVENT_HID_DESC_HI_NAME_LEN = 0x0Cu,
  SWITCH_BT_IDENTITY_EVENT_HID_STRING_LENGTHS = 0x0Du,
  SWITCH_BT_IDENTITY_EVENT_HID_PROVIDER_LENGTH = 0x0Eu,
  SWITCH_BT_IDENTITY_EVENT_GAP_IDENTITY_API_0 = 0x0Fu,
  SWITCH_BT_IDENTITY_EVENT_GAP_IDENTITY_API_1 = 0x10u,
} switch_bt_identity_event_t;

typedef enum {
  SWITCH_PROFILE_LEFT_JOYCON = SB_CONTROLLER_MODE_LEFT_JOYCON,
  SWITCH_PROFILE_RIGHT_JOYCON = SB_CONTROLLER_MODE_RIGHT_JOYCON,
  SWITCH_PROFILE_PRO_CONTROLLER = SB_CONTROLLER_MODE_PRO_CONTROLLER,
} switch_controller_profile_t;

typedef enum {
  SWITCH_BT_STATE_OFF = 0,
  SWITCH_BT_STATE_STARTING,
  SWITCH_BT_STATE_PAIRABLE,
  SWITCH_BT_STATE_RECONNECTING,
  SWITCH_BT_STATE_CONNECTED,
  SWITCH_BT_STATE_STOPPING,
} switch_bt_state_t;

typedef struct SWITCH_HID_PACKED {
  uint32_t magic;
  uint8_t version;
  uint8_t mode;
  uint8_t has_local_mac;
  uint8_t has_host_addr;
  uint8_t local_bt_mac[SWITCH_BD_ADDR_LEN];
  uint8_t switch_bd_addr[SWITCH_BD_ADDR_LEN];
  uint32_t crc32;
} nina_pair_slot_t;

typedef struct {
  switch_controller_profile_t mode;
  const char *app_nvs_ns;
  const char *bt_bond_path;
  const char *device_name;
  const uint8_t *hid_descriptor;
  size_t hid_descriptor_len;
} nina_mode_cfg_t;

static const uint8_t kPeripheralMinorClassGamepad = 0x02;
static const uint8_t kJoyConSdpSubclass = 0x08u;
static const uint16_t kReconnectBackoffMs[] = {1000u, 3000u, 8000u, 15000u};
static const char kSwitchGlobalNvsNamespace[] = "sw_global";
static const char kSwitchGlobalNvsKeyControllerMode[] = "ctrl_mode";
static const char kSwitchModeNvsKeySlot[] = "slot";

static const uint8_t kCurrentDescriptor[] SWITCH_HID_MAYBE_UNUSED = {
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

static const uint8_t kJoycontrolDescriptor[] SWITCH_HID_MAYBE_UNUSED = {
    0x05, 0x01, 0x15, 0x00, 0x09, 0x04, 0xA1, 0x01,
    0x85, 0x30, 0x05, 0x01, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0A, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x0A, 0x55, 0x00, 0x65, 0x00, 0x81, 0x02,
    0x05, 0x09, 0x19, 0x0B, 0x29, 0x0E, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02,
    0x75, 0x01, 0x95, 0x02, 0x81, 0x03, 0x0B, 0x01,
    0x00, 0x01, 0x00, 0xA1, 0x00, 0x0B, 0x30, 0x00,
    0x01, 0x00, 0x0B, 0x31, 0x00, 0x01, 0x00, 0x0B,
    0x32, 0x00, 0x01, 0x00, 0x0B, 0x35, 0x00, 0x01,
    0x00, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x04, 0x81, 0x02, 0xC0, 0x0B,
    0x39, 0x00, 0x01, 0x00, 0x15, 0x00, 0x25, 0x07,
    0x35, 0x00, 0x46, 0x3B, 0x01, 0x65, 0x14, 0x75,
    0x04, 0x95, 0x01, 0x81, 0x02, 0x05, 0x09, 0x19,
    0x0F, 0x29, 0x12, 0x15, 0x00, 0x25, 0x01, 0x75,
    0x01, 0x95, 0x04, 0x81, 0x02, 0x75, 0x08, 0x95,
    0x34, 0x81, 0x03, 0x06, 0x00, 0xFF, 0x85, 0x21,
    0x09, 0x01, 0x75, 0x08, 0x95, 0x3F, 0x81, 0x03,
    0x85, 0x81, 0x09, 0x02, 0x75, 0x08, 0x95, 0x3F,
    0x81, 0x03, 0x85, 0x01, 0x09, 0x03, 0x75, 0x08,
    0x95, 0x3F, 0x91, 0x83, 0x85, 0x10, 0x09, 0x04,
    0x75, 0x08, 0x95, 0x3F, 0x91, 0x83, 0x85, 0x80,
    0x09, 0x05, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x83,
    0x85, 0x82, 0x09, 0x06, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0xC0,
};

_Static_assert(sizeof(kJoycontrolDescriptor) == 203u, "Unexpected joycontrol descriptor length");

#if SWITCH_HID_USE_JOYCONTROL_DESCRIPTOR
#define SWITCH_HID_DESCRIPTOR kJoycontrolDescriptor
#else
#define SWITCH_HID_DESCRIPTOR kCurrentDescriptor
#endif

static const esp_hidd_app_param_t kSwitchJoyConApp = {
    .name = "Wireless Gamepad",
    .description = "Gamepad",
    .provider = "Nintendo",
    .subclass = kJoyConSdpSubclass,
    .desc_list = (uint8_t *)SWITCH_HID_DESCRIPTOR,
    .desc_list_len = sizeof(SWITCH_HID_DESCRIPTOR),
};

static const nina_mode_cfg_t kNinaModeCfgs[NINA_MODE_COUNT] = {
    [SWITCH_PROFILE_LEFT_JOYCON] = {
        .mode = SWITCH_PROFILE_LEFT_JOYCON,
        .app_nvs_ns = "sw_ljc",
        .bt_bond_path = "bt_ljc",
        .device_name = "Joy-Con (L)",
        .hid_descriptor = SWITCH_HID_DESCRIPTOR,
        .hid_descriptor_len = sizeof(SWITCH_HID_DESCRIPTOR),
    },
    [SWITCH_PROFILE_RIGHT_JOYCON] = {
        .mode = SWITCH_PROFILE_RIGHT_JOYCON,
        .app_nvs_ns = "sw_rjc",
        .bt_bond_path = "bt_rjc",
        .device_name = "Joy-Con (R)",
        .hid_descriptor = SWITCH_HID_DESCRIPTOR,
        .hid_descriptor_len = sizeof(SWITCH_HID_DESCRIPTOR),
    },
    [SWITCH_PROFILE_PRO_CONTROLLER] = {
        .mode = SWITCH_PROFILE_PRO_CONTROLLER,
        .app_nvs_ns = "sw_pro",
        .bt_bond_path = "bt_pro",
        .device_name = "Pro Controller",
        .hid_descriptor = SWITCH_HID_DESCRIPTOR,
        .hid_descriptor_len = sizeof(SWITCH_HID_DESCRIPTOR),
    },
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
static const uint8_t kSerialNumberTemplate[16] = {
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
    .controller_mode = SB_CONTROLLER_MODE_LEFT_JOYCON,
    .bluetooth_enabled = 0,
    .reserved0 = 0,
    .reserved1 = 0,
};
static switch_controller_profile_t s_controller_profile = SWITCH_PROFILE_LEFT_JOYCON;
static nina_pair_slot_t s_active_pair_slot;
static esp_hidd_app_param_t s_active_hid_app;
static switch_bt_state_t s_bt_state = SWITCH_BT_STATE_OFF;
static bool s_bluetooth_enabled = false;
static bool s_bluetooth_stopping = false;
static uint64_t s_last_report_us = 0;
static uint8_t s_report_timer = 0;
static uint8_t s_player_lights = 0;
static bool s_imu_enabled = false;
static bool s_vibration_enabled = false;
static bool s_shipment_low_power = false;
static uint8_t s_intended_base_mac[6] = {0};
static bool s_intended_base_mac_valid = false;
static uint8_t s_saved_switch_host_bdaddr[SWITCH_BD_ADDR_LEN] = {0};
static bool s_saved_switch_host_valid = false;
static uint32_t s_reconnect_generation = 0u;
static TaskHandle_t s_reconnect_task_handle = NULL;
static sb_event_entry_t s_event_ring[SWITCH_EVENT_RING_SIZE];
static size_t s_event_ring_head = 0u;
static size_t s_event_ring_count = 0u;
static uint16_t s_next_event_sequence = 1u;
static bool s_event_ring_overflowed = false;
static portMUX_TYPE s_event_ring_lock = portMUX_INITIALIZER_UNLOCKED;

static uint32_t monotonic_timestamp_ms(void) {
  return (uint32_t)(esp_timer_get_time() / 1000u);
}

static void record_event(uint8_t source, uint8_t event, uint8_t arg0, uint8_t arg1) {
  const uint32_t timestamp_ms = monotonic_timestamp_ms();

  portENTER_CRITICAL(&s_event_ring_lock);

  s_event_ring[s_event_ring_head].timestamp_ms = timestamp_ms;
  s_event_ring[s_event_ring_head].source = source;
  s_event_ring[s_event_ring_head].event = event;
  s_event_ring[s_event_ring_head].arg0 = arg0;
  s_event_ring[s_event_ring_head].arg1 = arg1;

  s_event_ring_head = (s_event_ring_head + 1u) % SWITCH_EVENT_RING_SIZE;
  if (s_event_ring_count < SWITCH_EVENT_RING_SIZE) {
    s_event_ring_count++;
  } else {
    s_event_ring_overflowed = true;
  }
  s_next_event_sequence++;

  portEXIT_CRITICAL(&s_event_ring_lock);
}

static uint8_t bt_identity_stage_id(const char *stage) {
  if (stage == NULL) {
    return 0u;
  }
  if (strcmp(stage, "after_base_mac") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_BASE_MAC;
  }
  if (strcmp(stage, "after_controller_init") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_CONTROLLER_INIT;
  }
  if (strcmp(stage, "after_bluedroid_enable") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_BLUEDROID_ENABLE;
  }
  if (strcmp(stage, "after_register_app_evt") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_REGISTER_APP_EVT;
  }
  if (strcmp(stage, "after_gap_identity") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_GAP_IDENTITY;
  }
  if (strcmp(stage, "after_gap_identity_settled") == 0) {
    return SWITCH_BT_IDENTITY_STAGE_AFTER_GAP_IDENTITY_SETTLED;
  }
  return 0u;
}

static uint8_t clamp_size_to_u8(size_t value) {
  return value > 0xFFu ? 0xFFu : (uint8_t)value;
}

static void reset_controller_runtime_state(void) {
  memset(&s_state, 0, sizeof(s_state));
  s_state.hat = 8u;
  s_state.battery_level = 8u;
  s_status.battery_level = 8u;
  s_player_lights = 0u;
  s_imu_enabled = false;
  s_vibration_enabled = false;
  s_shipment_low_power = false;
  s_last_report_us = 0;
  s_report_timer = 0;
}

static bool switch_profile_from_mode(sb_controller_mode_t mode, switch_controller_profile_t *profile) {
  switch (mode) {
    case SB_CONTROLLER_MODE_LEFT_JOYCON:
      if (profile != NULL) {
        *profile = SWITCH_PROFILE_LEFT_JOYCON;
      }
      return true;
    case SB_CONTROLLER_MODE_RIGHT_JOYCON:
      if (profile != NULL) {
        *profile = SWITCH_PROFILE_RIGHT_JOYCON;
      }
      return true;
    case SB_CONTROLLER_MODE_PRO_CONTROLLER:
      if (profile != NULL) {
        *profile = SWITCH_PROFILE_PRO_CONTROLLER;
      }
      return true;
    default:
      return false;
  }
}

static sb_controller_mode_t switch_controller_mode(void) {
  return (sb_controller_mode_t)s_controller_profile;
}

static bool bd_addr_is_zero(const uint8_t bd_addr[SWITCH_BD_ADDR_LEN]) {
  if (bd_addr == NULL) {
    return true;
  }

  for (size_t i = 0; i < SWITCH_BD_ADDR_LEN; ++i) {
    if (bd_addr[i] != 0u) {
      return false;
    }
  }
  return true;
}

static void debug_dump_pair_slot(const char *stage,
                                 switch_controller_profile_t mode,
                                 const nina_mode_cfg_t *cfg,
                                 const nina_pair_slot_t *slot) {
  const char *stage_name = stage != NULL ? stage : "unknown";

  if (cfg == NULL || slot == NULL) {
    ESP_LOGE(TAG,
             "[%s] pair slot dump unavailable cfg=%p slot=%p",
             stage_name,
             (void *)cfg,
             (void *)slot);
    return;
  }

  ESP_LOGI(TAG, "[%s] mode=%u", stage_name, (unsigned int)mode);
  ESP_LOGI(TAG,
           "[%s] app_ns=%s bond_path=%s",
           stage_name,
           cfg->app_nvs_ns,
           cfg->bt_bond_path);
  ESP_LOGI(TAG,
           "[%s] slot.magic=0x%08" PRIX32 " version=%u slot.mode=%u",
           stage_name,
           slot->magic,
           slot->version,
           slot->mode);
  ESP_LOGI(TAG,
           "[%s] has_local_mac=%u local_bt_mac=" BDSTR,
           stage_name,
           slot->has_local_mac,
           BDARG(slot->local_bt_mac));
  ESP_LOGI(TAG,
           "[%s] has_host_addr=%u switch_bd_addr=" BDSTR,
           stage_name,
           slot->has_host_addr,
           BDARG(slot->switch_bd_addr));
}

static const nina_mode_cfg_t *nina_mode_cfg_for_profile(switch_controller_profile_t profile) {
  if ((uint8_t)profile >= NINA_MODE_COUNT) {
    return NULL;
  }
  return &kNinaModeCfgs[(uint8_t)profile];
}

static const nina_mode_cfg_t *active_mode_cfg(void) {
  return nina_mode_cfg_for_profile(s_controller_profile);
}

static uint32_t nina_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;

  if (data == NULL && len != 0u) {
    return 0u;
  }

  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8u; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (0xEDB88320u & mask);
    }
  }

  return ~crc;
}

static void nina_pair_slot_init_empty(switch_controller_profile_t mode, nina_pair_slot_t *slot) {
  if (slot == NULL) {
    return;
  }

  memset(slot, 0, sizeof(*slot));
  slot->magic = NINA_PAIR_SLOT_MAGIC;
  slot->version = NINA_PAIR_SLOT_VERSION;
  slot->mode = (uint8_t)mode;
  slot->crc32 = nina_crc32((const uint8_t *)slot, offsetof(nina_pair_slot_t, crc32));
}

static uint32_t nina_pair_slot_compute_crc(const nina_pair_slot_t *slot) {
  if (slot == NULL) {
    return 0u;
  }
  return nina_crc32((const uint8_t *)slot, offsetof(nina_pair_slot_t, crc32));
}

static bool nina_pair_slot_valid(const nina_pair_slot_t *slot,
                                 switch_controller_profile_t expected_mode) {
  if (slot == NULL) {
    return false;
  }
  if (slot->magic != NINA_PAIR_SLOT_MAGIC ||
      slot->version != NINA_PAIR_SLOT_VERSION ||
      slot->mode != (uint8_t)expected_mode ||
      slot->crc32 != nina_pair_slot_compute_crc(slot)) {
    return false;
  }
  if (slot->has_local_mac > 1u || slot->has_host_addr > 1u) {
    return false;
  }
  if (slot->has_local_mac != 0u && bd_addr_is_zero(slot->local_bt_mac)) {
    return false;
  }
  if (slot->has_host_addr != 0u && bd_addr_is_zero(slot->switch_bd_addr)) {
    return false;
  }
  return true;
}

static esp_err_t nina_pair_slot_load(switch_controller_profile_t mode, nina_pair_slot_t *out_slot) {
  const nina_mode_cfg_t *cfg = nina_mode_cfg_for_profile(mode);
  nvs_handle_t handle = 0;
  esp_err_t err = ESP_OK;
  nina_pair_slot_t loaded;
  size_t len = sizeof(loaded);

  if (cfg == NULL || out_slot == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  nina_pair_slot_init_empty(mode, out_slot);
  memset(&loaded, 0, sizeof(loaded));

  err = nvs_open(cfg->app_nvs_ns, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_get_blob(handle, kSwitchModeNvsKeySlot, &loaded, &len);
  nvs_close(handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }
  if (len != sizeof(loaded) || !nina_pair_slot_valid(&loaded, mode)) {
    ESP_LOGW(TAG,
             "[NINA_BT] ignoring invalid pair slot for mode=%u namespace=%s",
             (unsigned int)mode,
             cfg->app_nvs_ns);
    return ESP_OK;
  }

  *out_slot = loaded;
  return ESP_OK;
}

static esp_err_t nina_pair_slot_save(switch_controller_profile_t mode,
                                     const nina_pair_slot_t *slot) {
  const nina_mode_cfg_t *cfg = nina_mode_cfg_for_profile(mode);
  nvs_handle_t handle = 0;
  esp_err_t err = ESP_OK;
  nina_pair_slot_t stored;

  if (cfg == NULL || slot == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  stored = *slot;
  stored.magic = NINA_PAIR_SLOT_MAGIC;
  stored.version = NINA_PAIR_SLOT_VERSION;
  stored.mode = (uint8_t)mode;
  if (stored.has_local_mac == 0u) {
    memset(stored.local_bt_mac, 0, sizeof(stored.local_bt_mac));
  }
  if (stored.has_host_addr == 0u) {
    memset(stored.switch_bd_addr, 0, sizeof(stored.switch_bd_addr));
  }
  stored.crc32 = nina_pair_slot_compute_crc(&stored);

  err = nvs_open(cfg->app_nvs_ns, NVS_READWRITE, &handle);
  if (err == ESP_OK) {
    err = nvs_set_blob(handle, kSwitchModeNvsKeySlot, &stored, sizeof(stored));
  }
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  if (handle != 0) {
    nvs_close(handle);
  }
  if (err == ESP_OK && mode == s_controller_profile) {
    s_active_pair_slot = stored;
  }
  return err;
}

static esp_err_t nina_pair_slot_erase(switch_controller_profile_t mode) {
  const nina_mode_cfg_t *cfg = nina_mode_cfg_for_profile(mode);
  nvs_handle_t handle = 0;
  esp_err_t err = ESP_OK;

  if (cfg == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  err = nvs_open(cfg->app_nvs_ns, NVS_READWRITE, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    err = ESP_OK;
  }
  if (err == ESP_OK && handle != 0) {
    err = nvs_erase_key(handle, kSwitchModeNvsKeySlot);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      err = ESP_OK;
    }
  }
  if (err == ESP_OK && handle != 0) {
    err = nvs_commit(handle);
  }
  if (handle != 0) {
    nvs_close(handle);
  }
  if (err == ESP_OK && mode == s_controller_profile) {
    nina_pair_slot_init_empty(mode, &s_active_pair_slot);
  }
  return err;
}

static void apply_saved_host_from_active_slot(void) {
  if (s_active_pair_slot.has_host_addr != 0u &&
      !bd_addr_is_zero(s_active_pair_slot.switch_bd_addr)) {
    memcpy(s_saved_switch_host_bdaddr,
           s_active_pair_slot.switch_bd_addr,
           sizeof(s_saved_switch_host_bdaddr));
    s_saved_switch_host_valid = true;
  } else {
    memset(s_saved_switch_host_bdaddr, 0, sizeof(s_saved_switch_host_bdaddr));
    s_saved_switch_host_valid = false;
  }
}

static esp_err_t load_active_pair_slot_from_nvs(const char *source) {
  esp_err_t err = nina_pair_slot_load(s_controller_profile, &s_active_pair_slot);

  if (err != ESP_OK) {
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_LOAD_SETTINGS,
                 (uint8_t)err,
                 (uint8_t)switch_controller_mode());
    return err;
  }

  apply_saved_host_from_active_slot();
  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_LOAD_SETTINGS,
               0u,
               s_saved_switch_host_valid ? 1u : 0u);
  ESP_LOGI(TAG,
           "[NINA_BT] loaded slot from %s: mode=%u namespace=%s saved_host=%s "
           "%02X:%02X:%02X:%02X:%02X:%02X local_mac=%s "
           "%02X:%02X:%02X:%02X:%02X:%02X",
           source != NULL ? source : "unknown",
           (unsigned int)switch_controller_mode(),
           active_mode_cfg() != NULL ? active_mode_cfg()->app_nvs_ns : "?",
           s_saved_switch_host_valid ? "yes" : "no",
           s_saved_switch_host_bdaddr[0],
           s_saved_switch_host_bdaddr[1],
           s_saved_switch_host_bdaddr[2],
           s_saved_switch_host_bdaddr[3],
           s_saved_switch_host_bdaddr[4],
           s_saved_switch_host_bdaddr[5],
           s_active_pair_slot.has_local_mac != 0u ? "yes" : "no",
           s_active_pair_slot.local_bt_mac[0],
           s_active_pair_slot.local_bt_mac[1],
           s_active_pair_slot.local_bt_mac[2],
           s_active_pair_slot.local_bt_mac[3],
           s_active_pair_slot.local_bt_mac[4],
           s_active_pair_slot.local_bt_mac[5]);
  return ESP_OK;
}

static esp_err_t save_controller_mode_to_nvs(sb_controller_mode_t mode) {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kSwitchGlobalNvsNamespace, NVS_READWRITE, &handle);

  if (err == ESP_OK) {
    err = nvs_set_u8(handle, kSwitchGlobalNvsKeyControllerMode, (uint8_t)mode);
  }
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  if (handle != 0) {
    nvs_close(handle);
  }

  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_SAVE_MODE,
               (uint8_t)err,
               (uint8_t)mode);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "failed to persist controller mode=%u err=0x%X", (unsigned int)mode, err);
  }
  return err;
}

static esp_err_t save_switch_host_to_nvs(const uint8_t bd_addr[SWITCH_BD_ADDR_LEN],
                                         const char *source) {
  esp_err_t err = ESP_OK;

  if (bd_addr_is_zero(bd_addr)) {
    return ESP_ERR_INVALID_ARG;
  }

  memcpy(s_saved_switch_host_bdaddr, bd_addr, SWITCH_BD_ADDR_LEN);
  s_saved_switch_host_valid = true;

  s_active_pair_slot.has_host_addr = 1u;
  memcpy(s_active_pair_slot.switch_bd_addr, bd_addr, sizeof(s_active_pair_slot.switch_bd_addr));
  err = nina_pair_slot_save(s_controller_profile, &s_active_pair_slot);

  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_SAVE_HOST,
               (uint8_t)err,
               (uint8_t)switch_controller_mode());
  if (err == ESP_OK) {
    ESP_LOGI(TAG,
             "saved Switch host from %s: %02X:%02X:%02X:%02X:%02X:%02X profile=%u",
             source != NULL ? source : "unknown",
             bd_addr[0],
             bd_addr[1],
             bd_addr[2],
             bd_addr[3],
             bd_addr[4],
             bd_addr[5],
             (unsigned int)switch_controller_mode());
  } else {
    ESP_LOGW(TAG,
             "failed to persist Switch host from %s err=0x%X",
             source != NULL ? source : "unknown",
             err);
  }
  return err;
}

static esp_err_t clear_saved_switch_host_from_nvs(const char *source) {
  esp_err_t err = ESP_OK;

  memset(s_saved_switch_host_bdaddr, 0, sizeof(s_saved_switch_host_bdaddr));
  s_saved_switch_host_valid = false;

  s_active_pair_slot.has_host_addr = 0u;
  memset(s_active_pair_slot.switch_bd_addr, 0, sizeof(s_active_pair_slot.switch_bd_addr));
  err = nina_pair_slot_save(s_controller_profile, &s_active_pair_slot);

  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_CLEAR_SAVED_HOST,
               (uint8_t)err,
               0u);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "cleared saved Switch host from %s", source != NULL ? source : "unknown");
  } else {
    ESP_LOGW(TAG,
             "failed to clear saved Switch host from %s err=0x%X",
             source != NULL ? source : "unknown",
             err);
  }
  return err;
}

static esp_err_t verify_local_bdaddr_matches_slot(const char *source) {
  const uint8_t *address = esp_bt_dev_get_address();
  esp_err_t err = ESP_OK;

  if (bd_addr_is_zero(address)) {
    return ESP_ERR_INVALID_STATE;
  }

  if (s_active_pair_slot.has_local_mac == 0u ||
      bd_addr_is_zero(s_active_pair_slot.local_bt_mac)) {
    ESP_LOGE(TAG,
             "[NINA_BT] cannot verify local BT address from %s because active slot has no "
             "mode-specific MAC",
             source != NULL ? source : "unknown");
    return ESP_ERR_INVALID_STATE;
  }

  if (memcmp(address, s_active_pair_slot.local_bt_mac, SWITCH_BD_ADDR_LEN) != 0) {
    err = ESP_ERR_INVALID_STATE;
  }

  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_SAVE_LOCAL_ADDR,
               (uint8_t)err,
               0u);
  if (err == ESP_OK) {
    ESP_LOGI(TAG,
             "verified local BT address from %s: " BDSTR,
             source != NULL ? source : "unknown",
             BDARG(address));
  } else {
    ESP_LOGE(TAG,
             "[NINA_BT] local BT address mismatch from %s: actual=" BDSTR
             " expected_slot=" BDSTR,
             source != NULL ? source : "unknown",
             BDARG(address),
             BDARG(s_active_pair_slot.local_bt_mac));
  }
  return err;
}

static esp_err_t load_settings_from_nvs(void) {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kSwitchGlobalNvsNamespace, NVS_READONLY, &handle);
  uint8_t saved_mode = (uint8_t)SWITCH_PROFILE_LEFT_JOYCON;

  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_LOAD_SETTINGS,
                 (uint8_t)err,
                 0u);
    return err;
  }

  if (err == ESP_OK) {
    err = nvs_get_u8(handle, kSwitchGlobalNvsKeyControllerMode, &saved_mode);
    nvs_close(handle);
  }

  if (err == ESP_OK) {
    switch_controller_profile_t saved_profile = SWITCH_PROFILE_LEFT_JOYCON;
    if (switch_profile_from_mode((sb_controller_mode_t)saved_mode, &saved_profile)) {
      s_controller_profile = saved_profile;
    }
  } else if (err != ESP_ERR_NVS_NOT_FOUND) {
    return err;
  }

  return load_active_pair_slot_from_nvs("settings");
}

static const char *switch_controller_name(void) {
  const nina_mode_cfg_t *cfg = active_mode_cfg();
  if (cfg != NULL && cfg->device_name != NULL) {
    return cfg->device_name;
  }
  return "Joy-Con (L)";
}

static uint8_t switch_controller_type(void) {
  switch (s_controller_profile) {
    case SWITCH_PROFILE_RIGHT_JOYCON:
      return SWITCH_CONTROLLER_TYPE_RIGHT_JOYCON;
    case SWITCH_PROFILE_PRO_CONTROLLER:
      return SWITCH_CONTROLLER_TYPE_PRO;
    case SWITCH_PROFILE_LEFT_JOYCON:
    default:
      return SWITCH_CONTROLLER_TYPE_LEFT_JOYCON;
  }
}

static bool switch_profile_has_left_side(void) {
  return s_controller_profile != SWITCH_PROFILE_RIGHT_JOYCON;
}

static bool switch_profile_has_right_side(void) {
  return s_controller_profile != SWITCH_PROFILE_LEFT_JOYCON;
}

static void sync_mode_status(void) {
  s_status.controller_mode = (uint8_t)switch_controller_mode();
  s_status.bluetooth_enabled = s_bluetooth_enabled ? 1u : 0u;
  s_status.reserved0 = (uint8_t)s_bt_state;
  if (s_bluetooth_enabled) {
    s_status.flags |= SB_STATUS_FLAG_BT_POWERED;
  } else {
    s_status.flags &= (uint8_t)~(SB_STATUS_FLAG_BT_POWERED |
                                SB_STATUS_FLAG_BT_READY |
                                SB_STATUS_FLAG_HID_READY |
                                SB_STATUS_FLAG_CONNECTED |
                                SB_STATUS_FLAG_VIRTUAL_CABLE);
  }
}

static void log_bt_identity(const char *stage) {
  uint8_t bt_address[6] = {0};
  uint8_t base_mac[6] = {0};
  esp_bt_cod_t cod = {0};
  uint8_t identity_flags = 0u;
  const nina_mode_cfg_t *cfg = active_mode_cfg();
  const uint8_t *address = esp_bt_dev_get_address();
  const esp_bluedroid_status_t bluedroid_status = esp_bluedroid_get_status();
  esp_err_t cod_err = ESP_ERR_INVALID_STATE;
  const uint16_t descriptor_len =
      (uint16_t)(cfg != NULL ? cfg->hid_descriptor_len : kSwitchJoyConApp.desc_list_len);
  int bond_count = -1;

  if (address != NULL) {
    memcpy(bt_address, address, sizeof(bt_address));
    identity_flags |= 0x01u;
    if ((bt_address[0] & 0x03u) == 0x02u) {
      identity_flags |= 0x08u;
    }
  }

  if (s_intended_base_mac_valid) {
    memcpy(base_mac, s_intended_base_mac, sizeof(base_mac));
    identity_flags |= 0x02u;
    if ((base_mac[0] & 0x03u) == 0x02u) {
      identity_flags |= 0x10u;
    }
  }

  if (bluedroid_status == ESP_BLUEDROID_STATUS_ENABLED) {
    cod_err = esp_bt_gap_get_cod(&cod);
    if (cod_err == ESP_OK) {
      identity_flags |= 0x04u;
    }
    bond_count = esp_bt_gap_get_bond_device_num();
  }

  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_STAGE,
               bt_identity_stage_id(stage),
               identity_flags);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BT_ADDR_0_1,
               bt_address[0],
               bt_address[1]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BT_ADDR_2_3,
               bt_address[2],
               bt_address[3]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BT_ADDR_4_5,
               bt_address[4],
               bt_address[5]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BASE_MAC_0_1,
               base_mac[0],
               base_mac[1]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BASE_MAC_2_3,
               base_mac[2],
               base_mac[3]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_BASE_MAC_4_5,
               base_mac[4],
               base_mac[5]);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_COD_STATUS,
               (uint8_t)cod_err,
               (uint8_t)bluedroid_status);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_COD_MAJOR_MINOR,
               (uint8_t)cod.major,
               (uint8_t)cod.minor);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_COD_SERVICE,
               (uint8_t)(cod.service & 0xFFu),
               (uint8_t)((cod.service >> 8) & 0xFFu));
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_HID_SUBCLASS_DESC_LO,
               (uint8_t)kSwitchJoyConApp.subclass,
               (uint8_t)(descriptor_len & 0xFFu));
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_HID_DESC_HI_NAME_LEN,
               (uint8_t)((descriptor_len >> 8) & 0xFFu),
               clamp_size_to_u8(strlen(switch_controller_name())));
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_HID_STRING_LENGTHS,
               clamp_size_to_u8(strlen(kSwitchJoyConApp.name)),
               clamp_size_to_u8(strlen(kSwitchJoyConApp.description)));
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_HID_PROVIDER_LENGTH,
               clamp_size_to_u8(strlen(kSwitchJoyConApp.provider)),
               0u);

  ESP_LOGI(TAG,
           "BT_IDENTITY stage=%s bt_addr=%02X:%02X:%02X:%02X:%02X:%02X "
           "intended_bt_mac=%02X:%02X:%02X:%02X:%02X:%02X "
           "profile=%u controller_type=0x%02X gap_name=\"%s\" "
           "bond_count=%d saved_host_valid=%u saved_host=%02X:%02X:%02X:%02X:%02X:%02X "
           "cod_err=0x%02X cod_major=0x%02X cod_minor=0x%02X "
           "cod_service=0x%03X hid_service=\"%s\" description=\"%s\" provider=\"%s\" "
           "hid_subclass=0x%02X descriptor_len=%u",
           stage != NULL ? stage : "unknown",
           bt_address[0],
           bt_address[1],
           bt_address[2],
           bt_address[3],
           bt_address[4],
           bt_address[5],
           base_mac[0],
           base_mac[1],
           base_mac[2],
           base_mac[3],
           base_mac[4],
           base_mac[5],
           (unsigned int)switch_controller_mode(),
           (unsigned int)switch_controller_type(),
           switch_controller_name(),
           bond_count,
           s_saved_switch_host_valid ? 1u : 0u,
           s_saved_switch_host_bdaddr[0],
           s_saved_switch_host_bdaddr[1],
           s_saved_switch_host_bdaddr[2],
           s_saved_switch_host_bdaddr[3],
           s_saved_switch_host_bdaddr[4],
           s_saved_switch_host_bdaddr[5],
           (unsigned int)cod_err,
           (unsigned int)cod.major,
           (unsigned int)cod.minor,
           (unsigned int)cod.service,
           kSwitchJoyConApp.name,
           kSwitchJoyConApp.description,
           kSwitchJoyConApp.provider,
           (unsigned int)kSwitchJoyConApp.subclass,
           (unsigned int)descriptor_len);
}

static esp_err_t debug_dump_actual_bt_addr(const char *stage,
                                           const uint8_t expected[SWITCH_BD_ADDR_LEN]) {
  const char *stage_name = stage != NULL ? stage : "unknown";
  const uint8_t *actual = esp_bt_dev_get_address();
  uint8_t expected_copy[SWITCH_BD_ADDR_LEN] = {0};

  if (actual == NULL || bd_addr_is_zero(actual)) {
    ESP_LOGE(TAG, "[%s] actual BT address unavailable", stage_name);
    return ESP_ERR_INVALID_STATE;
  }

  if (expected != NULL) {
    memcpy(expected_copy, expected, sizeof(expected_copy));
  }

  ESP_LOGI(TAG,
           "[%s] actual_bt_addr=" BDSTR " expected_local_bt_mac=" BDSTR,
           stage_name,
           BDARG(actual),
           BDARG(expected_copy));

  if (expected == NULL || bd_addr_is_zero(expected_copy) ||
      memcmp(actual, expected_copy, SWITCH_BD_ADDR_LEN) != 0) {
    ESP_LOGE(TAG,
             "[%s] BT MAC mismatch: per-mode Bluetooth identity is NOT active",
             stage_name);
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

static void debug_dump_bond_list(const char *stage) {
  const char *stage_name = stage != NULL ? stage : "unknown";
  esp_bd_addr_t devices[SWITCH_MAX_BOND_DEVICES];
  int device_count = 0;
  esp_err_t err = ESP_OK;

  if (!s_bluetooth_enabled || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    ESP_LOGW(TAG, "[%s] bond list unavailable because Bluedroid is not enabled", stage_name);
    return;
  }

  device_count = esp_bt_gap_get_bond_device_num();
  ESP_LOGI(TAG, "[%s] bond_count=%d", stage_name, device_count);

  if (device_count <= 0) {
    return;
  }

  if (device_count > SWITCH_MAX_BOND_DEVICES) {
    ESP_LOGW(TAG,
             "[%s] bond_count=%d exceeds dump cap=%d",
             stage_name,
             device_count,
             SWITCH_MAX_BOND_DEVICES);
    device_count = SWITCH_MAX_BOND_DEVICES;
  }

  memset(devices, 0, sizeof(devices));
  err = esp_bt_gap_get_bond_device_list(&device_count, devices);
  ESP_LOGI(TAG,
           "[%s] esp_bt_gap_get_bond_device_list -> %s actual_count=%d",
           stage_name,
           esp_err_to_name(err),
           device_count);

  if (err != ESP_OK) {
    return;
  }

  for (int i = 0; i < device_count; ++i) {
    ESP_LOGI(TAG, "[%s] bond[%d]=" BDSTR, stage_name, i, BDARG(devices[i]));
  }
}

static void delayed_bt_identity_log_task(void *arg) {
  (void)arg;
  vTaskDelay(pdMS_TO_TICKS(500));
  log_bt_identity("after_gap_identity_settled");
  vTaskDelete(NULL);
}

static void schedule_delayed_bt_identity_log(void) {
  (void)xTaskCreate(delayed_bt_identity_log_task,
                    "bt_id_log",
                    2048,
                    NULL,
                    tskIDLE_PRIORITY + 1u,
                    NULL);
}

static bool nina_bt_is_bonded_to_host(const uint8_t bd_addr[SWITCH_BD_ADDR_LEN]) {
  esp_bd_addr_t devices[SWITCH_MAX_BOND_DEVICES];
  int device_count = 0;
  esp_err_t err = ESP_OK;

  if (bd_addr_is_zero(bd_addr) ||
      !s_bluetooth_enabled ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    return false;
  }

  device_count = esp_bt_gap_get_bond_device_num();
  if (device_count <= 0) {
    return false;
  }
  if (device_count > SWITCH_MAX_BOND_DEVICES) {
    device_count = SWITCH_MAX_BOND_DEVICES;
  }

  memset(devices, 0, sizeof(devices));
  err = esp_bt_gap_get_bond_device_list(&device_count, devices);
  if (err != ESP_OK) {
    return false;
  }

  for (int i = 0; i < device_count; ++i) {
    if (memcmp(devices[i], bd_addr, SWITCH_BD_ADDR_LEN) == 0) {
      return true;
    }
  }
  return false;
}

static esp_err_t nina_bt_enter_pairing_mode(void) {
  esp_err_t err = ESP_OK;

  if (!s_bluetooth_enabled || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_STATE;
    return ESP_ERR_INVALID_STATE;
  }

  err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  if (err == ESP_OK) {
    s_bt_state = SWITCH_BT_STATE_PAIRABLE;
    sync_mode_status();
    ESP_LOGI(TAG, "[NINA_BT] entering pairing mode");
  } else {
    s_status.last_error = (uint8_t)err;
  }
  return err;
}

static void configure_gap_identity(void) {
  esp_bt_cod_t cod = {
      .major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL,
      .minor = kPeripheralMinorClassGamepad,
      .service = ESP_BT_COD_SRVC_LMTD_DISCOVER,
      .reserved_2 = 0,
      .reserved_8 = 0,
  };
  esp_err_t name_err = ESP_OK;
  esp_err_t cod_err = ESP_OK;
  esp_err_t scan_err = ESP_OK;
  const bool saved_host_bonded =
      s_saved_switch_host_valid && nina_bt_is_bonded_to_host(s_saved_switch_host_bdaddr);

  name_err = esp_bt_dev_set_device_name(switch_controller_name());
  cod_err = esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
  scan_err = esp_bt_gap_set_scan_mode(
      ESP_BT_CONNECTABLE,
      saved_host_bonded ? ESP_BT_NON_DISCOVERABLE : ESP_BT_GENERAL_DISCOVERABLE);
  if (scan_err == ESP_OK && !saved_host_bonded &&
      (s_status.flags & SB_STATUS_FLAG_CONNECTED) == 0u) {
    s_bt_state = SWITCH_BT_STATE_PAIRABLE;
    sync_mode_status();
  }
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_GAP_IDENTITY_API_0,
               (uint8_t)name_err,
               (uint8_t)cod_err);
  record_event(SB_EVENT_SOURCE_BT_IDENTITY,
               SWITCH_BT_IDENTITY_EVENT_GAP_IDENTITY_API_1,
               (uint8_t)scan_err,
               0u);
  log_bt_identity("after_gap_identity");
  ESP_LOGI(TAG,
           "[NINA_BT] saved host: %s bonded: %s",
           s_saved_switch_host_valid ? "yes" : "no",
           saved_host_bonded ? "yes" : "no");
  schedule_delayed_bt_identity_log();
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
  } else if (conn_status != (uint8_t)ESP_HIDD_CONN_STATE_CONNECTED) {
    s_status.last_error = conn_status;
  }
  if (status == SWITCH_HID_STATUS_SUCCESS &&
      conn_status == (uint8_t)ESP_HIDD_CONN_STATE_CONNECTED) {
    s_status.flags |= SB_STATUS_FLAG_CONNECTED | SB_STATUS_FLAG_VIRTUAL_CABLE;
    s_bt_state = SWITCH_BT_STATE_CONNECTED;
    sync_mode_status();
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
  if (!s_bluetooth_stopping) {
    s_bt_state = SWITCH_BT_STATE_PAIRABLE;
    sync_mode_status();
  }
}

static esp_err_t send_hid_report(esp_hidd_report_type_t report_type,
                                 uint8_t report_id,
                                 size_t report_len,
                                 const uint8_t *report_data) {
  static bool logged_full_report_shape = false;
  static bool logged_reply_report_shape = false;

  if (report_data == NULL || report_len == 0u) {
    return ESP_ERR_INVALID_ARG;
  }

  if (report_id == SWITCH_REPORT_STANDARD_FULL) {
    if (!logged_full_report_shape) {
      ESP_LOGI(TAG,
               "HID_REPORT_SHAPE report_id=0x%02X len=%u expected=%u hidp_prefix=none "
               "profile=%u",
               report_id,
               (unsigned int)report_len,
               (unsigned int)SWITCH_STANDARD_REPORT_BYTES,
               (unsigned int)switch_controller_mode());
      logged_full_report_shape = true;
    }
    if (report_len != SWITCH_STANDARD_REPORT_BYTES) {
      ESP_LOGW(TAG,
               "unexpected 0x30 payload length: got=%u expected=%u",
               (unsigned int)report_len,
               (unsigned int)SWITCH_STANDARD_REPORT_BYTES);
    }
  } else if (report_id == SWITCH_REPORT_SUBCOMMAND_REPLY) {
    if (!logged_reply_report_shape) {
      ESP_LOGI(TAG,
               "HID_REPORT_SHAPE report_id=0x%02X len=%u expected=%u hidp_prefix=none "
               "profile=%u",
               report_id,
               (unsigned int)report_len,
               (unsigned int)SWITCH_SUBCOMMAND_REPLY_BYTES,
               (unsigned int)switch_controller_mode());
      logged_reply_report_shape = true;
    }
    if (report_len != SWITCH_SUBCOMMAND_REPLY_BYTES) {
      ESP_LOGW(TAG,
               "unexpected 0x21 payload length: got=%u expected=%u",
               (unsigned int)report_len,
               (unsigned int)SWITCH_SUBCOMMAND_REPLY_BYTES);
    }
  }

  esp_err_t err = esp_bt_hid_device_send_report(
      report_type, report_id, (uint16_t)report_len, (uint8_t *)report_data);
  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_SEND_REPORT,
               (uint8_t)err,
               report_id);

  s_status.last_hid_event = SWITCH_HID_EVENT_SEND_REPORT;
  s_status.last_hid_report_type = (uint8_t)report_type;
  s_status.last_hid_report_id = report_id;
  if (err != ESP_OK) {
    note_hid_api_error(SWITCH_HID_EVENT_SEND_REPORT, SWITCH_HID_STATUS_ERROR);
  }

  return err;
}

static void nina_identity_derive_mode_mac(const uint8_t base_mac[SWITCH_BD_ADDR_LEN],
                                          switch_controller_profile_t mode,
                                          uint8_t out_mac[SWITCH_BD_ADDR_LEN]) {
  if (base_mac == NULL || out_mac == NULL) {
    return;
  }

  memcpy(out_mac, base_mac, SWITCH_BD_ADDR_LEN);
  out_mac[0] = (uint8_t)((out_mac[0] | 0x02u) & 0xFEu);
  out_mac[5] ^= (uint8_t)(0x40u + (uint8_t)mode);
  if (out_mac[5] < 4u) {
    out_mac[5] = (uint8_t)(4u + (uint8_t)mode);
  }
}

static esp_err_t nina_identity_get_or_create_local_mac(switch_controller_profile_t mode,
                                                       uint8_t out_mac[SWITCH_BD_ADDR_LEN]) {
  uint8_t efuse_mac[6] = {0};
  uint8_t expected_mac[SWITCH_BD_ADDR_LEN] = {0};
  esp_err_t err = ESP_OK;

  if (out_mac == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  err = esp_efuse_mac_get_default(efuse_mac);
  if (err != ESP_OK) {
    return err;
  }

  nina_identity_derive_mode_mac(efuse_mac, mode, expected_mac);
  if (mode == s_controller_profile &&
      s_active_pair_slot.has_local_mac != 0u &&
      !bd_addr_is_zero(s_active_pair_slot.local_bt_mac) &&
      memcmp(s_active_pair_slot.local_bt_mac, expected_mac, SWITCH_BD_ADDR_LEN) != 0) {
    ESP_LOGW(TAG,
             "[NINA_BT] replacing stale local BT MAC for mode=%u stored=" BDSTR
             " expected=" BDSTR,
             (unsigned int)mode,
             BDARG(s_active_pair_slot.local_bt_mac),
             BDARG(expected_mac));
  }

  memcpy(out_mac, expected_mac, SWITCH_BD_ADDR_LEN);
  return ESP_OK;
}

static uint8_t bt_mac_base_offset_for_idf_config(void) {
#if defined(CONFIG_ESP32_UNIVERSAL_MAC_ADDRESSES) && CONFIG_ESP32_UNIVERSAL_MAC_ADDRESSES == 2
  return 1u;
#else
  return 2u;
#endif
}

static void subtract_from_mac_last_octets(uint8_t mac[SWITCH_BD_ADDR_LEN], uint8_t value) {
  uint16_t borrow = value;

  for (int i = (int)SWITCH_BD_ADDR_LEN - 1; i >= 0 && borrow != 0u; --i) {
    const uint16_t current = mac[i];
    if (current >= borrow) {
      mac[i] = (uint8_t)(current - borrow);
      borrow = 0u;
    } else {
      mac[i] = (uint8_t)(0x100u + current - borrow);
      borrow = 1u;
    }
  }
}

static esp_err_t set_local_bt_mac_before_controller_init(
    const uint8_t bt_mac[SWITCH_BD_ADDR_LEN]) {
  uint8_t readback[SWITCH_BD_ADDR_LEN] = {0};
  esp_err_t err = ESP_OK;

  if (bd_addr_is_zero(bt_mac)) {
    return ESP_ERR_INVALID_ARG;
  }

#if SWITCH_HAS_IFACE_MAC_ADDR_SET
  err = esp_iface_mac_addr_set(bt_mac, ESP_MAC_BT);
  ESP_LOGI(TAG,
           "[NINA_BT] esp_iface_mac_addr_set(" BDSTR ") -> %s",
           BDARG(bt_mac),
           esp_err_to_name(err));
#else
  uint8_t base_mac[SWITCH_BD_ADDR_LEN];
  memcpy(base_mac, bt_mac, sizeof(base_mac));
  subtract_from_mac_last_octets(base_mac, bt_mac_base_offset_for_idf_config());
  base_mac[0] = (uint8_t)((base_mac[0] | 0x02u) & 0xFEu);
  err = esp_base_mac_addr_set(base_mac);
  ESP_LOGI(TAG,
           "[NINA_BT] esp_base_mac_addr_set(base=" BDSTR " for target BT=" BDSTR
           ") -> %s",
           BDARG(base_mac),
           BDARG(bt_mac),
           esp_err_to_name(err));
#endif

  if (err != ESP_OK) {
    return err;
  }

  err = esp_read_mac(readback, ESP_MAC_BT);
  ESP_LOGI(TAG,
           "[NINA_BT] esp_read_mac(ESP_MAC_BT) -> %s readback=" BDSTR
           " expected=" BDSTR,
           esp_err_to_name(err),
           BDARG(readback),
           BDARG(bt_mac));
  if (err != ESP_OK) {
    return err;
  }
  if (memcmp(readback, bt_mac, SWITCH_BD_ADDR_LEN) != 0) {
    ESP_LOGE(TAG, "[NINA_BT] pre-init BT MAC readback mismatch");
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

static esp_err_t configure_mode_local_bt_mac(void) {
  uint8_t local_mac[SWITCH_BD_ADDR_LEN] = {0};
  esp_err_t err = ESP_OK;

  err = nina_identity_get_or_create_local_mac(s_controller_profile, local_mac);
  if (err != ESP_OK) {
    return err;
  }

  s_active_pair_slot.has_local_mac = 1u;
  memcpy(s_active_pair_slot.local_bt_mac, local_mac, sizeof(s_active_pair_slot.local_bt_mac));
  err = nina_pair_slot_save(s_controller_profile, &s_active_pair_slot);
  if (err != ESP_OK) {
    return err;
  }

  memcpy(s_intended_base_mac, local_mac, sizeof(s_intended_base_mac));
  s_intended_base_mac_valid = true;

  ESP_LOGI(TAG,
           "[NINA_BT] local BT MAC: %02X:%02X:%02X:%02X:%02X:%02X",
           local_mac[0],
           local_mac[1],
           local_mac[2],
           local_mac[3],
           local_mac[4],
           local_mac[5]);
  return set_local_bt_mac_before_controller_init(local_mac);
}

static esp_err_t configure_bluedroid_bond_path(const nina_mode_cfg_t *cfg) {
  if (cfg == NULL || cfg->bt_bond_path == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (strlen(cfg->bt_bond_path) > NINA_NVS_NAMESPACE_MAX_LEN) {
    return ESP_ERR_INVALID_ARG;
  }

#if SWITCH_HAS_BT_CONFIG_FILE_PATH_UPDATE
  esp_err_t err = ESP_OK;
  err = esp_bt_config_file_path_update(cfg->bt_bond_path);
  ESP_LOGI(TAG,
           "[NINA_BT] esp_bt_config_file_path_update(%s) -> %s",
           cfg->bt_bond_path,
           esp_err_to_name(err));
  return err;
#else
  ESP_LOGE(TAG,
           "[NINA_BT] ESP-IDF %s lacks esp_bt_config_file_path_update(); "
           "bond namespace remains shared unless this API is backported",
           IDF_VER);
  return ESP_OK;
#endif
}

static void copy_bt_address_be(uint8_t out[6]) {
  const uint8_t *address = esp_bt_dev_get_address();
  if (s_active_pair_slot.has_local_mac != 0u &&
      !bd_addr_is_zero(s_active_pair_slot.local_bt_mac)) {
    memcpy(out, s_active_pair_slot.local_bt_mac, SWITCH_BD_ADDR_LEN);
    return;
  }
  if (bd_addr_is_zero(address)) {
    memcpy(out, kFallbackAddressBE, sizeof(kFallbackAddressBE));
    return;
  }
  memcpy(out, address, SWITCH_BD_ADDR_LEN);
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
  uint8_t device_type[1] = {switch_controller_type()};
  uint8_t serial_number[sizeof(kSerialNumberTemplate)];
  uint8_t factory_unknown[1] = {0xA0};
  uint8_t color_info_present[1] = {0x01};
  uint8_t shipment_state[1] = {(uint8_t)(s_shipment_low_power ? 0x01u : 0x00u)};

  if (out == NULL || len == 0u) {
    return;
  }

  memset(out, 0xFF, len);

  copy_bt_address_le(&patchram_addr_record[3]);
  memcpy(serial_number, kSerialNumberTemplate, sizeof(serial_number));
  switch (s_controller_profile) {
    case SWITCH_PROFILE_RIGHT_JOYCON:
      serial_number[3] = 'R';
      break;
    case SWITCH_PROFILE_PRO_CONTROLLER:
      serial_number[3] = 'P';
      break;
    case SWITCH_PROFILE_LEFT_JOYCON:
    default:
      serial_number[3] = 'L';
      break;
  }

  overlay_spi_range(address, out, len, 0x0012u, patchram_addr_record, sizeof(patchram_addr_record));
  overlay_spi_range(address, out, len, 0x5000u, shipment_state, sizeof(shipment_state));
  overlay_spi_range(address, out, len, 0x6000u, serial_number, sizeof(serial_number));
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
  if (!s_bluetooth_enabled) {
    s_status.bond_device_count = 0;
    return;
  }

  s_status.bond_device_count = clamp_u8_count(esp_bt_gap_get_bond_device_num());
}

static void note_gap_event(esp_bt_gap_cb_event_t event, uint8_t status, uint8_t reason) {
  s_status.last_gap_event = (uint8_t)event;
  s_status.last_gap_status = status;
  s_status.last_gap_reason = reason;
  refresh_bond_device_count();
}

static int current_bond_device_count(void) {
  if (!s_bluetooth_enabled || esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    return 0;
  }
  return esp_bt_gap_get_bond_device_num();
}

static void log_reconnect_state(const char *source,
                                bool register_in_use,
                                const uint8_t *register_bdaddr) {
  uint8_t local_bdaddr[SWITCH_BD_ADDR_LEN] = {0};
  uint8_t register_addr[SWITCH_BD_ADDR_LEN] = {0};
  const uint8_t *local_address = esp_bt_dev_get_address();
  const int bond_count = current_bond_device_count();

  if (local_address != NULL) {
    memcpy(local_bdaddr, local_address, sizeof(local_bdaddr));
  }
  if (register_bdaddr != NULL) {
    memcpy(register_addr, register_bdaddr, sizeof(register_addr));
  }

  ESP_LOGI(TAG,
           "RECONNECT_STATE source=%s profile=%u local=%02X:%02X:%02X:%02X:%02X:%02X "
           "bond_count=%d saved_host_valid=%u saved_host=%02X:%02X:%02X:%02X:%02X:%02X "
           "register_in_use=%u register_bdaddr=%02X:%02X:%02X:%02X:%02X:%02X",
           source != NULL ? source : "unknown",
           (unsigned int)switch_controller_mode(),
           local_bdaddr[0],
           local_bdaddr[1],
           local_bdaddr[2],
           local_bdaddr[3],
           local_bdaddr[4],
           local_bdaddr[5],
           bond_count,
           s_saved_switch_host_valid ? 1u : 0u,
           s_saved_switch_host_bdaddr[0],
           s_saved_switch_host_bdaddr[1],
           s_saved_switch_host_bdaddr[2],
           s_saved_switch_host_bdaddr[3],
           s_saved_switch_host_bdaddr[4],
           s_saved_switch_host_bdaddr[5],
           register_in_use ? 1u : 0u,
           register_addr[0],
           register_addr[1],
           register_addr[2],
           register_addr[3],
           register_addr[4],
           register_addr[5]);
}

static void record_reconnect_skip(switch_reconnect_skip_reason_t reason, uint8_t arg) {
  record_event(SB_EVENT_SOURCE_HID_API, SWITCH_HID_API_EVENT_RECONNECT_SKIP, reason, arg);
}

static esp_err_t attempt_saved_switch_reconnect(uint8_t attempt_index) {
  esp_bd_addr_t host_bdaddr;
  const int bond_count = current_bond_device_count();
  const bool saved_host_bonded =
      s_saved_switch_host_valid && nina_bt_is_bonded_to_host(s_saved_switch_host_bdaddr);

  refresh_bond_device_count();

  if (!s_bluetooth_enabled) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_BT_DISABLED, attempt_index);
    return ESP_ERR_INVALID_STATE;
  }
  if (s_bluetooth_stopping) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_STOPPING, attempt_index);
    return ESP_ERR_INVALID_STATE;
  }
  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) != 0u) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_ALREADY_CONNECTED, attempt_index);
    return ESP_OK;
  }
  if (!s_saved_switch_host_valid || bd_addr_is_zero(s_saved_switch_host_bdaddr)) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_NO_SAVED_HOST, attempt_index);
    return ESP_ERR_INVALID_STATE;
  }
  if (bond_count <= 0 || !saved_host_bonded) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_NO_BONDS, attempt_index);
    return ESP_ERR_INVALID_STATE;
  }

  memcpy(host_bdaddr, s_saved_switch_host_bdaddr, sizeof(host_bdaddr));
  s_bt_state = SWITCH_BT_STATE_RECONNECTING;
  sync_mode_status();
  ESP_LOGI(TAG,
           "RECONNECT_ATTEMPT attempt=%u profile=%u host=%02X:%02X:%02X:%02X:%02X:%02X bond_count=%d",
           (unsigned int)attempt_index,
           (unsigned int)switch_controller_mode(),
           host_bdaddr[0],
           host_bdaddr[1],
           host_bdaddr[2],
           host_bdaddr[3],
           host_bdaddr[4],
           host_bdaddr[5],
           bond_count);

  const esp_err_t connect_err = esp_bt_hid_device_connect(host_bdaddr);
  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_CONNECT,
               (uint8_t)connect_err,
               attempt_index);
  if (connect_err != ESP_OK) {
    s_status.last_error = (uint8_t)connect_err;
  }
  return connect_err;
}

static void reconnect_task(void *arg) {
  const uint32_t generation = (uint32_t)(uintptr_t)arg;

  for (size_t i = 0; i < (sizeof(kReconnectBackoffMs) / sizeof(kReconnectBackoffMs[0])); ++i) {
    vTaskDelay(pdMS_TO_TICKS(kReconnectBackoffMs[i]));

    if (generation != s_reconnect_generation) {
      record_reconnect_skip(SWITCH_RECONNECT_SKIP_STALE_TASK, (uint8_t)(i + 1u));
      break;
    }
    if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) != 0u) {
      record_reconnect_skip(SWITCH_RECONNECT_SKIP_ALREADY_CONNECTED, (uint8_t)(i + 1u));
      break;
    }

    (void)attempt_saved_switch_reconnect((uint8_t)(i + 1u));
  }

  if (generation == s_reconnect_generation) {
    s_reconnect_task_handle = NULL;
    if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) == 0u) {
      (void)nina_bt_enter_pairing_mode();
    }
  }
  vTaskDelete(NULL);
}

static void schedule_saved_switch_reconnect(const char *source) {
  const int bond_count = current_bond_device_count();
  const bool saved_host_bonded =
      s_saved_switch_host_valid && nina_bt_is_bonded_to_host(s_saved_switch_host_bdaddr);
  TaskHandle_t task_handle = NULL;
  BaseType_t task_started = pdFALSE;

  log_reconnect_state(source, false, NULL);

  if (!s_bluetooth_enabled) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_BT_DISABLED, 0u);
    return;
  }
  if (s_bluetooth_stopping) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_STOPPING, 0u);
    return;
  }
  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) != 0u) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_ALREADY_CONNECTED, 0u);
    return;
  }
  if (!s_saved_switch_host_valid || bd_addr_is_zero(s_saved_switch_host_bdaddr)) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_NO_SAVED_HOST, 0u);
    (void)nina_bt_enter_pairing_mode();
    return;
  }
  if (bond_count <= 0 || !saved_host_bonded) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_NO_BONDS, 0u);
    (void)nina_bt_enter_pairing_mode();
    return;
  }

  s_reconnect_generation++;
  s_bt_state = SWITCH_BT_STATE_RECONNECTING;
  sync_mode_status();
  ESP_LOGI(TAG, "[NINA_BT] entering reconnect");
  task_started = xTaskCreate(reconnect_task,
                             "hid_reconnect",
                             SWITCH_RECONNECT_TASK_STACK,
                             (void *)(uintptr_t)s_reconnect_generation,
                             tskIDLE_PRIORITY + 1u,
                             &task_handle);
  if (task_started != pdPASS) {
    record_reconnect_skip(SWITCH_RECONNECT_SKIP_TASK_CREATE_FAILED, 0u);
    return;
  }

  s_reconnect_task_handle = task_handle;
  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_SCHEDULE_RECONNECT,
               s_saved_switch_host_valid ? 1u : 0u,
               clamp_u8_count(bond_count));
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

static void pack_center_stick(uint8_t out[3]) {
  pack_switch_stick(SWITCH_STICK_CENTER, SWITCH_STICK_CENTER, out);
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

static uint32_t hat_to_profile_dpad_bits(uint8_t hat) {
  if (!switch_profile_has_left_side()) {
    return 0u;
  }
  return hat_to_left_dpad_bits(hat);
}

static void build_common_input_report(uint8_t report[SWITCH_INPUT_COMMON_BYTES]) {
  const bool reports_left = switch_profile_has_left_side();
  const bool reports_right = switch_profile_has_right_side();
  uint32_t buttons = s_state.buttons;
  uint8_t right_buttons = 0;
  uint8_t left_buttons = 0;
  uint8_t shared_buttons = 0;

  if (reports_left) {
    buttons |= hat_to_profile_dpad_bits(s_state.hat);
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
  }

  if (reports_right) {
    if ((buttons & SB_BTN_RJC_Y) != 0u) {
      right_buttons |= 0x01u;
    }
    if ((buttons & SB_BTN_RJC_X) != 0u) {
      right_buttons |= 0x02u;
    }
    if ((buttons & SB_BTN_RJC_B) != 0u) {
      right_buttons |= 0x04u;
    }
    if ((buttons & SB_BTN_RJC_A) != 0u) {
      right_buttons |= 0x08u;
    }
    if ((buttons & SB_BTN_RJC_SR) != 0u) {
      right_buttons |= 0x10u;
    }
    if ((buttons & SB_BTN_RJC_SL) != 0u) {
      right_buttons |= 0x20u;
    }
    if ((buttons & SB_BTN_RJC_R) != 0u) {
      right_buttons |= 0x40u;
    }
    if ((buttons & SB_BTN_RJC_ZR) != 0u) {
      right_buttons |= 0x80u;
    }

    if ((buttons & SB_BTN_RJC_PLUS) != 0u) {
      shared_buttons |= 0x02u;
    }
    if ((buttons & SB_BTN_RJC_STICK) != 0u) {
      shared_buttons |= 0x04u;
    }
    if ((buttons & SB_BTN_RJC_HOME) != 0u) {
      shared_buttons |= 0x10u;
    }
  }

  if ((s_state.misc & SB_MISC_CHARGING_GRIP) != 0u) {
    shared_buttons |= 0x80u;
  }

  memset(report, 0, SWITCH_INPUT_COMMON_BYTES);
  report[0] = s_report_timer++;
  report[1] = build_battery_and_connection();
  report[2] = right_buttons;
  report[3] = shared_buttons;
  report[4] = left_buttons;
  if (reports_left) {
    pack_switch_stick(scale_axis_u12(s_state.lx), scale_axis_u12(s_state.ly), &report[5]);
  } else {
    pack_center_stick(&report[5]);
  }
  if (reports_right) {
    pack_switch_stick(scale_axis_u12(s_state.rx), scale_axis_u12(s_state.ry), &report[8]);
  } else {
    pack_center_stick(&report[8]);
  }
  report[11] = 0x80u;

  {
    static bool logged_common_report = false;
    static uint8_t last_common_payload[SWITCH_INPUT_COMMON_BYTES - 1u] = {0};
    if (!logged_common_report ||
        memcmp(last_common_payload, &report[1], sizeof(last_common_payload)) != 0) {
      ESP_LOGD(TAG,
               "COMMON_REPORT profile=%u type=0x%02X mode=0x%02X bytes=%02X %02X %02X %02X "
               "%02X %02X %02X %02X %02X %02X %02X %02X",
               (unsigned int)switch_controller_mode(),
               (unsigned int)switch_controller_type(),
               (unsigned int)s_status.input_report_mode,
               report[0],
               report[1],
               report[2],
               report[3],
               report[4],
               report[5],
               report[6],
               report[7],
               report[8],
               report[9],
               report[10],
               report[11]);
      memcpy(last_common_payload, &report[1], sizeof(last_common_payload));
      logged_common_report = true;
    }
  }
}

static void build_simple_input_report(uint8_t report[SWITCH_SIMPLE_REPORT_BYTES]) {
  const bool reports_left = switch_profile_has_left_side();
  const bool reports_right = switch_profile_has_right_side();
  uint32_t buttons = s_state.buttons;
  uint16_t left_x = reports_left ? scale_axis_u16(s_state.lx) : SWITCH_SIMPLE_STICK_CENTER;
  uint16_t left_y = reports_left ? scale_axis_u16(s_state.ly) : SWITCH_SIMPLE_STICK_CENTER;
  uint16_t right_x = reports_right ? scale_axis_u16(s_state.rx) : SWITCH_SIMPLE_STICK_CENTER;
  uint16_t right_y = reports_right ? scale_axis_u16(s_state.ry) : SWITCH_SIMPLE_STICK_CENTER;

  memset(report, 0, SWITCH_SIMPLE_REPORT_BYTES);

  if (reports_left) {
    buttons |= hat_to_profile_dpad_bits(s_state.hat);
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
  }

  if (reports_right) {
    if ((buttons & SB_BTN_RJC_Y) != 0u) {
      report[0] |= 0x01u;
    }
    if ((buttons & SB_BTN_RJC_X) != 0u) {
      report[0] |= 0x02u;
    }
    if ((buttons & SB_BTN_RJC_B) != 0u) {
      report[0] |= 0x04u;
    }
    if ((buttons & SB_BTN_RJC_A) != 0u) {
      report[0] |= 0x08u;
    }
    if ((buttons & SB_BTN_RJC_SL) != 0u) {
      report[0] |= 0x10u;
    }
    if ((buttons & SB_BTN_RJC_SR) != 0u) {
      report[0] |= 0x20u;
    }

    if ((buttons & SB_BTN_RJC_PLUS) != 0u) {
      report[1] |= 0x02u;
    }
    if ((buttons & SB_BTN_RJC_STICK) != 0u) {
      report[1] |= 0x08u;
    }
    if ((buttons & SB_BTN_RJC_HOME) != 0u) {
      report[1] |= 0x10u;
    }
    if ((buttons & SB_BTN_RJC_R) != 0u) {
      report[1] |= 0x40u;
    }
    if ((buttons & SB_BTN_RJC_ZR) != 0u) {
      report[1] |= 0x80u;
    }
  }

  report[2] = reports_left ? (uint8_t)(s_state.hat <= 7u ? s_state.hat : 8u) : 8u;
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

static void build_device_info_reply(uint8_t reply[SWITCH_DEVICE_INFO_REPLY_BYTES]) {
  uint8_t address_be[6];

  copy_bt_address_be(address_be);

  memset(reply, 0, SWITCH_DEVICE_INFO_REPLY_BYTES);
  reply[0] = 0x04u;
  reply[1] = 0x00u;
  reply[2] = switch_controller_type();
  reply[3] = 0x02u;
  memcpy(&reply[4], address_be, sizeof(address_be));
  reply[10] = 0x01u;
  reply[11] = 0x01u;

  ESP_LOGI(TAG,
           "DEVICE_INFO profile=%u controller_type=0x%02X reported_mac=" BDSTR,
           (unsigned int)switch_controller_mode(),
           (unsigned int)reply[2],
           BDARG(address_be));
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

static void build_trigger_elapsed_reply(uint8_t reply[SWITCH_TRIGGER_ELAPSED_REPLY_BYTES]) {
  const uint8_t elapsed_lo = 0x2Cu;
  const uint8_t elapsed_hi = 0x01u;
  memset(reply, 0, SWITCH_TRIGGER_ELAPSED_REPLY_BYTES);

  if (s_controller_profile == SWITCH_PROFILE_PRO_CONTROLLER) {
    reply[0] = elapsed_lo;
    reply[1] = elapsed_hi;
    reply[2] = elapsed_lo;
    reply[3] = elapsed_hi;
    return;
  }

  reply[8] = elapsed_lo;
  reply[9] = elapsed_hi;
  reply[10] = elapsed_lo;
  reply[11] = elapsed_hi;
}

static void build_nfc_ir_mcu_config_reply(
    uint8_t reply[SWITCH_NFC_IR_MCU_CONFIG_REPLY_BYTES]) {
  static const uint8_t kNfcIrMcuConfigReply[SWITCH_NFC_IR_MCU_CONFIG_REPLY_BYTES] = {
      0x01u, 0x00u, 0xFFu, 0x00u, 0x08u, 0x00u, 0x1Bu, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
      0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xC8u,
  };

  memcpy(reply, kNfcIrMcuConfigReply, sizeof(kNfcIrMcuConfigReply));
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
      (void)send_subcommand_reply(
          SWITCH_ACK_DEVICE_INFO, subcommand, reply, SWITCH_DEVICE_INFO_REPLY_BYTES);
      break;

    case SWITCH_SUBCMD_SET_INPUT_REPORT_MODE:
      if (args_len >= 1u) {
        s_status.input_report_mode = args[0];
      }
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
      break;

    case SWITCH_SUBCMD_TRIGGER_BUTTONS_ELAPSED:
      build_trigger_elapsed_reply(reply);
      (void)send_subcommand_reply(
          SWITCH_ACK_TRIGGER_BUTTONS, subcommand, reply, SWITCH_TRIGGER_ELAPSED_REPLY_BYTES);
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

    case SWITCH_SUBCMD_SET_NFC_IR_MCU_CONFIG:
      build_nfc_ir_mcu_config_reply(reply);
      (void)send_subcommand_reply(SWITCH_ACK_NFC_IR_MCU_CONFIG,
                                  subcommand,
                                  reply,
                                  SWITCH_NFC_IR_MCU_CONFIG_REPLY_BYTES);
      break;

    case SWITCH_SUBCMD_SET_NFC_IR_MCU_STATE:
      (void)send_subcommand_reply(SWITCH_ACK_SIMPLE, subcommand, NULL, 0u);
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
    record_event(SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, 0u, 0u);
    note_gap_event(event, 0u, 0u);
    return;
  }

  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      record_event(
          SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, (uint8_t)param->auth_cmpl.stat, 0u);
      note_gap_event(event, (uint8_t)param->auth_cmpl.stat, 0u);
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK,
                   (uint8_t)event,
                   param->pin_req.min_16_digit ? 16u : 4u,
                   0u);
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
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, 1u, 0u);
      note_gap_event(event, 1u, 0u);
      (void)esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, 0u, 0u);
      note_gap_event(event, 0u, 0u);
      break;

    case ESP_BT_GAP_KEY_REQ_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, 0u, 0u);
      note_gap_event(event, 0u, 0u);
      break;
#endif

    case ESP_BT_GAP_CONFIG_EIR_DATA_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->config_eir_data.stat,
                   0u);
      note_gap_event(event, (uint8_t)param->config_eir_data.stat, 0u);
      break;

    case ESP_BT_GAP_MODE_CHG_EVT:
      record_event(
          SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, (uint8_t)param->mode_chg.mode, 0u);
      note_gap_event(event, (uint8_t)param->mode_chg.mode, 0u);
      break;

    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->acl_conn_cmpl_stat.stat,
                   0u);
      note_gap_event(event, (uint8_t)param->acl_conn_cmpl_stat.stat, 0u);
      break;

    case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK,
                   (uint8_t)event,
                   0u,
                   (uint8_t)param->acl_disconn_cmpl_stat.reason);
      note_gap_event(event, 0u, (uint8_t)param->acl_disconn_cmpl_stat.reason);
      break;

    default:
      record_event(SB_EVENT_SOURCE_GAP_CALLBACK, (uint8_t)event, 0u, 0u);
      note_gap_event(event, 0u, 0u);
      break;
  }
}

static esp_hidd_app_param_t *active_hid_app_param(void) {
  const nina_mode_cfg_t *cfg = active_mode_cfg();

  s_active_hid_app = kSwitchJoyConApp;
  if (cfg != NULL && cfg->hid_descriptor != NULL && cfg->hid_descriptor_len > 0u) {
    s_active_hid_app.desc_list = (uint8_t *)cfg->hid_descriptor;
    s_active_hid_app.desc_list_len = (int)cfg->hid_descriptor_len;
  }
  return &s_active_hid_app;
}

static void hid_callback(esp_hidd_cb_event_t event, esp_hidd_cb_param_t *param) {
  s_status.last_hid_event = (uint8_t)event;

  switch (event) {
    case ESP_HIDD_INIT_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK, (uint8_t)event, (uint8_t)param->init.status, 0u);
      s_status.last_hid_status = (uint8_t)param->init.status;
      if (param->init.status == ESP_HIDD_SUCCESS) {
        const esp_err_t register_err = esp_bt_hid_device_register_app(
            active_hid_app_param(),
            (esp_hidd_qos_param_t *)&kQos,
            (esp_hidd_qos_param_t *)&kQos);
        record_event(SB_EVENT_SOURCE_HID_API,
                     SWITCH_HID_API_EVENT_REGISTER_APP,
                     (uint8_t)register_err,
                     0u);
        if (register_err != ESP_OK) {
          s_status.last_error = (uint8_t)register_err;
        }
      } else {
        s_status.last_error = (uint8_t)param->init.status;
      }
      break;

    case ESP_HIDD_REGISTER_APP_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->register_app.status,
                   param->register_app.in_use ? 1u : 0u);
      log_bt_identity("after_register_app_evt");
      s_status.last_hid_status = (uint8_t)param->register_app.status;
      if (param->register_app.status == ESP_HIDD_SUCCESS) {
        refresh_bond_device_count();
        s_status.flags |= SB_STATUS_FLAG_HID_READY | SB_STATUS_FLAG_BT_READY;
        if (param->register_app.in_use) {
          s_status.flags |= SB_STATUS_FLAG_VIRTUAL_CABLE;
          if (save_switch_host_to_nvs(param->register_app.bd_addr, "register_app") != ESP_OK) {
            s_status.last_error = SWITCH_HID_STATUS_ERROR;
          }
        }

        (void)verify_local_bdaddr_matches_slot("register_app");
        log_reconnect_state(
            "register_app", param->register_app.in_use, param->register_app.bd_addr);
        configure_gap_identity();
        schedule_saved_switch_reconnect("register_app");
      } else {
        s_status.last_error = (uint8_t)param->register_app.status;
      }
      break;

    case ESP_HIDD_OPEN_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->open.status,
                   (uint8_t)param->open.conn_status);
      note_hid_connection_open((uint8_t)param->open.status, (uint8_t)param->open.conn_status);
      if (param->open.status == ESP_HIDD_SUCCESS &&
          param->open.conn_status == ESP_HIDD_CONN_STATE_CONNECTED) {
        s_reconnect_generation++;
        if (save_switch_host_to_nvs(param->open.bd_addr, "open") != ESP_OK) {
          s_status.last_error = SWITCH_HID_STATUS_ERROR;
        }
      }
      break;

    case ESP_HIDD_CLOSE_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->close.status,
                   (uint8_t)param->close.conn_status);
      note_hid_connection_close((uint8_t)param->close.status, (uint8_t)param->close.conn_status);
      if (param->close.status == ESP_HIDD_SUCCESS && !s_bluetooth_stopping) {
        schedule_saved_switch_reconnect("close");
      }
      break;

    case ESP_HIDD_SET_PROTOCOL_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->set_protocol.protocol_mode,
                   0u);
      s_status.protocol_mode = (uint8_t)param->set_protocol.protocol_mode;
      break;

    case ESP_HIDD_SET_REPORT_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   param->set_report.report_id,
                   (uint8_t)param->set_report.report_type);
      s_status.last_hid_report_type = (uint8_t)param->set_report.report_type;
      s_status.last_hid_report_id = param->set_report.report_id;
      handle_output_report(
          param->set_report.report_id, param->set_report.data, (size_t)param->set_report.len);
      break;

    case ESP_HIDD_GET_REPORT_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   param->get_report.report_id,
                   (uint8_t)param->get_report.report_type);
      s_status.last_hid_report_type = (uint8_t)param->get_report.report_type;
      s_status.last_hid_report_id = param->get_report.report_id;
      break;

    case ESP_HIDD_INTR_DATA_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   param->intr_data.report_id,
                   (uint8_t)param->intr_data.len);
      s_status.last_hid_report_type = (uint8_t)ESP_HIDD_REPORT_TYPE_INTRDATA;
      s_status.last_hid_report_id = param->intr_data.report_id;
      handle_output_report(
          param->intr_data.report_id, param->intr_data.data, (size_t)param->intr_data.len);
      break;

    case ESP_HIDD_SEND_REPORT_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->send_report.status,
                   param->send_report.report_id);
      s_status.last_hid_status = (uint8_t)param->send_report.status;
      s_status.last_hid_report_type = (uint8_t)param->send_report.report_type;
      s_status.last_hid_report_id = param->send_report.report_id;
      if (param->send_report.status != ESP_HIDD_SUCCESS) {
        s_status.last_error = param->send_report.reason;
      }
      break;

    case ESP_HIDD_REPORT_ERR_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->report_err.status,
                   0u);
      s_status.last_hid_status = (uint8_t)param->report_err.status;
      break;

    case ESP_HIDD_VC_UNPLUG_EVT:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK,
                   (uint8_t)event,
                   (uint8_t)param->vc_unplug.status,
                   (uint8_t)param->vc_unplug.conn_status);
      s_status.last_hid_status = (uint8_t)param->vc_unplug.status;
      s_status.last_hid_conn_status = (uint8_t)param->vc_unplug.conn_status;
      s_status.flags &= (uint8_t)~SB_STATUS_FLAG_VIRTUAL_CABLE;
      s_reconnect_generation++;
      if (param->vc_unplug.status == ESP_HIDD_SUCCESS) {
        (void)clear_saved_switch_host_from_nvs("vc_unplug_evt");
        configure_gap_identity();
      }
      break;

    default:
      record_event(SB_EVENT_SOURCE_HID_CALLBACK, (uint8_t)event, 0u, 0u);
      break;
  }
}

static void keep_first_shutdown_error(esp_err_t err, esp_err_t *first_error) {
  if (first_error == NULL || err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
    return;
  }
  if (*first_error == ESP_OK) {
    *first_error = err;
  }
}

static esp_err_t stop_bluetooth(void) {
  esp_err_t first_error = ESP_OK;

  if (!s_bluetooth_enabled && (s_status.flags & SB_STATUS_FLAG_BT_POWERED) == 0u) {
    s_bluetooth_stopping = false;
    s_bt_state = SWITCH_BT_STATE_OFF;
    sync_mode_status();
    return ESP_OK;
  }

  s_bluetooth_stopping = true;
  s_bt_state = SWITCH_BT_STATE_STOPPING;
  sync_mode_status();
  s_reconnect_generation++;

  record_event(SB_EVENT_SOURCE_BRIDGE,
               SB_MSG_SET_BLUETOOTH_ENABLED,
               0u,
               (uint8_t)switch_controller_mode());

  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) != 0u) {
    esp_err_t err = esp_bt_hid_device_disconnect();
    record_event(SB_EVENT_SOURCE_HID_API, SWITCH_HID_API_EVENT_DISCONNECT, (uint8_t)err, 0u);
    keep_first_shutdown_error(err, &first_error);
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    keep_first_shutdown_error(err, &first_error);

    err = esp_bt_hid_device_deinit();
    keep_first_shutdown_error(err, &first_error);
    vTaskDelay(pdMS_TO_TICKS(50));

    err = esp_bluedroid_disable();
    keep_first_shutdown_error(err, &first_error);
  }

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
    esp_err_t err = esp_bluedroid_deinit();
    keep_first_shutdown_error(err, &first_error);
  }

  {
    const esp_bt_controller_status_t controller_status = esp_bt_controller_get_status();
    if (controller_status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
      esp_err_t err = esp_bt_controller_disable();
      keep_first_shutdown_error(err, &first_error);
    }
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
      esp_err_t err = esp_bt_controller_deinit();
      keep_first_shutdown_error(err, &first_error);
    }
  }

  s_bluetooth_enabled = false;
  s_bluetooth_stopping = false;
  s_bt_state = SWITCH_BT_STATE_OFF;
  s_intended_base_mac_valid = false;
  s_status.protocol_mode = 0;
  s_status.input_report_mode = SWITCH_REPORT_STANDARD_FULL;
  s_status.last_hid_conn_status = 0;
  reset_controller_runtime_state();
  refresh_bond_device_count();
  sync_mode_status();

  if (first_error != ESP_OK) {
    s_status.last_error = (uint8_t)first_error;
  }

  return first_error;
}

static esp_err_t start_bluetooth(void) {
  esp_err_t err = ESP_OK;
  const nina_mode_cfg_t *cfg = active_mode_cfg();

  if (s_bluetooth_enabled) {
    sync_mode_status();
    return ESP_OK;
  }

  if (cfg == NULL) {
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_ARG;
    return ESP_ERR_INVALID_ARG;
  }

  s_bluetooth_stopping = false;
  s_bt_state = SWITCH_BT_STATE_STARTING;
  s_reconnect_generation++;

  record_event(SB_EVENT_SOURCE_BRIDGE,
               SB_MSG_SET_BLUETOOTH_ENABLED,
               1u,
               (uint8_t)switch_controller_mode());

  err = load_active_pair_slot_from_nvs("bt_start");
  if (err != ESP_OK) {
    s_status.last_error = (uint8_t)err;
    s_bt_state = SWITCH_BT_STATE_OFF;
    sync_mode_status();
    return err;
  }

  ESP_LOGI(TAG, "[NINA_BT] selected mode: %s", cfg->device_name);
  ESP_LOGI(TAG, "[NINA_BT] app namespace: %s", cfg->app_nvs_ns);
  ESP_LOGI(TAG, "[NINA_BT] bond path: %s", cfg->bt_bond_path);
  debug_dump_pair_slot("after_slot_load", s_controller_profile, cfg, &s_active_pair_slot);

  err = configure_mode_local_bt_mac();
  if (err != ESP_OK) {
    s_status.last_error = (uint8_t)err;
    s_bt_state = SWITCH_BT_STATE_OFF;
    sync_mode_status();
    return err;
  }
  debug_dump_pair_slot("before_bt_init", s_controller_profile, cfg, &s_active_pair_slot);

  err = configure_bluedroid_bond_path(cfg);
  if (err != ESP_OK) {
    s_status.last_error = (uint8_t)err;
    s_bt_state = SWITCH_BT_STATE_OFF;
    sync_mode_status();
    return err;
  }
  log_bt_identity("after_base_mac");

  s_bluetooth_enabled = true;
  s_status.flags &= (uint8_t)~(SB_STATUS_FLAG_BT_READY |
                              SB_STATUS_FLAG_HID_READY |
                              SB_STATUS_FLAG_CONNECTED |
                              SB_STATUS_FLAG_VIRTUAL_CABLE);
  sync_mode_status();

  {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
  }
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }
  log_bt_identity("after_controller_init");

  err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

  err = esp_bluedroid_enable();
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }
  log_bt_identity("after_bluedroid_enable");
  err = debug_dump_actual_bt_addr("after_bluedroid_enable", s_active_pair_slot.local_bt_mac);
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }
  (void)verify_local_bdaddr_matches_slot("bluedroid_enable");
  debug_dump_bond_list("after_bluedroid_enable");

  err = esp_bt_gap_register_callback(gap_callback);
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

#if (CONFIG_BT_SSP_ENABLED == true)
  {
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    err = esp_bt_gap_set_security_param(param_type, &iocap, sizeof(iocap));
  }
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }
#endif

  {
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code = {0};
    err = esp_bt_gap_set_pin(pin_type, 0u, pin_code);
  }
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

  err = esp_bt_hid_device_register_callback(hid_callback);
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

  err = esp_bt_hid_device_init();
  if (err != ESP_OK) {
    (void)stop_bluetooth();
    s_status.last_error = (uint8_t)err;
    return err;
  }

  refresh_bond_device_count();
  sync_mode_status();
  return ESP_OK;
}

esp_err_t switch_hid_init(void) {
  esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  s_controller_profile = SWITCH_PROFILE_LEFT_JOYCON;
  nina_pair_slot_init_empty(s_controller_profile, &s_active_pair_slot);
  memset(&s_active_hid_app, 0, sizeof(s_active_hid_app));
  s_bluetooth_enabled = false;
  s_bluetooth_stopping = false;
  s_bt_state = SWITCH_BT_STATE_OFF;
  s_intended_base_mac_valid = false;
  memset(s_saved_switch_host_bdaddr, 0, sizeof(s_saved_switch_host_bdaddr));
  s_saved_switch_host_valid = false;
  s_reconnect_generation++;
  s_status.flags = SB_STATUS_FLAG_BRIDGE_READY;
  s_status.protocol_mode = 0;
  s_status.input_report_mode = SWITCH_REPORT_STANDARD_FULL;
  s_status.battery_level = 8;
  s_status.last_hid_conn_status = 0;
  reset_controller_runtime_state();
  err = load_settings_from_nvs();
  if (err != ESP_OK) {
    s_status.last_error = (uint8_t)err;
    ESP_LOGW(TAG, "failed to load persisted settings err=0x%X", err);
  }
  refresh_bond_device_count();
  sync_mode_status();
  return ESP_OK;
}

esp_err_t switch_hid_set_controller_mode(sb_controller_mode_t mode) {
  switch_controller_profile_t profile = SWITCH_PROFILE_LEFT_JOYCON;
  record_event(SB_EVENT_SOURCE_BRIDGE, SB_MSG_SET_CONTROLLER_MODE, (uint8_t)mode, 0u);

  if (!switch_profile_from_mode(mode, &profile)) {
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_ARG;
    return ESP_ERR_INVALID_ARG;
  }

  if (profile == s_controller_profile) {
    esp_err_t save_err = save_controller_mode_to_nvs(mode);
    if (save_err != ESP_OK) {
      s_status.last_error = (uint8_t)save_err;
      return save_err;
    }
    sync_mode_status();
    return ESP_OK;
  }

  if (s_bluetooth_enabled) {
    esp_err_t save_err = save_controller_mode_to_nvs(mode);
    if (save_err != ESP_OK) {
      s_status.last_error = (uint8_t)save_err;
      return save_err;
    }
    ESP_LOGW(TAG,
             "controller profile change requested while Bluetooth is enabled: requested=%u "
             "active=%u; restarting to apply BT MAC, bond path, SDP, and protocol identity",
             (unsigned int)mode,
             (unsigned int)switch_controller_mode());
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
    return ESP_OK;
  }

  s_controller_profile = profile;
  s_intended_base_mac_valid = false;
  s_status.input_report_mode = SWITCH_REPORT_STANDARD_FULL;
  reset_controller_runtime_state();
  {
    esp_err_t load_err = load_active_pair_slot_from_nvs("set_mode");
    if (load_err != ESP_OK) {
      s_status.last_error = (uint8_t)load_err;
      return load_err;
    }
  }
  sync_mode_status();
  {
    esp_err_t save_err = save_controller_mode_to_nvs(mode);
    if (save_err != ESP_OK) {
      s_status.last_error = (uint8_t)save_err;
      return save_err;
    }
  }
  ESP_LOGI(TAG,
           "controller profile set: mode=%u type=0x%02X name=\"%s\"",
           (unsigned int)switch_controller_mode(),
           (unsigned int)switch_controller_type(),
           switch_controller_name());
  return ESP_OK;
}

esp_err_t switch_hid_set_bluetooth_enabled(bool enabled) {
  return enabled ? start_bluetooth() : stop_bluetooth();
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

size_t switch_hid_copy_event_log(sb_event_entry_t *entries,
                                 size_t max_entries,
                                 uint16_t *first_sequence,
                                 bool *overflowed) {
  size_t snapshot_count = 0u;
  size_t start_index = 0u;
  uint16_t start_sequence = 0u;
  bool did_overflow = false;

  portENTER_CRITICAL(&s_event_ring_lock);

  snapshot_count = s_event_ring_count;
  if (snapshot_count > max_entries) {
    snapshot_count = max_entries;
  }
  if (snapshot_count > 0u) {
    start_index = (s_event_ring_head + SWITCH_EVENT_RING_SIZE - snapshot_count) % SWITCH_EVENT_RING_SIZE;
  }
  start_sequence = (uint16_t)(s_next_event_sequence - snapshot_count);
  did_overflow = s_event_ring_overflowed;

  if (entries != NULL) {
    for (size_t i = 0; i < snapshot_count; ++i) {
      const size_t ring_index = (start_index + i) % SWITCH_EVENT_RING_SIZE;
      entries[i] = s_event_ring[ring_index];
    }
  }

  portEXIT_CRITICAL(&s_event_ring_lock);

  if (first_sequence != NULL) {
    *first_sequence = start_sequence;
  }
  if (overflowed != NULL) {
    *overflowed = did_overflow;
  }

  return snapshot_count;
}

void switch_hid_tick(void) {
  uint64_t now = 0;

  if (!s_bluetooth_enabled ||
      (s_status.flags & SB_STATUS_FLAG_CONNECTED) == 0u ||
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

esp_err_t switch_hid_start_pairing_mode(void) {
  esp_err_t err = ESP_OK;

  record_event(SB_EVENT_SOURCE_BRIDGE, SB_MSG_PAIRING_START, 0u, 0u);
  if (!s_bluetooth_enabled) {
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_STATE;
    return ESP_ERR_INVALID_STATE;
  }

  s_reconnect_generation++;
  err = clear_saved_switch_host_from_nvs("pairing_start");
  if (err != ESP_OK) {
    s_status.last_error = (uint8_t)err;
    return err;
  }
  configure_gap_identity();
  return nina_bt_enter_pairing_mode();
}

esp_err_t switch_hid_forget_pairing_current_mode(void) {
  esp_err_t result = ESP_OK;
  uint8_t saved_host[SWITCH_BD_ADDR_LEN] = {0};
  const bool had_saved_host =
      s_saved_switch_host_valid && !bd_addr_is_zero(s_saved_switch_host_bdaddr);

  record_event(SB_EVENT_SOURCE_BRIDGE, SB_MSG_PAIRING_FORGET_CURRENT_MODE, 0u, 0u);
  ESP_LOGI(TAG, "[NINA_BT] forget pairing requested for current mode");

  if (had_saved_host) {
    memcpy(saved_host, s_saved_switch_host_bdaddr, sizeof(saved_host));
  }

  s_reconnect_generation++;

  if (!s_bluetooth_enabled && had_saved_host) {
    result = ESP_ERR_INVALID_STATE;
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_STATE;
    ESP_LOGW(TAG,
             "[NINA_BT] saved host was cleared but Bluetooth is off, so bond removal is pending");
  }

  if (s_bluetooth_enabled &&
      (s_status.flags & (SB_STATUS_FLAG_CONNECTED | SB_STATUS_FLAG_VIRTUAL_CABLE)) != 0u) {
    esp_err_t unplug_err = esp_bt_hid_device_virtual_cable_unplug();
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_VC_UNPLUG,
                 (uint8_t)unplug_err,
                 0u);
    if (unplug_err != ESP_OK) {
      result = unplug_err;
      s_status.last_error = (uint8_t)unplug_err;
    }
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  if (s_bluetooth_enabled &&
      had_saved_host &&
      esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED &&
      nina_bt_is_bonded_to_host(saved_host)) {
    esp_err_t remove_err = esp_bt_gap_remove_bond_device(saved_host);
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_REMOVE_BOND,
                 (uint8_t)remove_err,
                 0u);
    if (remove_err != ESP_OK && result == ESP_OK) {
      result = remove_err;
      s_status.last_error = (uint8_t)remove_err;
    }
  }

  {
    esp_err_t erase_err = nina_pair_slot_erase(s_controller_profile);
    if (erase_err != ESP_OK && result == ESP_OK) {
      result = erase_err;
      s_status.last_error = (uint8_t)erase_err;
    }
  }

  memset(s_saved_switch_host_bdaddr, 0, sizeof(s_saved_switch_host_bdaddr));
  s_saved_switch_host_valid = false;
  if (s_bluetooth_enabled) {
    refresh_bond_device_count();
    configure_gap_identity();
  }
  sync_mode_status();
  return result;
}

void switch_hid_get_pairing_info(sb_pairing_info_payload_t *info) {
  nina_pair_slot_t slot;
  esp_err_t err = ESP_OK;

  if (info == NULL) {
    return;
  }

  memset(info, 0, sizeof(*info));
  info->mode = (uint8_t)switch_controller_mode();
  info->bt_state = (uint8_t)s_bt_state;

  err = nina_pair_slot_load(s_controller_profile, &slot);
  if (err != ESP_OK) {
    slot = s_active_pair_slot;
  }

  if (slot.has_local_mac == 0u || bd_addr_is_zero(slot.local_bt_mac)) {
    uint8_t local_mac[SWITCH_BD_ADDR_LEN] = {0};
    if (nina_identity_get_or_create_local_mac(s_controller_profile, local_mac) == ESP_OK) {
      slot.has_local_mac = 1u;
      memcpy(slot.local_bt_mac, local_mac, sizeof(slot.local_bt_mac));
      (void)nina_pair_slot_save(s_controller_profile, &slot);
    }
  }

  if (slot.has_local_mac != 0u) {
    memcpy(info->local_bt_mac, slot.local_bt_mac, sizeof(info->local_bt_mac));
  }
  if (slot.has_host_addr != 0u && !bd_addr_is_zero(slot.switch_bd_addr)) {
    info->has_saved_host = 1u;
    memcpy(info->saved_switch_bd_addr,
           slot.switch_bd_addr,
           sizeof(info->saved_switch_bd_addr));
    info->is_bonded = nina_bt_is_bonded_to_host(slot.switch_bd_addr) ? 1u : 0u;
  }
}

esp_err_t switch_hid_virtual_cable_unplug(void) {
  return switch_hid_forget_pairing_current_mode();
}

esp_err_t switch_hid_clear_all_bonds(void) {
  esp_err_t result = ESP_OK;
  record_event(SB_EVENT_SOURCE_BRIDGE, SB_MSG_CLEAR_BONDS, 0u, 0u);

  if (!s_bluetooth_enabled) {
    s_status.last_error = (uint8_t)ESP_ERR_INVALID_STATE;
    refresh_bond_device_count();
    return ESP_ERR_INVALID_STATE;
  }

  s_reconnect_generation++;
  (void)clear_saved_switch_host_from_nvs("clear_bonds");

  if ((s_status.flags & SB_STATUS_FLAG_CONNECTED) != 0u) {
    esp_err_t disconnect_err = esp_bt_hid_device_disconnect();
    record_event(SB_EVENT_SOURCE_HID_API,
                 SWITCH_HID_API_EVENT_DISCONNECT,
                 (uint8_t)disconnect_err,
                 0u);
    if (disconnect_err != ESP_OK) {
      result = disconnect_err;
      s_status.last_error = (uint8_t)disconnect_err;
    }
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_CLEAR_BONDS_BEGIN,
               clamp_u8_count(esp_bt_gap_get_bond_device_num()),
               0u);

  for (uint8_t attempt = 0; attempt < 6u; ++attempt) {
    esp_bd_addr_t devices[SWITCH_MAX_BOND_DEVICES];
    int device_count = esp_bt_gap_get_bond_device_num();

    refresh_bond_device_count();
    if (device_count <= 0) {
      record_event(SB_EVENT_SOURCE_HID_API,
                   SWITCH_HID_API_EVENT_CLEAR_BONDS_DONE,
                   (uint8_t)result,
                   s_status.bond_device_count);
      configure_gap_identity();
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
      record_event(SB_EVENT_SOURCE_HID_API,
                   SWITCH_HID_API_EVENT_REMOVE_BOND,
                   (uint8_t)remove_err,
                   (uint8_t)i);
      if (remove_err != ESP_OK) {
        result = remove_err;
        s_status.last_error = 0xFDu;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(150));
  }

  refresh_bond_device_count();
  record_event(SB_EVENT_SOURCE_HID_API,
               SWITCH_HID_API_EVENT_CLEAR_BONDS_DONE,
               (uint8_t)result,
               s_status.bond_device_count);
  configure_gap_identity();
  return result;
}
