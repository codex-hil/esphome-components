#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c_bus.h"
#include "esphome/components/moduliq_cpld_gpio/moduliq_cpld_gpio.h"

namespace esphome::moduliq_cpld_soft_i2c {
class CPLDSoftI2C : public Component, public i2c::I2CBus {
 public:
  void set_parent(moduliq_cpld_gpio::CPLDGPIO *parent) { parent_ = parent; }
  void set_pins(uint8_t sda, uint8_t scl) { sda_ = sda; scl_ = scl; }
  void set_frequency(uint32_t hz) { half_period_us_ = (500000U + hz - 1) / hz; }
  void set_stretch_timeout(uint32_t us) { stretch_timeout_us_ = us; }
  void set_scan(bool scan) { scan_requested_ = scan; }
  float get_setup_priority() const override { return setup_priority::IO + 10; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *tx, size_t tx_len,
                           uint8_t *rx, size_t rx_len) override;
 protected:
  bool initialize_();
  bool levels_(bool sda, bool scl);
  bool sample_(bool &sda, bool &scl);
  bool raise_scl_(bool sda);
  bool start_();
  bool stop_();
  bool recover_();
  bool write_bit_(bool bit);
  bool read_bit_(bool &bit);
  bool write_byte_(uint8_t byte);
  bool read_byte_(uint8_t &byte, bool last);
  void pause_();
  moduliq_cpld_gpio::CPLDGPIO *parent_{nullptr};
  uint8_t sda_{0}, scl_{1};
  uint16_t mask_{0};
  uint32_t half_period_us_{500}, stretch_timeout_us_{10000};
  uint8_t scan_address_{8};
  uint32_t parent_generation_{0}, transaction_start_{0};
  bool in_transaction_{false};
  bool reserved_{false}, initialized_{false}, scan_requested_{false}, scanned_{false};
  i2c::ErrorCode error_{i2c::ERROR_OK};
};
}
