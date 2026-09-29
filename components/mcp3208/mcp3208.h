#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mcp3208 {

class MCP3208 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reference_voltage(float voltage) { this->reference_voltage_ = voltage; }
  float read_voltage(uint8_t channel, bool differential);

 protected:
  uint16_t read_raw_(uint8_t channel, bool differential);

  float reference_voltage_{3.3f};
};

class MCP3208Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(MCP3208 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_differential(bool differential) { this->differential_ = differential; }

  void dump_config() override;
  void update() override;

 protected:
  MCP3208 *parent_{nullptr};
  uint8_t channel_{0};
  bool differential_{false};
};

}  // namespace mcp3208
}  // namespace esphome
