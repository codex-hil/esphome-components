#include "dacx0504.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace dacx0504 {

static const char *const TAG = "dacx0504";

static const uint8_t REG_SYNC = 0x02;
static const uint8_t REG_CONFIG = 0x03;
static const uint8_t REG_GAIN = 0x04;
static const uint8_t REG_TRIGGER = 0x05;
static const uint8_t REG_DAC0 = 0x08;

static const uint16_t CONFIG_REF_POWER_DOWN = 0x0100;
static const uint16_t TRIGGER_SOFT_RESET = 0x000A;
static const uint16_t SYNC_ASYNC_ALL = 0xFF00;
static const uint16_t SYNC_LDAC_ALL = 0xFF0F;
static const uint16_t GAIN_REFDIV_2 = 0x0100;

void DACX0504::setup() {
  this->spi_setup();

  if (this->ldac_pin_ != nullptr) {
    this->ldac_pin_->setup();
    this->ldac_pin_->digital_write(true);
  }

  this->write_register_(REG_TRIGGER, TRIGGER_SOFT_RESET);
  delay(1);
  // With LDAC wired, use synchronous mode so the pin controls active DAC updates.
  this->write_register_(REG_SYNC, this->ldac_pin_ != nullptr ? SYNC_LDAC_ALL : SYNC_ASYNC_ALL);
  this->write_register_(REG_CONFIG, this->reference_ == DACX0504_EXTERNAL_REFERENCE ? CONFIG_REF_POWER_DOWN : 0x0000);
  this->write_register_(REG_GAIN, this->gain_register_());
}

void DACX0504::dump_config() {
  ESP_LOGCONFIG(TAG, "DACx0504:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  LDAC Pin: ", this->ldac_pin_);
  ESP_LOGCONFIG(TAG, "  Resolution: %u bit", static_cast<unsigned>(this->model_));
  ESP_LOGCONFIG(TAG, "  Reference: %s", this->reference_ == DACX0504_INTERNAL_REFERENCE ? "internal" : "external");
  ESP_LOGCONFIG(TAG, "  Reference divider: %u", this->reference_divider_);
  ESP_LOGCONFIG(TAG, "  Output gain: %u", this->gain_);
}

void DACX0504::write_register_(uint8_t reg, uint16_t value) {
  this->enable();
  this->transfer_byte(reg & 0x7F);
  this->transfer_byte(value >> 8);
  this->transfer_byte(value & 0xFF);
  this->disable();
}

void DACX0504::pulse_ldac_() {
  if (this->ldac_pin_ == nullptr) {
    return;
  }
  this->ldac_pin_->digital_write(false);
  delayMicroseconds(1);
  this->ldac_pin_->digital_write(true);
  delayMicroseconds(1);
}

uint16_t DACX0504::scale_state_(float state) const {
  if (!std::isfinite(state)) {
    state = 0.0f;
  }
  state = std::clamp(state, 0.0f, 1.0f);
  const uint8_t resolution = static_cast<uint8_t>(this->model_);
  const uint32_t max_code = (1UL << resolution) - 1UL;
  const uint32_t code = static_cast<uint32_t>(std::lround(state * max_code));
  return static_cast<uint16_t>(code << (16 - resolution));
}

uint16_t DACX0504::gain_register_() const {
  uint16_t value = this->reference_divider_ == 2 ? GAIN_REFDIV_2 : 0x0000;
  if (this->gain_ == 2) {
    value |= 0x000F;
  }
  return value;
}

void DACX0504::set_channel_value(uint8_t channel, float state) {
  if (channel > 3) {
    return;
  }
  this->write_register_(REG_DAC0 + channel, this->scale_state_(state));
  this->pulse_ldac_();
}

void DACX0504Channel::write_state(float state) {
  if (this->parent_ != nullptr) {
    this->parent_->set_channel_value(this->channel_, state);
  }
}

}  // namespace dacx0504
}  // namespace esphome
