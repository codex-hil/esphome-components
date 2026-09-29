#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace spi_test {

class SPITestComponent : public Component,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  /// Read a register
  uint8_t read_register(uint8_t reg);

  /// Write a register
  void write_register(uint8_t reg, uint8_t value);
};

}  // namespace spi_test
}  // namespace esphome
