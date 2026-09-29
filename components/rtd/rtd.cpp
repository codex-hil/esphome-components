// SPDX-License-Identifier: GPL-3.0-only
#include "rtd.h"
#include "rtd_math.h"
#include "esphome/core/log.h"

namespace esphome::rtd {
static const char *const TAG = "rtd";

void RTDSensor::setup() {
  this->source_->add_on_state_callback([this](float resistance) { this->process_(resistance); });
  if (this->source_->has_state())
    this->process_(this->source_->state);
}

void RTDSensor::process_(float resistance) {
  const float temperature = temperature_celsius(resistance, this->nominal_resistance_);
  if (!std::isfinite(temperature))
    this->status_set_warning();
  else
    this->status_clear_warning();
  this->publish_state(temperature);
}

void RTDSensor::dump_config() {
  LOG_SENSOR("", "RTD Sensor", this);
  LOG_SENSOR("  ", "Resistance source (ohms)", this->source_);
  ESP_LOGCONFIG(TAG, "  Nominal resistance: %.0f Ohm; IEC 60751 alpha=0.00385; -200..850 C",
                this->nominal_resistance_);
}
}  // namespace esphome::rtd
