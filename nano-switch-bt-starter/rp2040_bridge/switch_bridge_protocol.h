#ifndef SWITCH_BRIDGE_PROTOCOL_H
#define SWITCH_BRIDGE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(__GNUC__)
#define SB_PACKED __attribute__((packed))
#else
#define SB_PACKED
#endif

#define SB_FRAME_MAGIC0 0x53u
#define SB_FRAME_MAGIC1 0x42u
#define SB_PROTOCOL_VERSION 0x01u
#define SB_MAX_PAYLOAD_BYTES 48u
#define SB_FRAME_HEADER_BYTES 8u
#define SB_MAX_FRAME_BYTES (SB_FRAME_HEADER_BYTES + SB_MAX_PAYLOAD_BYTES)

typedef enum {
  SB_MSG_HELLO = 0x01,
  SB_MSG_GET_STATUS = 0x02,
  SB_MSG_STATUS = 0x03,
  SB_MSG_GET_EVENTS = 0x04,
  SB_MSG_EVENTS = 0x05,
  SB_MSG_SET_STATE = 0x10,
  SB_MSG_VIRTUAL_CABLE_UNPLUG = 0x11,
  SB_MSG_CLEAR_BONDS = 0x12,
} sb_message_type_t;

typedef enum {
  SB_ENDPOINT_HOST = 0x01,
  SB_ENDPOINT_RP2040 = 0x02,
  SB_ENDPOINT_NINA = 0x03,
} sb_endpoint_t;

typedef enum {
  SB_STATUS_FLAG_BRIDGE_READY = 1u << 0,
  SB_STATUS_FLAG_BT_READY = 1u << 1,
  SB_STATUS_FLAG_HID_READY = 1u << 2,
  SB_STATUS_FLAG_CONNECTED = 1u << 3,
  SB_STATUS_FLAG_VIRTUAL_CABLE = 1u << 4,
} sb_status_flag_bits_t;

typedef enum {
  SB_BTN_LJC_DOWN = 1u << 0,
  SB_BTN_LJC_UP = 1u << 1,
  SB_BTN_LJC_RIGHT = 1u << 2,
  SB_BTN_LJC_LEFT = 1u << 3,
  SB_BTN_LJC_SL = 1u << 4,
  SB_BTN_LJC_SR = 1u << 5,
  SB_BTN_LJC_L = 1u << 6,
  SB_BTN_LJC_ZL = 1u << 7,
  SB_BTN_LJC_MINUS = 1u << 8,
  SB_BTN_LJC_STICK = 1u << 9,
  SB_BTN_LJC_CAPTURE = 1u << 10,
} sb_left_joycon_button_bits_t;

typedef enum {
  SB_MISC_CHARGING = 1u << 0,
  SB_MISC_CHARGING_GRIP = 1u << 1,
} sb_misc_flag_bits_t;

typedef enum {
  SB_EVENT_SOURCE_NONE = 0x00,
  SB_EVENT_SOURCE_HID_CALLBACK = 0x01,
  SB_EVENT_SOURCE_GAP_CALLBACK = 0x02,
  SB_EVENT_SOURCE_HID_API = 0x03,
  SB_EVENT_SOURCE_BRIDGE = 0x04,
  SB_EVENT_SOURCE_BT_IDENTITY = 0x05,
} sb_event_source_t;

#define SB_EVENT_DUMP_MAX_ENTRIES 5u
#define SB_EVENT_LOG_MAX_ENTRIES 128u

typedef struct SB_PACKED {
  uint8_t magic0;
  uint8_t magic1;
  uint8_t version;
  uint8_t type;
  uint8_t sequence;
  uint8_t payload_len;
  uint16_t crc16;
} sb_frame_header_t;

typedef struct SB_PACKED {
  uint32_t buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint8_t hat;
  uint8_t misc;
  uint8_t battery_level;
  uint8_t reserved;
} sb_controller_state_t;

typedef struct SB_PACKED {
  uint8_t endpoint;
  uint8_t flags;
  uint16_t max_payload;
  uint32_t link_baud;
} sb_hello_payload_t;

