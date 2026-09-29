#include "output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace max6966 {

static const char *const TAG = "max6966.output";

void MAX6966Output::setup() {
  this->parent_->set_port_current(this->channel_, this->full_current_);

  this->turn_off();
}

void MAX6966Output::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX6966 Output:");
  ESP_LOGCONFIG(TAG, "  Channel: %d", this->channel_);
  ESP_LOGCONFIG(TAG, "  Current: %s", this->full_current_ ? "full (20mA)" : "half (10mA)");
  ESP_LOGCONFIG(TAG, "  Open Drain: %s", this->open_drain_ ? "yes" : "no");
}

void MAX6966Output::write_state(float state) {
  if (this->parent_ == nullptr) {
    return;
  }

  // Convert float (0.0-1.0) to PWM value (0x00-0xFF)
  uint8_t value;

  if (state <= 0.0f) {
    // Turn off
    if (this->open_drain_) {
      // Open drain: use high-Z state (0xFF)
      value = MAX6966_PORT_OFF;
    } else {
      // Constant current: use logic low (0x00)
      value = MAX6966_PORT_LOGIC_LOW;
    }
  } else if (state >= 1.0f) {
    // Full brightness
    if (this->open_drain_) {
      // Open drain: use logic high (0x01) - requires external pullup
      value = MAX6966_PORT_LOGIC_HIGH;
    } else {
      // Constant current: use constant current static (0x02)
      value = MAX6966_PORT_CONSTANT_CURRENT;
    }
  } else {
    // PWM mode - map 0.0-1.0 to appropriate PWM range
    if (this->open_drain_) {
      // For open drain, use 0x01-0xFE range (logic high to PWM)
      uint16_t pwm_value = static_cast<uint16_t>(state * 253.0f) + 1;
      value = static_cast<uint8_t>(pwm_value);
    } else {
      // For constant current, use 0x02-0xFE range
      uint16_t pwm_value = static_cast<uint16_t>(state * 252.0f) + 2;
      value = static_cast<uint8_t>(pwm_value);
    }
  }

  this->parent_->set_port(this->channel_, value);
}

}  // namespace max6966
}  // namespace esphome
