#include "ads124s08.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace ads124s08 {

static const char *const TAG = "ads124s08";

static const uint8_t CMD_RESET = 0x06;
static const uint8_t CMD_START = 0x08;
static const uint8_t CMD_STOP = 0x0A;
static const uint8_t CMD_RDATA = 0x12;
static const uint8_t CMD_RREG = 0x20;
static const uint8_t CMD_WREG = 0x40;

static const uint8_t REG_ID = 0x00;
static const uint8_t REG_STATUS = 0x01;
static const uint8_t REG_INPMUX = 0x02;
static const uint8_t REG_PGA = 0x03;
static const uint8_t REG_DATARATE = 0x04;
static const uint8_t REG_REF = 0x05;
static const uint8_t REG_IDACMAG = 0x06;
static const uint8_t REG_IDACMUX = 0x07;
static const uint8_t REG_SYS = 0x09;

static const uint8_t STATUS_REF_ALARM = 0x03;
static const uint8_t STATUS_PGA_RAIL_ALARM = 0x3C;

static const uint8_t ID_DEV_ID_MASK = 0x07;

static const uint8_t PGA_ENABLE = 0x08;
static const uint8_t DATARATE_SINGLE_SHOT = 0x20;
static const uint8_t DATARATE_LOW_LATENCY_FILTER = 0x10;
static const uint8_t REF_POSITIVE_BUFFER_BYPASS = 0x20;
static const uint8_t REF_NEGATIVE_BUFFER_BYPASS = 0x10;
static const uint8_t REF_MONITOR_L0_L1 = 0x80;
static const uint8_t REF_SELECTION_SHIFT = 2;
static const uint8_t REF_INTERNAL_ALWAYS_ON = 0x02;
static const uint8_t IDAC_PGA_RAIL_MONITOR = 0x80;
static const uint8_t IDAC2_DISCONNECTED = 0xF0;
static const uint8_t IDACMUX_ALL_DISCONNECTED = 0xFF;

void ADS124S08Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADS124S08 hub...");
  this->spi_setup();

  if (this->drdy_pin_ != nullptr) {
    this->drdy_pin_->setup();
  }
  if (this->start_sync_pin_ != nullptr) {
    this->start_sync_pin_->setup();
    // START commands are ignored by the ADC if START/SYNC is high.
    this->start_sync_pin_->digital_write(false);
  }

  this->command_(CMD_RESET);
  delay(5);

  const uint8_t id = this->read_register_(REG_ID);
  if (id == 0xFF) {
    ESP_LOGE(TAG, "Failed to communicate with ADS124S08 (ID=0xFF)");
    this->mark_failed();
    return;
  }
  const uint8_t dev_id = id & ID_DEV_ID_MASK;
  if (dev_id != static_cast<uint8_t>(this->model_)) {
    ESP_LOGE(TAG, "Configured model does not match ADS124S0x DEV_ID (configured=%u, device=%u)",
             static_cast<unsigned>(this->model_), static_cast<unsigned>(dev_id));
    this->mark_failed();
    return;
  }

  this->write_register_(REG_SYS, 0x00);  // Disable STATUS byte and CRC in RDATA responses.
  this->write_register_(REG_REF, this->reference_register_());
  this->write_register_(REG_DATARATE, DATARATE_SINGLE_SHOT | DATARATE_LOW_LATENCY_FILTER |
                                          (static_cast<uint8_t>(this->data_rate_) & 0x0F));
  this->write_register_(REG_PGA, PGA_ENABLE | (this->pga_gain_code_ & 0x07));
  this->command_(CMD_STOP);

  ESP_LOGD(TAG, "ADS124S08 ID register: 0x%02X", id);
}

float ADS124S08Component::get_setup_priority() const { return setup_priority::DATA; }