typedef struct SB_PACKED {
  uint8_t flags;
  uint8_t protocol_mode;
  uint8_t input_report_mode;
  uint8_t battery_level;
  uint8_t last_host_report_id;
  uint8_t last_error;
  uint8_t last_subcommand;
  uint8_t last_hid_event;
  uint8_t last_hid_status;
  uint8_t last_hid_conn_status;
  uint8_t last_hid_report_type;
  uint8_t last_hid_report_id;
  uint8_t last_gap_event;
  uint8_t last_gap_status;
  uint8_t last_gap_reason;
  uint8_t bond_device_count;
} sb_status_payload_t;

typedef struct SB_PACKED {
  uint32_t timestamp_ms;
  uint8_t source;
  uint8_t event;
  uint8_t arg0;
  uint8_t arg1;
} sb_event_entry_t;

typedef struct SB_PACKED {
  uint16_t first_sequence;
  uint8_t chunk_index;
  uint8_t chunk_count;
  uint8_t entry_count;
  uint8_t total_entries;
  uint8_t overflowed;
  uint8_t reserved;
  sb_event_entry_t entries[SB_EVENT_DUMP_MAX_ENTRIES];
} sb_event_dump_payload_t;

typedef struct SB_PACKED {
  sb_frame_header_t header;
  uint8_t payload[SB_MAX_PAYLOAD_BYTES];
} sb_frame_t;

static inline uint16_t sb_crc16_ccitt_seed(uint16_t seed,
                                           const uint8_t *data,
                                           size_t len) {
  uint16_t crc = seed;

  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if ((crc & 0x8000u) != 0u) {
        crc = (uint16_t)((crc << 1) ^ 0x1021u);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

static inline uint16_t sb_crc16_ccitt(const uint8_t *data, size_t len) {
  return sb_crc16_ccitt_seed(0xFFFFu, data, len);
}

static inline size_t sb_frame_wire_size(uint8_t payload_len) {
  return (size_t)SB_FRAME_HEADER_BYTES + (size_t)payload_len;
}

static inline uint16_t sb_compute_frame_crc(const sb_frame_t *frame) {
  uint8_t metadata[4];

  metadata[0] = frame->header.version;
  metadata[1] = frame->header.type;
  metadata[2] = frame->header.sequence;
  metadata[3] = frame->header.payload_len;

  uint16_t crc = sb_crc16_ccitt(metadata, sizeof(metadata));
  if (frame->header.payload_len > 0u) {
    crc = sb_crc16_ccitt_seed(crc, frame->payload, frame->header.payload_len);
  }

  return crc;
}

static inline bool sb_prepare_frame(sb_frame_t *frame,
                                    uint8_t type,
                                    uint8_t sequence,
                                    const void *payload,
                                    uint8_t payload_len) {
  if (frame == NULL || payload_len > SB_MAX_PAYLOAD_BYTES) {
    return false;
  }

  frame->header.magic0 = SB_FRAME_MAGIC0;
  frame->header.magic1 = SB_FRAME_MAGIC1;
  frame->header.version = SB_PROTOCOL_VERSION;
  frame->header.type = type;
  frame->header.sequence = sequence;
  frame->header.payload_len = payload_len;

  memset(frame->payload, 0, SB_MAX_PAYLOAD_BYTES);
  if (payload_len > 0u && payload != NULL) {
    memcpy(frame->payload, payload, payload_len);
  }

  frame->header.crc16 = sb_compute_frame_crc(frame);
  return true;
}

static inline bool sb_validate_frame(const sb_frame_t *frame) {
  if (frame == NULL) {
    return false;
  }

  if (frame->header.magic0 != SB_FRAME_MAGIC0 ||
      frame->header.magic1 != SB_FRAME_MAGIC1 ||
      frame->header.version != SB_PROTOCOL_VERSION ||
      frame->header.payload_len > SB_MAX_PAYLOAD_BYTES) {
    return false;
  }

  return frame->header.crc16 == sb_compute_frame_crc(frame);
}

#endif
