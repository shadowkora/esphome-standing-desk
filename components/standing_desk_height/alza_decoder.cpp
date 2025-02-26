#include "alza_decoder.h"
#include "esphome/core/log.h"

namespace esphome {
namespace standing_desk_height {

static const char *const TAG = "DeskDecoder";

bool DeskDecoder::put(uint8_t b) {
  ESP_LOGD(TAG, "Received byte: 0x%02X", b);

  switch (state_) {
    case State::WAIT_START:
      if (b == 0x5A) {
        ESP_LOGD(TAG, "Start byte detected.");
        state_ = State::BYTE1;
        checksum_ = 0;  // Reset checksum accumulator
      }
      break;

    case State::BYTE1:
      buf_[0] = b;
      checksum_ = b;
      state_ = State::BYTE2;
      ESP_LOGD(TAG, "Byte1: 0x%02X", b);
      break;

    case State::BYTE2:
      buf_[1] = b;
      checksum_ += b;
      state_ = State::BYTE3;
      ESP_LOGD(TAG, "Byte2: 0x%02X", b);
      break;

    case State::BYTE3:
      buf_[2] = b;
      checksum_ += b;
      state_ = State::BYTE4;
      ESP_LOGD(TAG, "Byte3: 0x%02X", b);
      break;

    case State::BYTE4:
      buf_[3] = b;
      checksum_ += b;
      state_ = State::CHECKSUM;
      ESP_LOGD(TAG, "Byte4 (Display Mode): 0x%02X", b);
      break;

    case State::CHECKSUM:
      ESP_LOGD(TAG, "Checksum received: 0x%02X, calculated: 0x%02X", b, (checksum_ & 0xff));
      if ((checksum_ & 0xff) == b) {
        ESP_LOGD(TAG, "Valid frame received!");
        state_ = State::WAIT_START;
        return true;
      } else {
        ESP_LOGW(TAG, "Checksum error! Resetting state.");
        state_ = State::WAIT_START;
      }
      break;
  }
  return false;
}

int DeskDecoder::decode_digit(uint8_t b) {
  // Mask out the high bit (which may indicate the decimal point)
  uint8_t val = b & 0x7F;
  // We use the canonical 7-seg values:
  // 0: 0x3F, 1: 0x06, 2: 0x5B, 3: 0x4F, 4: 0x66,
  // 5: 0x6D, 6: 0x7D, 7: 0x07, 8: 0x7F, 9: 0x6F.
  switch (val) {
    case 0x3F: return 0;
    case 0x06: return 1;
    case 0x5B: return 2;
    case 0x4F: return 3;
    case 0x66: return 4;
    case 0x6D: return 5;
    case 0x7D: return 6;
    case 0x07: return 7;
    case 0x7F: return 8;
    case 0x6F: return 9;
    default:   return -1;  // unknown digit
  }
}

float DeskDecoder::decode() {
  int d1 = decode_digit(buf_[0]);
  int d2 = decode_digit(buf_[1]);
  int d3 = decode_digit(buf_[2]);

  if (d1 < 0 || d2 < 0 || d3 < 0) {
    ESP_LOGW(TAG, "Error decoding digits: %d %d %d", d1, d2, d3);
    return -1.0f;
  }

  bool has_decimal = (buf_[1] & 0x80) != 0;
  float height;

  if (has_decimal) {
    height = (d1 * 10 + d2) + (d3 / 10.0f);
    ESP_LOGD(TAG, "Decoded height: %.1f (with decimal)", height);
  } else {
    height = (d1 * 100 + d2 * 10 + d3);
    ESP_LOGD(TAG, "Decoded height: %.1f", height);
  }

  return height;
}

void DeskDecoder::reset() {
  state_ = State::WAIT_START;
  checksum_ = 0;
}

}  // namespace standing_desk_height
}  // namespace esphome