void ADS124S08Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS124S08 Hub:");
  ESP_LOGCONFIG(TAG, "  Model: ADS124S0%u", this->model_ == ADS124S08 ? 8 : 6);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DRDY Pin: ", this->drdy_pin_);
  LOG_PIN("  START/SYNC Pin: ", this->start_sync_pin_);
  ESP_LOGCONFIG(TAG, "  Reference: %u", static_cast<uint8_t>(this->reference_));
  ESP_LOGCONFIG(TAG, "  PGA gain: %.0f", this->gain_value_());
  ESP_LOGCONFIG(TAG, "  Data rate code: 0x%02X", static_cast<uint8_t>(this->data_rate_));
  ESP_LOGCONFIG(TAG, "  Conversion time: %" PRIu32 " ms", this->conversion_time_ms_);
  ESP_LOGCONFIG(TAG, "  Channels: %u", static_cast<unsigned>(this->channels_.size()));
  LOG_UPDATE_INTERVAL(this);
}

void ADS124S08Component::add_rtd_channel(uint8_t ain_p, uint8_t ain_n, uint8_t idac_pin, float idac_current_a,
                                         float reference_resistance, float nominal_resistance, uint8_t rtd_wires,
                                         sensor::Sensor *temperature_sensor, sensor::Sensor *resistance_sensor,
                                         sensor::Sensor *raw_sensor, text_sensor::TextSensor *status_sensor) {
  this->channels_.push_back({ADS124S08_CHANNEL_RTD, ain_p, ain_n, idac_pin, idac_current_a, reference_resistance,
                             nominal_resistance, 3950.0f, 25.0f, rtd_wires, temperature_sensor, resistance_sensor,
                             raw_sensor, status_sensor});
}

void ADS124S08Component::add_ntc_channel(uint8_t ain_p, uint8_t ain_n, float reference_resistance,
                                         float nominal_resistance, float beta, float reference_temperature_c,
                                         sensor::Sensor *temperature_sensor, sensor::Sensor *resistance_sensor,
                                         sensor::Sensor *raw_sensor, text_sensor::TextSensor *status_sensor) {
  this->channels_.push_back({ADS124S08_CHANNEL_NTC, ain_p, ain_n, 0x0F, 0.0f, reference_resistance, nominal_resistance,
                             beta, reference_temperature_c, 0, temperature_sensor, resistance_sensor, raw_sensor,
                             status_sensor});
}

void ADS124S08Component::command_(uint8_t command) {
  this->enable();
  this->transfer_byte(command);
  this->disable();
}

uint8_t ADS124S08Component::read_register_(uint8_t reg) {
  this->enable();
  this->transfer_byte(CMD_RREG | (reg & 0x1F));
  this->transfer_byte(0x00);
  const uint8_t value = this->transfer_byte(0x00);
  this->disable();
  return value;
}

void ADS124S08Component::write_register_(uint8_t reg, uint8_t value) {
  this->enable();
  this->transfer_byte(CMD_WREG | (reg & 0x1F));
  this->transfer_byte(0x00);
  this->transfer_byte(value);
  this->disable();
}

int32_t ADS124S08Component::read_data_() {
  this->enable();
  this->transfer_byte(CMD_RDATA);
  const uint8_t b2 = this->transfer_byte(0x00);
  const uint8_t b1 = this->transfer_byte(0x00);
  const uint8_t b0 = this->transfer_byte(0x00);
  this->disable();

  int32_t value = (static_cast<int32_t>(b2) << 16) | (static_cast<int32_t>(b1) << 8) | b0;
  if (value & 0x800000) {
    value |= 0xFF000000;
  }
  return value;
}

void ADS124S08Component::update() {
  if (this->state_ != STATE_IDLE) {
    ESP_LOGW(TAG, "Previous channel scan still in progress");
    return;
  }
  if (this->channels_.empty()) {
    ESP_LOGW(TAG, "No ADS124S08 channels configured");
    return;
  }

  this->current_channel_ = 0;
  this->start_next_channel_();
}

