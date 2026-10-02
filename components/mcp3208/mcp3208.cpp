#include "mcp3208.h"
#include <cmath>

#include "esphome/core/log.h"

namespace esphome {
namespace mcp3208 {

static const char *const TAG = "mcp3208";

void MCP3208::setup() {
  this->spi_setup();
  if (!this->spi_is_ready()) this->mark_failed();
  this->update_loop_request_();
}
void MCP3208::loop() {
  if (this->is_failed() || sample_period_us_ == 0 || requested_mask_ == 0) return;
  const uint32_t now = micros();
  if (now - last_sample_us_ < sample_period_us_) return;
  last_sample_us_ = now;
  for (uint8_t i = 0; i < 16; i++) {
    const uint8_t index = next_channel_;
    next_channel_ = (next_channel_ + 1) & 15;
    if (requested_mask_ & (1U << index)) {
      cached_raw_[index] = this->read_raw_(index & 7, index >= 8);
      valid_mask_ |= 1U << index;
      break;
    }
  }
}

void MCP3208::dump_config() {
  ESP_LOGCONFIG(TAG, "MCP3208:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Reference voltage: %.3f V", this->reference_voltage_);
}

uint16_t MCP3208::read_raw_(uint8_t channel, bool differential) {
  channel &= 0x07;
  const uint8_t command0 = 0x04 | (differential ? 0x00 : 0x02) | ((channel & 0x04) >> 2);
  const uint8_t command1 = (channel & 0x03) << 6;

  uint8_t frame[3] = {command0, command1, 0x00};
  this->enable();
  this->transfer_array(frame, sizeof(frame));
  this->disable();
  return (static_cast<uint16_t>(frame[1] & 0x0F) << 8) | frame[2];
}

float MCP3208::read_voltage(uint8_t channel, bool differential) {
  if (!this->spi_is_ready() || this->is_failed()) return NAN;
  const uint8_t index = (channel & 7) + (differential ? 8 : 0);
  if (sample_period_us_ != 0 && !(valid_mask_ & (1U << index))) return NAN;
  const uint16_t raw = sample_period_us_ != 0 ? cached_raw_[index] : this->read_raw_(channel, differential);
  return (static_cast<float>(raw) * this->reference_voltage_) / 4096.0f;
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
