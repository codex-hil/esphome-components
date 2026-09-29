#include "as6500.h"
#include "esphome/core/log.h"

namespace esphome {
namespace as6500 {

static const char *const TAG = "as6500";

void AS6500::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AS6500...");

  this->spi_setup();

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->pin_mode(gpio::FLAG_INPUT);
  }

  this->power_on_reset_();
  delay(1);

  this->write_config_();

  if (!this->verify_config_()) {
    ESP_LOGE(TAG, "Configuration verification failed");
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "AS6500 configured successfully");
}

void AS6500::dump_config() {
  ESP_LOGCONFIG(TAG, "AS6500:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  ESP_LOGCONFIG(TAG, "  Clock Source: %s", this->use_crystal_ ? "Crystal (XOSC)" : "External REFCLK");
  ESP_LOGCONFIG(TAG, "  REFCLK Divisions: %u", this->refclk_divisions_);
  ESP_LOGCONFIG(TAG, "  High Resolution: %s",
                this->high_res_mode_ == HIGH_RES_OFF  ? "OFF"
                : this->high_res_mode_ == HIGH_RES_X2 ? "X2"
                                                      : "X4");
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    ESP_LOGCONFIG(TAG, "  Channel %d: %s", i + 1, this->stop_enabled_[i] ? "enabled" : "disabled");
  }
  LOG_UPDATE_INTERVAL(this);
}

void AS6500::update() {
  this->init_measurement_();

  if (this->interrupt_pin_ != nullptr) {
    uint32_t timeout = 1000;
    while (this->interrupt_pin_->digital_read() && timeout > 0) {
      delay(1);
      timeout--;
    }
    if (timeout == 0) {
      ESP_LOGW(TAG, "Measurement timeout");
      return;
    }
  } else {
    delay(10);
  }

  if (!this->read_results_()) {
    ESP_LOGW(TAG, "Failed to read results");
    return;
  }

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (this->channel_sensors_[i] != nullptr && this->stop_enabled_[i]) {
      float time_ps = static_cast<float>(this->stop_result_[i]);
      this->channel_sensors_[i]->publish_state(time_ps);
      ESP_LOGD(TAG, "Channel %d: ref_idx=%u, tstop=%u ps", i + 1, this->reference_index_[i], this->stop_result_[i]);
    }
  }
}

void AS6500::power_on_reset_() {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::POWER_ON_RESET);
  this->disable();
  delayMicroseconds(100);
}

void AS6500::write_config_() {
  uint8_t config[NUM_CONFIG_REGS];

  // CFG0: Pin enables
  config[0] = 0;
  if (this->stop_enabled_[0])
    config[0] |= cfg0::PIN_ENA_STOP1;
  if (this->stop_enabled_[1])
    config[0] |= cfg0::PIN_ENA_STOP2;
  if (this->stop_enabled_[2])
    config[0] |= cfg0::PIN_ENA_STOP3;
  if (this->stop_enabled_[3])
    config[0] |= cfg0::PIN_ENA_STOP4;
  if (!this->use_crystal_)
    config[0] |= cfg0::PIN_ENA_REFCLK;

  // CFG1: Hit enables, channel combine, high resolution
  config[1] = 0;
  if (this->stop_enabled_[0])
    config[1] |= cfg1::HIT_ENA_STOP1;
  if (this->stop_enabled_[1])
    config[1] |= cfg1::HIT_ENA_STOP2;
  if (this->stop_enabled_[2])
    config[1] |= cfg1::HIT_ENA_STOP3;
  if (this->stop_enabled_[3])
    config[1] |= cfg1::HIT_ENA_STOP4;

  switch (this->combine_mode_) {
    case COMBINE_PULSE_DISTANCE:
      config[1] |= cfg1::CHANNEL_COMBINE_PULSE_DISTANCE;
      break;
    case COMBINE_PULSE_WIDTH:
      config[1] |= cfg1::CHANNEL_COMBINE_PULSE_WIDTH;
      break;
    default:
      break;
  }

  switch (this->high_res_mode_) {
    case HIGH_RES_X2:
      config[1] |= cfg1::HIGH_RESOLUTION_X2;
      break;
    case HIGH_RES_X4:
      config[1] |= cfg1::HIGH_RESOLUTION_X4;
      break;
    default:
      break;
  }

  // CFG2: FIFO mode
  config[2] = 0;

  // CFG3-5: REFCLK_DIVISIONS (24-bit, LSB first)
  config[3] = this->refclk_divisions_ & 0xFF;
  config[4] = (this->refclk_divisions_ >> 8) & 0xFF;
  config[5] = (this->refclk_divisions_ >> 16) & 0x0F;

  // CFG6-16: Fixed values
  config[6] = defaults::CFG6;
  config[7] = defaults::CFG7;
  if (this->use_crystal_)
    config[7] |= cfg7::REFCLK_BY_XOSC;
  config[8] = defaults::CFG8;
  config[9] = defaults::CFG9;
  config[10] = defaults::CFG10;
  config[11] = defaults::CFG11;
  config[12] = defaults::CFG12;
  config[13] = defaults::CFG13;
  config[14] = defaults::CFG14;
  config[15] = defaults::CFG15;
  config[16] = defaults::CFG16;

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::WRITE_CONFIG | 0x00);
  for (uint8_t i = 0; i < NUM_CONFIG_REGS; i++) {
    this->transfer_byte(config[i]);
  }
  this->disable();
}