void ADS124S08Component::start_next_channel_() {
  if (this->current_channel_ >= this->channels_.size()) {
    this->state_ = STATE_IDLE;
    this->command_(CMD_STOP);
    return;
  }

  const auto &channel = this->channels_[this->current_channel_];
  this->state_ = STATE_CONFIGURING;
  this->configure_channel_(channel);
  this->state_ = STATE_CONVERTING;
  this->set_timeout("conversion", this->conversion_time_ms_, [this]() { this->finish_channel_(); });
}

void ADS124S08Component::configure_channel_(const ADS124S08Channel &channel) {
  this->command_(CMD_STOP);

  this->write_register_(REG_INPMUX, ((channel.ain_p & 0x0F) << 4) | (channel.ain_n & 0x0F));
  this->write_register_(REG_PGA, PGA_ENABLE | (this->pga_gain_code_ & 0x07));
  this->write_register_(REG_DATARATE, DATARATE_SINGLE_SHOT | DATARATE_LOW_LATENCY_FILTER |
                                          (static_cast<uint8_t>(this->data_rate_) & 0x0F));
  this->write_register_(REG_REF, this->reference_register_());

  if (channel.type == ADS124S08_CHANNEL_RTD) {
    this->write_register_(REG_IDACMAG, IDAC_PGA_RAIL_MONITOR | this->idac_current_code_(channel.idac_current_a));
    this->write_register_(REG_IDACMUX, IDAC2_DISCONNECTED | (channel.idac_pin & 0x0F));
  } else {
    this->write_register_(REG_IDACMAG, 0x00);
    this->write_register_(REG_IDACMUX, IDACMUX_ALL_DISCONNECTED);
  }

  this->command_(CMD_START);
}

void ADS124S08Component::finish_channel_() {
  this->state_ = STATE_READING;
  const auto &channel = this->channels_[this->current_channel_];

  if (this->drdy_pin_ != nullptr && this->drdy_pin_->digital_read()) {
    ESP_LOGW(TAG, "Channel %u conversion not ready", static_cast<unsigned>(this->current_channel_));
    this->publish_nan_(channel, "not_ready");
    this->current_channel_++;
    this->start_next_channel_();
    return;
  }

  const uint8_t status = this->read_register_(REG_STATUS);
  const int32_t raw = this->read_data_();

  if (channel.raw_sensor != nullptr) {
    channel.raw_sensor->publish_state(raw);
  }

  if (status & (STATUS_REF_ALARM | STATUS_PGA_RAIL_ALARM)) {
    ESP_LOGW(TAG, "Channel %u status alarm: 0x%02X", static_cast<unsigned>(this->current_channel_), status);
    this->publish_status_(channel, "alarm");
  }

  const float ratio = this->code_to_ratio_(raw);
  float resistance = NAN;
  float temperature = NAN;

  if (channel.type == ADS124S08_CHANNEL_RTD) {
    resistance = ratio * channel.reference_resistance;
    temperature = this->rtd_temperature_(resistance);
  } else {
    if (ratio > 0.0f && ratio < 1.0f) {
      resistance = channel.reference_resistance * ratio / (1.0f - ratio);
      temperature = this->ntc_temperature_(resistance, channel);
    }
  }

  if (!std::isfinite(resistance) || !std::isfinite(temperature)) {
    ESP_LOGW(TAG, "Channel %u invalid reading: raw=%d ratio=%.8f resistance=%.3f",
             static_cast<unsigned>(this->current_channel_), raw, ratio, resistance);
    this->publish_nan_(channel, "invalid");
  } else {
    if (channel.resistance_sensor != nullptr) {
      channel.resistance_sensor->publish_state(resistance);
    }
    channel.temperature_sensor->publish_state(temperature);
    this->publish_status_(channel, "ok");
    ESP_LOGD(TAG, "Channel %u raw=%d ratio=%.8f resistance=%.3f temperature=%.2f C",
             static_cast<unsigned>(this->current_channel_), raw, ratio, resistance, temperature);
  }

  this->current_channel_++;
  this->start_next_channel_();
}

