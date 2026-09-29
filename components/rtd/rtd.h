// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::rtd {
class RTDSensor : public Component, public sensor::Sensor {
 public:
  void set_sensor(sensor::Sensor *source) { this->source_ = source; }
  void set_nominal_resistance(float resistance) { this->nominal_resistance_ = resistance; }
  void setup() override;
  void dump_config() override;

 protected:
  void process_(float resistance);
  sensor::Sensor *source_{nullptr};
  float nominal_resistance_{100.0f};
};
}  // namespace esphome::rtd
