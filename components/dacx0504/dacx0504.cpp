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
static const uint16_t SYNC_ASYNC_ALL = 0x0F00;
static const uint16_t SYNC_LDAC_ALL = 0x0F0F;
static const uint16_t GAIN_REFDIV_2 = 0x0100;

void DACX0504::setup() {
  this->spi_setup();

  if (this->ldac_pin_ != nullptr) {
    this->ldac_pin_->setup();
    this->ldac_pin_->digital_write(true);
  }

  if (!this->spi_is_ready()) { this->mark_failed(); return; }
  // Do not soft-reset: with an external reference physically connected, reset
  // would enable the internal reference again and connect two sources.
  const uint16_t config = (this->reference_ == DACX0504_EXTERNAL_REFERENCE ? CONFIG_REF_POWER_DOWN : 0) |
                          (this->fast_sdo_ ? 0x0400 : 0);
  this->write_register_(REG_CONFIG, config);
  this->write_register_(REG_GAIN, this->gain_register_());
  this->write_register_(REG_SYNC, (this->synchronous_update_ || this->ldac_pin_ != nullptr) ?
                                SYNC_LDAC_ALL : SYNC_ASYNC_ALL);
  if (this->verify_registers_) {
    const uint16_t device = this->read_register(0x01);
    const uint8_t resolution_id = (device >> 12) & 7;
    const uint8_t expected_id = (16 - static_cast<uint8_t>(this->model_)) / 2;
    if (resolution_id != expected_id || (device & 0x0F00) != 0x0400 ||
        this->read_register(REG_CONFIG) != config ||
        this->read_register(REG_GAIN) != this->gain_register_() || (this->read_register(0x07) & 1)) {
      ESP_LOGE(TAG, "Device/register verification or reference alarm failed");
      this->mark_failed();
      return;
    }
  }
  for (size_t i = 0; i < this->initial_values_.size(); i++)
    this->set_channel_value(i, this->initial_values_[i]);
  if (this->synchronous_update_) this->commit();
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
  const uint8_t frame[] = {static_cast<uint8_t>(reg & 0x7F),
                           static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  this->enable();
  this->write_array(frame, sizeof(frame));
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
  if (channel > 3 || this->is_failed() || !this->spi_is_ready()) {
    return;
  }
  this->write_register_(REG_DAC0 + channel, this->scale_state_(state));
  if (!this->synchronous_update_) this->pulse_ldac_();
}

uint16_t DACX0504::read_register(uint8_t reg) {
  uint8_t frame[] = {static_cast<uint8_t>(0x80 | (reg & 0x0F)), 0, 0};
  this->enable(); this->write_array(frame, sizeof(frame)); this->disable();
  uint8_t response[3] = {};
  this->enable(); this->transfer_array(response, sizeof(response)); this->disable();
  if (response[0] != frame[0]) { this->status_set_warning(); return 0xFFFF; }
  return (static_cast<uint16_t>(response[1]) << 8) | response[2];
}
void DACX0504::set_channel_voltage(uint8_t channel, float voltage) {
  if (channel > 3 || this->is_failed() || !this->spi_is_ready() || !std::isfinite(voltage)) return;
  const uint8_t resolution = static_cast<uint8_t>(this->model_);
  const uint32_t steps = 1UL << resolution;
  const float ratio = std::clamp(voltage / this->full_scale_voltage(), 0.0f, 1.0f);
  const uint32_t code = std::min(static_cast<uint32_t>(std::lround(ratio * steps)), steps - 1);
  this->write_register_(REG_DAC0 + channel, code << (16 - resolution));
  if (!this->synchronous_update_) this->pulse_ldac_();
}
void DACX0504::commit() {
  if (this->is_failed() || !this->spi_is_ready()) return;
  if (this->ldac_pin_ != nullptr) this->pulse_ldac_();
  else this->write_register_(REG_TRIGGER, 0x0010);
}
void DACX0504Channel::write_state(float state) {
  if (this->parent_ != nullptr) {
    if (this->voltage_scaled_)
      this->parent_->set_channel_voltage(this->channel_, this->minimum_voltage_ +
          state * (this->maximum_voltage_ - this->minimum_voltage_));
    else this->parent_->set_channel_value(this->channel_, state);
  }
}

}  // namespace dacx0504
}  // namespace esphome