void ADS124S08Component::publish_status_(const ADS124S08Channel &channel, const char *status) {
  if (channel.status_sensor != nullptr) {
    channel.status_sensor->publish_state(status);
  }
}

void ADS124S08Component::publish_nan_(const ADS124S08Channel &channel, const char *status) {
  channel.temperature_sensor->publish_state(NAN);
  if (channel.resistance_sensor != nullptr) {
    channel.resistance_sensor->publish_state(NAN);
  }
  this->publish_status_(channel, status);
}

uint8_t ADS124S08Component::idac_current_code_(float current_a) const {
  const float current_ua = current_a * 1000000.0f;
  if (current_ua <= 30.0f)
    return 0x01;
  if (current_ua <= 75.0f)
    return 0x02;
  if (current_ua <= 175.0f)
    return 0x03;
  if (current_ua <= 375.0f)
    return 0x04;
  if (current_ua <= 625.0f)
    return 0x05;
  if (current_ua <= 875.0f)
    return 0x06;
  if (current_ua <= 1250.0f)
    return 0x07;
  if (current_ua <= 1750.0f)
    return 0x08;
  return 0x09;
}

uint8_t ADS124S08Component::reference_register_() const {
  uint8_t value = REF_INTERNAL_ALWAYS_ON | ((static_cast<uint8_t>(this->reference_) & 0x03) << REF_SELECTION_SHIFT);

  if (this->reference_ == ADS124S08_INTERNAL) {
    // TI recommends bypassing both reference buffers when measuring against the internal reference.
    value |= REF_POSITIVE_BUFFER_BYPASS | REF_NEGATIVE_BUFFER_BYPASS;
  } else {
    value |= REF_NEGATIVE_BUFFER_BYPASS;
    // External reference monitoring catches missing reference and open-excitation faults.
    value |= REF_MONITOR_L0_L1;
  }

  return value;
}

float ADS124S08Component::gain_value_() const { return static_cast<float>(1U << this->pga_gain_code_); }

float ADS124S08Component::code_to_ratio_(int32_t raw) const {
  return static_cast<float>(raw) / (8388608.0f * this->gain_value_());
}

float ADS124S08Component::rtd_temperature_(float resistance) const {
  const auto &channel = this->channels_[this->current_channel_];
  const float r0 = channel.nominal_resistance;
  const float a = 3.9083e-3f;
  const float b = -5.775e-7f;
  const float c = -4.183e-12f;
  const float ratio = resistance / r0;

  if (ratio >= 1.0f) {
    const float discriminant = a * a - 4.0f * b * (1.0f - ratio);
    if (discriminant < 0.0f)
      return NAN;
    return (-a + std::sqrt(discriminant)) / (2.0f * b);
  }

  float t = -50.0f;
  for (uint8_t i = 0; i < 12; i++) {
    const float f = 1.0f + a * t + b * t * t + c * (t - 100.0f) * t * t * t - ratio;
    const float df = a + 2.0f * b * t + c * (4.0f * t * t * t - 300.0f * t * t);
    if (std::fabs(df) < 1e-9f)
      break;
    t -= f / df;
  }
  return t;
}

float ADS124S08Component::ntc_temperature_(float resistance, const ADS124S08Channel &channel) const {
  if (resistance <= 0.0f || channel.nominal_resistance <= 0.0f || channel.beta <= 0.0f)
    return NAN;

  const float t0_k = channel.reference_temperature_c + 273.15f;
  const float inv_t = (1.0f / t0_k) + (std::log(resistance / channel.nominal_resistance) / channel.beta);
  if (inv_t <= 0.0f)
    return NAN;
  return (1.0f / inv_t) - 273.15f;
}

}  // namespace ads124s08
}  // namespace esphome
