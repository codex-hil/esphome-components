#include "max6966.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace max6966 {

static const char *const TAG = "max6966";

void MAX6966Component::setup() {
  this->spi_setup();

  // Initialize the device
  // 1. Set all ports to off (high impedance) initially
  for (uint8_t i = 0; i < this->num_ports_; i++) {
    this->port_values_[i] = MAX6966_PORT_OFF;
  }
  this->write_register(MAX6966_REG_ALL_PORTS, MAX6966_PORT_OFF);

  // 2. Configure output currents (all full current by default)
  this->write_register(MAX6966_REG_CURRENT_OUT70, this->current_out70_);
  this->write_register(MAX6966_REG_CURRENT_OUT98, this->current_out98_);

  // 3. Set global current to maximum (20mA/10mA)
  this->write_register(MAX6966_REG_GLOBAL_CURRENT, this->global_current_);

  // 4. Configure device (run mode, no stagger, internal oscillator)
  this->write_register(MAX6966_REG_CONFIGURATION, this->configuration_);

  ESP_LOGCONFIG(TAG, "MAX6966 initialized successfully");
}

void MAX6966Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX6966:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Number of Ports: %d", this->num_ports_);
  ESP_LOGCONFIG(TAG, "  Global Current: %d (%.1fmA full / %.1fmA half)", this->global_current_,
                2.5f * (this->global_current_ + 1), 1.25f * (this->global_current_ + 1));
}

void MAX6966Component::set_port(uint8_t port, uint8_t value) {
  if (port >= this->num_ports_) {
    ESP_LOGW(TAG, "Invalid port number: %d", port);
    return;
  }

  this->port_values_[port] = value;
  this->write_register(MAX6966_REG_P0 + port, value);
}

void MAX6966Component::set_port_current(uint8_t port, bool full_current) {
  if (port >= this->num_ports_) {
    ESP_LOGW(TAG, "Invalid port number: %d", port);
    return;
  }

  if (port < 8) {
    // Ports 0-7 in CURRENT_OUT70 register
    if (full_current) {
      this->current_out70_ |= (1 << port);
    } else {
      this->current_out70_ &= ~(1 << port);
    }
    this->write_register(MAX6966_REG_CURRENT_OUT70, this->current_out70_);
  } else {
    // Ports 8-9 in CURRENT_OUT98 register
    uint8_t bit = port - 8;
    if (full_current) {
      this->current_out98_ |= (1 << bit);
    } else {
      this->current_out98_ &= ~(1 << bit);
    }
    this->write_register(MAX6966_REG_CURRENT_OUT98, this->current_out98_);
  }
}

void MAX6966Component::set_global_current(uint8_t current) {
  if (current > 7) {
    ESP_LOGW(TAG, "Invalid global current: %d (max 7)", current);
    return;
  }

  this->global_current_ = current;
  this->write_register(MAX6966_REG_GLOBAL_CURRENT, this->global_current_);
}

void MAX6966Component::set_pwm_stagger(bool enable) {
  if (enable) {
    this->configuration_ |= MAX6966_CFG_PWM_STAGGER;
  } else {
    this->configuration_ &= ~MAX6966_CFG_PWM_STAGGER;
  }
  this->write_register(MAX6966_REG_CONFIGURATION, this->configuration_);
}

void MAX6966Component::set_configuration(uint8_t config) {
  this->configuration_ = config;
  this->write_register(MAX6966_REG_CONFIGURATION, this->configuration_);
}

uint8_t MAX6966Component::read_register(uint8_t reg) {
  // SPI protocol: send R/W bit (1 for read) + 6-bit address + 8-bit data
  // Read: send 0x80 | address to indicate read operation
  this->enable();
  uint8_t tx_data[2] = {static_cast<uint8_t>(0x80 | reg), 0x00};
  uint8_t rx_data[2] = {0, 0};

  this->write_array(tx_data, 2);
  // Data is available after 16 clock cycles (next transaction)
  this->disable();

  // Read the data by sending another dummy transaction
  this->enable();
  this->write_byte(0x80 | reg);
  rx_data[1] = this->read_byte();
  this->disable();

  return rx_data[1];
}

void MAX6966Component::write_register(uint8_t reg, uint8_t value) {
  // SPI protocol: send R/W bit (0 for write) + 6-bit address + 8-bit data
  // The address is 6 bits, so we use bits 0-5 of the register
  // Bit 6 is unused, bit 7 is R/W (0 for write, 1 for read)
  this->enable();
  uint8_t tx_data[2] = {static_cast<uint8_t>(reg & 0x7F), value};
  this->write_array(tx_data, 2);
  this->disable();
}

uint8_t MAX6966Component::read_input_ports_p0_p7() { return this->read_register(MAX6966_REG_INPUT_P0_P7); }

uint8_t MAX6966Component::read_input_ports_p8_p9() { return this->read_register(MAX6966_REG_INPUT_P8_P9) & 0x03; }

bool MAX6966Component::read_input_port(uint8_t port) {
  if (port >= this->num_ports_) {
    ESP_LOGW(TAG, "Invalid port number: %d", port);
    return false;
  }

  if (port < 8) {
    uint8_t value = this->read_input_ports_p0_p7();
    return (value & (1 << port)) != 0;
  } else {
    uint8_t value = this->read_input_ports_p8_p9();
    return (value & (1 << (port - 8))) != 0;
  }
}

}  // namespace max6966
}  // namespace esphome
