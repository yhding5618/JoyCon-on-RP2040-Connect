#include <Arduino.h>

#include "../shared/include/switch_bridge_protocol.h"

#if !defined(ARDUINO_NANO_RP2040_CONNECT)
#error This sketch targets the Arduino Nano RP2040 Connect.
#endif

static constexpr uint32_t kUsbBaud = 115200;
static constexpr uint32_t kNinaBaud = 921600;
static constexpr uint32_t kHelloPeriodMs = 1500;

class FrameDecoder {
 public:
  FrameDecoder() : position_(0), expected_size_(SB_FRAME_HEADER_BYTES) {}

  bool Push(uint8_t byte, sb_frame_t *out_frame) {
    if (position_ == 0) {
      if (byte != SB_FRAME_MAGIC0) {
        return false;
      }
      buffer_[position_++] = byte;
      return false;
    }

    if (position_ == 1) {
      if (byte == SB_FRAME_MAGIC0) {
        buffer_[0] = byte;
        return false;
      }
      if (byte != SB_FRAME_MAGIC1) {
        Reset();
        return false;
      }
      buffer_[position_++] = byte;
      return false;
    }

    if (position_ >= SB_MAX_FRAME_BYTES) {
      Reset();
      return false;
    }

    buffer_[position_++] = byte;

    if (position_ == SB_FRAME_HEADER_BYTES) {
      const sb_frame_header_t *header =
          reinterpret_cast<const sb_frame_header_t *>(buffer_);

      if (header->version != SB_PROTOCOL_VERSION ||
          header->payload_len > SB_MAX_PAYLOAD_BYTES) {
        Reset();
        return false;
      }

      expected_size_ = sb_frame_wire_size(header->payload_len);
    }

    if (position_ < expected_size_) {
      return false;
    }

    memcpy(out_frame, buffer_, expected_size_);
    const bool is_valid = sb_validate_frame(out_frame);
    Reset();
    return is_valid;
  }

 private:
  void Reset() {
    position_ = 0;
    expected_size_ = SB_FRAME_HEADER_BYTES;
  }

  uint8_t buffer_[SB_MAX_FRAME_BYTES];
  size_t position_;
  size_t expected_size_;
};

static FrameDecoder g_usb_decoder;
static FrameDecoder g_nina_decoder;
static uint8_t g_sequence_to_nina = 0;
static uint32_t g_last_hello_ms = 0;

static size_t WriteFrame(Print &destination, const sb_frame_t &frame) {
  return destination.write(reinterpret_cast<const uint8_t *>(&frame),
                           sb_frame_wire_size(frame.header.payload_len));
}

static void SendHelloToNina() {
  sb_frame_t frame = {};
  sb_hello_payload_t hello = {};

  hello.endpoint = SB_ENDPOINT_RP2040;
  hello.flags = SB_STATUS_FLAG_BRIDGE_READY;
  hello.max_payload = SB_MAX_PAYLOAD_BYTES;
  hello.link_baud = kNinaBaud;

  if (sb_prepare_frame(&frame,
                       SB_MSG_HELLO,
                       g_sequence_to_nina++,
                       &hello,
                       sizeof(hello))) {
    WriteFrame(SerialHCI, frame);
  }
}

static void PumpFrames(Stream &source,
                       Print &destination,
                       FrameDecoder &decoder,
                       bool bump_sequence_on_forward) {
  while (source.available() > 0) {
    const int incoming = source.read();
    if (incoming < 0) {
      return;
    }

    sb_frame_t frame = {};
    if (!decoder.Push(static_cast<uint8_t>(incoming), &frame)) {
      continue;
    }

    if (bump_sequence_on_forward) {
      frame.header.sequence = g_sequence_to_nina++;
      frame.header.crc16 = sb_compute_frame_crc(&frame);
    }

    WriteFrame(destination, frame);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(kUsbBaud);
  SerialHCI.begin(kNinaBaud);

  delay(300);
  SendHelloToNina();
  g_last_hello_ms = millis();
}

void loop() {
  PumpFrames(Serial, SerialHCI, g_usb_decoder, true);
  PumpFrames(SerialHCI, Serial, g_nina_decoder, false);

  const uint32_t now = millis();
  if ((now - g_last_hello_ms) >= kHelloPeriodMs) {
    SendHelloToNina();
    g_last_hello_ms = now;
  }
}
