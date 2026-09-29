#include "ads8166.h"

#include "esphome/core/log.h"

namespace esphome {
namespace ads8166 {

static const char *const TAG = "ads8166";

static const uint8_t CMD_NOP = 0x00;
static const uint8_t CMD_WRITE_REGISTER = 0x01;
static const uint8_t CMD_READ_REGISTER = 0x02;

static const uint16_t REG_ACCESS = 0x00;
static const uint16_t DATA_CNTL = 0x10;
static const uint16_t DEVICE_CFG = 0x1C;
static const uint16_t CHANNEL_ID = 0x1D;

static const uint8_t REG_ACCESS_UNLOCK = 0xAA;
static const uint8_t DEVICE_CFG_MANUAL_MODE = 0x00;

void ADS8166::setup() {
  this->spi_setup();
  this->reset_();

  this->write_register_(REG_ACCESS, REG_ACCESS_UNLOCK);
  this->write_register_(DATA_CNTL, 0x00);  // 16-bit ADC result only, parity disabled.
  this->write_register_(DEVICE_CFG, DEVICE_CFG_MANUAL_MODE);
}

void ADS8166::dump_config() {
  ESP_LOGCONFIG(TAG, "ADS8166:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  ESP_LOGCONFIG(TAG, "  Reference voltage: %.3f V", this->reference_voltage_);
}

void ADS8166::reset_() {
  if (this->reset_pin_ == nullptr) {
    return;
  }
  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);
  delay(1);
  this->reset_pin_->digital_write(true);
  // RST rising to READY rising can take up to 4 ms; wait before register writes.
  delay(4);
}

uint32_t ADS8166::transfer_frame_(uint32_t frame) {
  this->enable();
  const uint8_t b2 = this->transfer_byte((frame >> 16) & 0xFF);
  const uint8_t b1 = this->transfer_byte((frame >> 8) & 0xFF);
  const uint8_t b0 = this->transfer_byte(frame & 0xFF);
  this->disable();
  return (static_cast<uint32_t>(b2) << 16) | (static_cast<uint32_t>(b1) << 8) | b0;
}

void ADS8166::write_register_(uint16_t address, uint8_t value) {
  const uint32_t frame =
      (static_cast<uint32_t>(CMD_WRITE_REGISTER) << 19) | ((static_cast<uint32_t>(address) & 0x07FF) << 8) | value;
  this->transfer_frame_(frame);
}

uint8_t ADS8166::read_register_(uint16_t address) {
  const uint32_t frame =
      (static_cast<uint32_t>(CMD_READ_REGISTER) << 19) | ((static_cast<uint32_t>(address) & 0x07FF) << 8);
  this->transfer_frame_(frame);
  return (this->transfer_frame_(0) >> 16) & 0xFF;
}

uint16_t ADS8166::read_conversion_() {
  return (this->transfer_frame_(static_cast<uint32_t>(CMD_NOP) << 19) >> 8) & 0xFFFF;
}

float ADS8166::read_voltage(uint8_t channel) {
  channel &= 0x07;
  this->write_register_(DEVICE_CFG, DEVICE_CFG_MANUAL_MODE);
  this->write_register_(CHANNEL_ID, channel);

  // Manual channel changes have pipeline latency; discard two frames after selecting a new channel.
  this->read_conversion_();
  this->read_conversion_();
  const uint16_t raw = this->read_conversion_();
  return (static_cast<float>(raw) * this->reference_voltage_) / 65535.0f;
}

void ADS8166Sensor::dump_config() {
  LOG_SENSOR("", "ADS8166 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  LOG_UPDATE_INTERVAL(this);
}

void ADS8166Sensor::update() {
  if (this->parent_ == nullptr) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(this->parent_->read_voltage(this->channel_));
}

}  // namespace ads8166
}  // namespace esphome
