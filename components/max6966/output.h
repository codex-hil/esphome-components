#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "max6966.h"

namespace esphome {
namespace max6966 {

class MAX6966Output : public output::FloatOutput, public Component {
 public:
  void set_parent(MAX6966Component *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_full_current(bool full_current) { this->full_current_ = full_current; }
  void set_open_drain(bool open_drain) { this->open_drain_ = open_drain; }

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(float state) override;

  MAX6966Component *parent_{nullptr};
  uint8_t channel_{0};
  bool full_current_{true};
  bool open_drain_{false};
};

}  // namespace max6966
}  // namespace esphome