bool AS6500::verify_config_() {
  uint8_t config[NUM_CONFIG_REGS];

  // Build expected config
  config[0] = 0;
  if (this->stop_enabled_[0])
    config[0] |= cfg0::PIN_ENA_STOP1;
  if (this->stop_enabled_[1])
    config[0] |= cfg0::PIN_ENA_STOP2;
  if (this->stop_enabled_[2])
    config[0] |= cfg0::PIN_ENA_STOP3;
  if (this->stop_enabled_[3])
    config[0] |= cfg0::PIN_ENA_STOP4;
  if (!this->use_crystal_)
    config[0] |= cfg0::PIN_ENA_REFCLK;

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::READ_CONFIG | 0x00);

  bool valid = true;
  for (uint8_t i = 0; i < NUM_CONFIG_REGS; i++) {
    uint8_t received = this->transfer_byte(0xFF);
    if (i == 0 && received != config[0]) {
      ESP_LOGE(TAG, "Config[%d] mismatch: expected 0x%02X, got 0x%02X", i, config[0], received);
      valid = false;
    }
  }
  this->disable();

  return valid;
}

void AS6500::init_measurement_() {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::INIT);
  this->disable();
}

bool AS6500::read_results_() {
  uint8_t buffer[NUM_CHANNELS * BYTES_PER_CHANNEL];

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::READ_RESULTS | result_addr::CH1_REFIDX);

  for (uint8_t i = 0; i < NUM_CHANNELS * BYTES_PER_CHANNEL; i++) {
    buffer[i] = this->transfer_byte(0xFF);
  }
  this->disable();

  for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
    uint8_t offset = ch * BYTES_PER_CHANNEL;

    this->reference_index_[ch] = (static_cast<uint32_t>(buffer[offset]) << 16) |
                                 (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                                 static_cast<uint32_t>(buffer[offset + 2]);

    this->stop_result_[ch] = (static_cast<uint32_t>(buffer[offset + 3]) << 16) |
                             (static_cast<uint32_t>(buffer[offset + 4]) << 8) |
                             static_cast<uint32_t>(buffer[offset + 5]);
  }

  return true;
}

void AS6500::write_register_(uint8_t address, uint8_t data) {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::WRITE_CONFIG | address);
  this->transfer_byte(data);
  this->disable();
}

uint8_t AS6500::read_register_(uint8_t address) {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(opcode::READ_CONFIG | address);
  uint8_t data = this->transfer_byte(0xFF);
  this->disable();
  return data;
}

}  // namespace as6500
}  // namespace esphome
