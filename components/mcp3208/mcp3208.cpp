#include "mcp3208.h"

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3208 {

static const char *const TAG = "mcp3208";

void MCP3208::setup() { this->spi_setup(); }

void MCP3208::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3208:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Reference voltage: %.3f V", this->reference_voltage_);
}

uint16_t MCP3208::read_raw_(uint8_t channel, bool differential) {
  channel &= 0x07;
  const uint8_t command0 = 0x04 | (differential ? 0x00 : 0x02) | ((channel & 0x04) >> 2);
  const uint8_t command1 = (channel & 0x03) << 6;

  this->enable();
  this->transfer_byte(command0);
  const uint8_t high = this->transfer_byte(command1);
  const uint8_t low = this->transfer_byte(0x00);
  this->disable();

  return (static_cast<uint16_t>(high & 0x0F) << 8) | low;
}

float MCP3208::read_voltage(uint8_t channel, bool differential) {
  const uint16_t raw = this->read_raw_(channel, differential);
  return (static_cast<float>(raw) * this->reference_voltage_) / 4095.0f;
}

void MCP3208Sensor::dump_config() {
  LOG_SENSOR("", "MCP3208 Sensor", this);
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Differential: %s", YESNO(this->differential_));
  LOG_UPDATE_INTERVAL(this);
}

void MCP3208Sensor::update() {
  if (this->parent_ == nullptr) {
    this->publish_state(NAN);
    return;
  }
  this->publish_state(this->parent_->read_voltage(this->channel_, this->differential_));
}

}  // namespace mcp3208
}  // namespace esphome
