#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

namespace esphome {
namespace dacx0504 {

enum DACX0504Model : uint8_t {
  DAC80504_MODEL = 16,
  DAC70504_MODEL = 14,
  DAC60504_MODEL = 12,
};

enum DACX0504Reference : uint8_t {
  DACX0504_INTERNAL_REFERENCE = 0,
  DACX0504_EXTERNAL_REFERENCE = 1,
};

class DACX0504 : public Component,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                       spi::DATA_RATE_10MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_model(DACX0504Model model) { this->model_ = model; }
  void set_reference(DACX0504Reference reference) { this->reference_ = reference; }
  void set_reference_divider(uint8_t divider) { this->reference_divider_ = divider; }
  void set_gain(uint8_t gain) { this->gain_ = gain; }
  void set_ldac_pin(GPIOPin *pin) { this->ldac_pin_ = pin; }

  void set_channel_value(uint8_t channel, float state);
  void set_channel_voltage(uint8_t channel, float voltage);
  void set_reference_voltage(float voltage) { reference_voltage_ = voltage; }
  void set_fast_sdo(bool enabled) { fast_sdo_ = enabled; }
  void set_verify_registers(bool enabled) { verify_registers_ = enabled; }
  void set_synchronous_update(bool enabled) { synchronous_update_ = enabled; }
  void set_initial_values(std::vector<float> values) { initial_values_ = std::move(values); }
  // In synchronous mode, setters stage codes; call commit() to update together.
  void commit();
  uint16_t read_register(uint8_t reg);
  float full_scale_voltage() const { return reference_voltage_ * gain_ / reference_divider_; }

 protected:
  void write_register_(uint8_t reg, uint16_t value);
  void pulse_ldac_();
  uint16_t scale_state_(float state) const;
  uint16_t gain_register_() const;

  DACX0504Model model_{DAC80504_MODEL};
  DACX0504Reference reference_{DACX0504_INTERNAL_REFERENCE};
  uint8_t reference_divider_{1};
  uint8_t gain_{1};
  GPIOPin *ldac_pin_{nullptr};
  float reference_voltage_{2.5f};
  bool fast_sdo_{false};
  bool verify_registers_{false};
  bool synchronous_update_{false};
  std::vector<float> initial_values_;
};

class DACX0504Channel : public output::FloatOutput, public Component {
 public:
  void set_parent(DACX0504 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void set_voltage_range(float minimum, float maximum) {
    minimum_voltage_ = minimum; maximum_voltage_ = maximum; voltage_scaled_ = true;
  }
  void setup() override {}
  void write_state(float state) override;

 protected:
  DACX0504 *parent_{nullptr};
  uint8_t channel_{0};
  bool voltage_scaled_{false};
  float minimum_voltage_{0}, maximum_voltage_{0};
};

}  // namespace dacx0504
}  // namespace esphome
