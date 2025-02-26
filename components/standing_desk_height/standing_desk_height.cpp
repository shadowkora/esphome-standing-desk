#include "standing_desk_height.h"

namespace esphome {
namespace standing_desk_height {

static const char *const TAG = "standing_desk_height";

void StandingDeskHeightSensor::set_decoder_variant(DecoderVariant decoder_variant) {
  if (this->decoder != nullptr) {
    delete this->decoder;
    this->decoder = nullptr;
  }

  // If the variant is unknown, default to ALZA.
  if (decoder_variant == DECODER_VARIANT_UNKNOWN) {
    ESP_LOGW(TAG, "Decoder variant unknown, defaulting to Alza decoder");
    decoder_variant = DECODER_VARIANT_ALZA;
  }

  this->decoder_variant = decoder_variant;
  switch (decoder_variant) {
    case DECODER_VARIANT_JARVIS:
      this->decoder = new JarvisDecoder();
      break;
    case DECODER_VARIANT_UPLIFT:
      this->decoder = new UpliftDecoder();
      break;
    case DECODER_VARIANT_OMNIDESK:
      this->decoder = new OmnideskDecoder();
      break;
    case DECODER_VARIANT_ALZA:    // Now defaults to Alza when unknown
      this->decoder = new DeskDecoder();
      break;
    default:
      ESP_LOGW(TAG, "Unknown decoder variant %d, defaulting to Alza", (uint8_t)decoder_variant);
      this->decoder_variant = DECODER_VARIANT_ALZA;
      this->decoder = new DeskDecoder();
      break;
  }
}

void StandingDeskHeightSensor::start_decoder_detection() {
  ESP_LOGI(TAG, "Starting decoder detection");

  this->decoder_variant = DECODER_VARIANT_UNKNOWN;
  this->try_next_decoder();
}

void StandingDeskHeightSensor::try_next_decoder() {
  if (this->decoder_variant == DECODER_VARIANT_COUNT - 1) {
    ESP_LOGW(TAG, "No valid decoder found. Defaulting to Alza decoder");
    delete this->decoder;
    this->decoder = nullptr;
    // Instead of setting to UNKNOWN, set explicitly to ALZA
    this->set_decoder_variant(DECODER_VARIANT_ALZA);
    this->is_detecting = false;
    return;
  }

  this->set_decoder_variant((DecoderVariant)(this->decoder_variant + 1));

  const LogString *variant_s = decoder_variant_to_string(this->decoder_variant);
  ESP_LOGD(TAG, "Attempting next decoder variant: %s", LOG_STR_ARG(variant_s));

  this->last_read = -1;
  this->started_detecting_at = millis();
  this->is_detecting = true;
}

void StandingDeskHeightSensor::setup() {
  if (this->decoder_variant == DECODER_VARIANT_UNKNOWN) {
    ESP_LOGD(TAG, "Decoder variant was not set in config; using decoder detection");
    this->start_decoder_detection();
  } else {
    const LogString *variant_s = decoder_variant_to_string(this->decoder_variant);
    ESP_LOGD(TAG, "Using hardcoded decoder variant %s", LOG_STR_ARG(variant_s));
  }
}

void StandingDeskHeightSensor::loop() {
  while (this->available() > 0) {
    uint8_t byte;
    this->read_byte(&byte);

    ESP_LOGD(TAG, "UART Received byte: 0x%02X", byte);

    if (this->decoder != nullptr && this->decoder->put(byte)) {
      float height = this->decoder->decode();
      ESP_LOGI(TAG, "Desk height detected: %.1f cm", height);
      this->last_read = height;
    }
  }

  if (this->is_detecting) {
    if (this->last_read != -1) {
      this->is_detecting = false;
      const LogString *variant_s = decoder_variant_to_string(this->decoder_variant);
      ESP_LOGI(TAG, "Decoder detection complete: %s", LOG_STR_ARG(variant_s));
    } else if (millis() - this->started_detecting_at > 1000) {
      ESP_LOGD(TAG, "Trying next decoder");
      this->try_next_decoder();
    }
  }
}


void StandingDeskHeightSensor::update() {
  if (this->last_read > 0 && this->last_read != this->last_published) {
    publish_state(this->last_read);
    this->last_published = this->last_read;
  }
}

void StandingDeskHeightSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Standing Desk Height:");
  LOG_SENSOR("  ", "Height Sensor", this);
  LOG_UPDATE_INTERVAL(this);

  const LogString *variant_s = decoder_variant_to_string(this->decoder_variant);
  ESP_LOGCONFIG(TAG, "  Decoder Variant: %s", LOG_STR_ARG(variant_s));
}

float StandingDeskHeightSensor::get_last_read() {
  return this->last_read;
}

}
}
