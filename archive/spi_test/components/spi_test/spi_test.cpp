#include "spi_test.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spi_test {

static const char *const TAG = "spi_test";

void SPITestComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPI Test Component...");
  this->spi_setup();
  ESP_LOGCONFIG(TAG, "SPI Test Component initialized");
}

void SPITestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SPI Test Component:");
  LOG_PIN("  CS Pin: ", this->cs_);
}

uint8_t SPITestComponent::read_register(uint8_t reg) {
  ESP_LOGD(TAG, "SPI Test: Reading register 0x%02X", reg);

  this->enable();
  uint8_t cmd = (reg << 2) | 0x01;  // Address + read bit
  this->write_byte(cmd);
  uint8_t value = this->read_byte();
  this->disable();

  ESP_LOGD(TAG, "SPI Test: Read 0x%02X from register 0x%02X", value, reg);
  return value;
}

void SPITestComponent::write_register(uint8_t reg, uint8_t value) {
  ESP_LOGD(TAG, "SPI Test: Writing 0x%02X to register 0x%02X", value, reg);

  this->enable();
  uint8_t cmd = (reg << 2) | 0x00;  // Address + write bit
  this->write_byte(cmd);
  this->write_byte(value);
  this->disable();

  ESP_LOGD(TAG, "SPI Test: Write completed");
}

}  // namespace spi_test
}  // namespace esphome
