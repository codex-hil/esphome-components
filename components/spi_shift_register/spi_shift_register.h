#pragma once
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/binary_output.h"
#include <vector>

namespace esphome::spi_shift_register {
enum Model : uint8_t { HC165, HC595 };
class SPIShiftRegister : public PollingComponent,
  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                       spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_model(Model model) { model_ = model; }
  void set_load_pulse_verified(bool value) { load_pulse_verified_ = value; }
  void set_initial_value(uint8_t value) { value_ = value; }
  void register_input(uint8_t bit, binary_sensor::BinarySensor *sensor) { inputs_.push_back({bit, sensor}); }
  void set_bit(uint8_t bit, bool value);
  void write_byte_value(uint8_t value);
 protected:
  struct Input { uint8_t bit; binary_sensor::BinarySensor *sensor; };
  std::vector<Input> inputs_;
  Model model_{HC165};
  bool load_pulse_verified_{false};
  uint8_t value_{0xFF};
};
class SPIShiftOutput : public output::BinaryOutput {
 public:
  void set_parent(SPIShiftRegister *parent) { parent_ = parent; }
  void set_channel(uint8_t channel) { channel_ = channel; }
 protected:
  void write_state(bool state) override { parent_->set_bit(channel_, state); }
  SPIShiftRegister *parent_{nullptr};
  uint8_t channel_{0};
};
}  // namespace esphome::spi_shift_register
