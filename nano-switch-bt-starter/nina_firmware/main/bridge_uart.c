#include "bridge_uart.h"

#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "switch_hid.h"

#define BRIDGE_UART_PORT UART_NUM_0
#define BRIDGE_UART_TX GPIO_NUM_1
#define BRIDGE_UART_RX GPIO_NUM_3
#define BRIDGE_UART_BAUD 921600
#define BRIDGE_TASK_STACK 4096

typedef struct {
  uint8_t buffer[SB_MAX_FRAME_BYTES];
  size_t position;
  size_t expected_size;
} bridge_decoder_t;

static bridge_decoder_t s_decoder = {
    .position = 0,
    .expected_size = SB_FRAME_HEADER_BYTES,
};
static uint8_t s_sequence = 0;

static void bridge_decoder_reset(bridge_decoder_t *decoder) {
  decoder->position = 0;
  decoder->expected_size = SB_FRAME_HEADER_BYTES;
}

static bool bridge_decoder_push(bridge_decoder_t *decoder,
                                uint8_t byte,
                                sb_frame_t *out_frame) {
  if (decoder->position == 0) {
    if (byte != SB_FRAME_MAGIC0) {
      return false;
    }
    decoder->buffer[decoder->position++] = byte;
    return false;
  }

  if (decoder->position == 1) {
    if (byte == SB_FRAME_MAGIC0) {
      decoder->buffer[0] = byte;
      return false;
    }
    if (byte != SB_FRAME_MAGIC1) {
      bridge_decoder_reset(decoder);
      return false;
    }
    decoder->buffer[decoder->position++] = byte;
    return false;
  }

  if (decoder->position >= SB_MAX_FRAME_BYTES) {
    bridge_decoder_reset(decoder);
    return false;
  }

  decoder->buffer[decoder->position++] = byte;

  if (decoder->position == SB_FRAME_HEADER_BYTES) {
    const sb_frame_header_t *header =
        (const sb_frame_header_t *)decoder->buffer;

    if (header->version != SB_PROTOCOL_VERSION ||
        header->payload_len > SB_MAX_PAYLOAD_BYTES) {
      bridge_decoder_reset(decoder);
      return false;
    }

    decoder->expected_size = sb_frame_wire_size(header->payload_len);
  }

  if (decoder->position < decoder->expected_size) {
    return false;
  }

  memcpy(out_frame, decoder->buffer, decoder->expected_size);
  const bool valid = sb_validate_frame(out_frame);
  bridge_decoder_reset(decoder);
  return valid;
}

static void bridge_uart_send(uint8_t type,
                             const void *payload,
                             uint8_t payload_len) {
  sb_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  if (!sb_prepare_frame(&frame, type, s_sequence++, payload, payload_len)) {
    return;
  }

  uart_write_bytes(BRIDGE_UART_PORT,
                   (const char *)&frame,
                   (int)sb_frame_wire_size(frame.header.payload_len));
}

static void bridge_uart_send_hello(void) {
  sb_hello_payload_t hello;
  memset(&hello, 0, sizeof(hello));

  hello.endpoint = SB_ENDPOINT_NINA;
  hello.flags = SB_STATUS_FLAG_BRIDGE_READY;
  hello.max_payload = SB_MAX_PAYLOAD_BYTES;
  hello.link_baud = BRIDGE_UART_BAUD;

  bridge_uart_send(SB_MSG_HELLO, &hello, sizeof(hello));
}

static void bridge_uart_send_status(void) {
  sb_status_payload_t status;
  memset(&status, 0, sizeof(status));
  switch_hid_get_status(&status);
  bridge_uart_send(SB_MSG_STATUS, &status, sizeof(status));
}

static void bridge_uart_handle_frame(const sb_frame_t *frame) {
  switch (frame->header.type) {
    case SB_MSG_HELLO:
      bridge_uart_send_status();
      break;

    case SB_MSG_GET_STATUS:
      bridge_uart_send_status();
      break;

    case SB_MSG_SET_STATE:
      if (frame->header.payload_len == sizeof(sb_controller_state_t)) {
        const sb_controller_state_t *state =
            (const sb_controller_state_t *)frame->payload;
        switch_hid_apply_state(state);
      }
      break;

    case SB_MSG_VIRTUAL_CABLE_UNPLUG:
      switch_hid_virtual_cable_unplug();
      bridge_uart_send_status();
      break;

    case SB_MSG_CLEAR_BONDS:
      switch_hid_clear_all_bonds();
      bridge_uart_send_status();
      break;

    default:
      break;
  }
}

static void bridge_uart_task(void *arg) {
  (void)arg;

  uint8_t byte = 0;
  while (1) {
    const int read = uart_read_bytes(BRIDGE_UART_PORT, &byte, 1, pdMS_TO_TICKS(20));
    if (read <= 0) {
      continue;
    }

    sb_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    if (bridge_decoder_push(&s_decoder, byte, &frame)) {
      bridge_uart_handle_frame(&frame);
    }
  }
}

esp_err_t bridge_uart_init(void) {
  const uart_config_t config = {
      .baud_rate = BRIDGE_UART_BAUD,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };

  esp_err_t err = uart_driver_install(BRIDGE_UART_PORT, 2048, 0, 0, NULL, 0);
  if (err != ESP_OK) {
    return err;
  }

  err = uart_param_config(BRIDGE_UART_PORT, &config);
  if (err != ESP_OK) {
    return err;
  }

  err = uart_set_pin(
      BRIDGE_UART_PORT, BRIDGE_UART_TX, BRIDGE_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    return err;
  }

  xTaskCreate(bridge_uart_task, "bridge_uart", BRIDGE_TASK_STACK, NULL, 5, NULL);
  bridge_uart_send_hello();
  return ESP_OK;
}
