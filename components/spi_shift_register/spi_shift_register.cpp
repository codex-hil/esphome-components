#include "spi_shift_register.h"
#include "esphome/core/log.h"

namespace esphome::spi_shift_register {
static const char *const TAG = "spi_shift_register";
void SPIShiftRegister::setup() {
  this->spi_setup();
  if (!this->spi_is_ready()) { this->mark_failed(); return; }
  if (model_ == HC165 && !load_pulse_verified_) {
    ESP_LOGE(TAG, "HC165 needs hardware /PL LOW load then HIGH throughout shift; readout disabled");
    this->mark_failed();
    return;
  }
  if (model_ == HC595) this->write_byte_value(value_);
}
void SPIShiftRegister::update() {
  if (model_ != HC165 || this->is_failed() || !this->spi_is_ready()) return;
  uint8_t frame = 0;
  this->enable();
  this->transfer_array(&frame, 1);
  this->disable();
  value_ = frame;
  for (auto &input : inputs_) input.sensor->publish_state((frame >> input.bit) & 1);
}
void SPIShiftRegister::write_byte_value(uint8_t value) {
  if (model_ != HC595 || this->is_failed() || !this->spi_is_ready()) return;
  this->enable();
  this->write_array(&value, 1);
  this->disable();
  value_ = value;
}
void SPIShiftRegister::set_bit(uint8_t bit, bool state) {
  if (bit > 7) return;
  const uint8_t mask = 1U << bit;
  this->write_byte_value(state ? value_ | mask : value_ & ~mask);
}
void SPIShiftRegister::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI Shift Register %s", model_ == HC165 ? "HC165" : "HC595");
  LOG_PIN("  Physical CS (or serial router owns CS): ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}
}  // namespace esphome::spi_shift_register
