#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads8166 {

class ADS8166 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_10MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reference_voltage(float reference_voltage) { this->reference_voltage_ = reference_voltage; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

  float read_voltage(uint8_t channel);

 protected:
  void reset_();
  uint32_t transfer_frame_(uint32_t frame);
  void write_register_(uint16_t address, uint8_t value);
  uint8_t read_register_(uint16_t address);
  uint16_t read_conversion_();

  float reference_voltage_{4.096f};
  GPIOPin *reset_pin_{nullptr};
};

class ADS8166Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(ADS8166 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void dump_config() override;
  void update() override;

 protected:
  ADS8166 *parent_{nullptr};
  uint8_t channel_{0};
};

}  // namespace ads8166
}  // namespace esphome
