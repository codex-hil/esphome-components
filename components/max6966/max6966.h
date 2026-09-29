#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max6966 {

// Register addresses
static const uint8_t MAX6966_REG_P0 = 0x00;
static const uint8_t MAX6966_REG_P1 = 0x01;
static const uint8_t MAX6966_REG_P2 = 0x02;
static const uint8_t MAX6966_REG_P3 = 0x03;
static const uint8_t MAX6966_REG_P4 = 0x04;
static const uint8_t MAX6966_REG_P5 = 0x05;
static const uint8_t MAX6966_REG_P6 = 0x06;
static const uint8_t MAX6966_REG_P7 = 0x07;
static const uint8_t MAX6966_REG_P8 = 0x08;
static const uint8_t MAX6966_REG_P9 = 0x09;
static const uint8_t MAX6966_REG_ALL_PORTS = 0x0A;
static const uint8_t MAX6966_REG_P0_P3 = 0x0B;
static const uint8_t MAX6966_REG_P4_P7 = 0x0C;
static const uint8_t MAX6966_REG_P8_P9 = 0x0D;
static const uint8_t MAX6966_REG_INPUT_P0_P7 = 0x0E;
static const uint8_t MAX6966_REG_INPUT_P8_P9 = 0x0F;
static const uint8_t MAX6966_REG_CONFIGURATION = 0x10;
static const uint8_t MAX6966_REG_CURRENT_OUT70 = 0x13;
static const uint8_t MAX6966_REG_CURRENT_OUT98 = 0x14;
static const uint8_t MAX6966_REG_GLOBAL_CURRENT = 0x15;

// Port values
static const uint8_t MAX6966_PORT_LOGIC_LOW = 0x00;
static const uint8_t MAX6966_PORT_LOGIC_HIGH = 0x01;
static const uint8_t MAX6966_PORT_CONSTANT_CURRENT = 0x02;
static const uint8_t MAX6966_PORT_OFF = 0xFF;

// Configuration register bits
static const uint8_t MAX6966_CFG_RUN = 0x01;
static const uint8_t MAX6966_CFG_CS_RUN = 0x02;
static const uint8_t MAX6966_CFG_RAMPUP_ENABLE = 0x04;
static const uint8_t MAX6966_CFG_PWM_STAGGER = 0x20;
static const uint8_t MAX6966_CFG_OSC = 0x80;

class MAX6966Output;

class MAX6966Component : public Component,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  MAX6966Component() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_num_ports(uint8_t num_ports) { this->num_ports_ = num_ports; }

  /// Set a single port's PWM value (0x00-0xFF)
  void set_port(uint8_t port, uint8_t value);

  /// Set a single port's current (half=false, full=true)
  void set_port_current(uint8_t port, bool full_current);

  /// Set global current (0-7)
  void set_global_current(uint8_t current);

  /// Enable or disable PWM staggering
  void set_pwm_stagger(bool enable);

  /// Set configuration register
  void set_configuration(uint8_t config);

  /// Read a register
  uint8_t read_register(uint8_t reg);

  /// Write a register
  void write_register(uint8_t reg, uint8_t value);

  /// Read input ports P0-P7 (returns 8-bit value)
  uint8_t read_input_ports_p0_p7();

  /// Read input ports P8-P9 (returns 2-bit value in lower bits)
  uint8_t read_input_ports_p8_p9();

  /// Read a specific input port (0-9) - returns true if high, false if low
  bool read_input_port(uint8_t port);

  /// Register an output
  void register_output(MAX6966Output *output) { this->outputs_.push_back(output); }

 protected:
  uint8_t num_ports_{10};
  std::vector<MAX6966Output *> outputs_;
  uint8_t port_values_[10]{};
  uint8_t current_out70_{0xFF};             // All ports full current by default
  uint8_t current_out98_{0x03};             // Ports 8-9 full current by default
  uint8_t global_current_{0x07};            // Maximum current (20mA/10mA)
  uint8_t configuration_{MAX6966_CFG_RUN};  // Run mode by default
};

}  // namespace max6966
}  // namespace esphome
