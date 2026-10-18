#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/select/select.h"
#include <string>
#include <vector>

namespace esphome::switch_matrix_32_2 {
class MatrixSelect;
class Matrix : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                     spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_cs_a_pin(GPIOPin *pin) { cs_a_ = pin; }
  void set_cs_b_pin(GPIOPin *pin) { cs_b_ = pin; }
  void set_oe_pin(GPIOPin *pin) { oe_ = pin; }
  void set_break_before_make(uint32_t ms) { break_ms_ = ms; }
  void register_select(MatrixSelect *entity) { selects_.push_back(entity); }
  bool set_masks(uint32_t a, uint32_t b);
  bool set_masks(const std::string &a, const std::string &b);
  bool set_channel(uint8_t channel, const std::string &option);
  static bool parse_mask(const std::string &text, uint32_t &value);
 protected:
  void write_bank_(GPIOPin *latch, uint32_t mask);
  void write_masks_(uint32_t a, uint32_t b);
  void publish_();
  GPIOPin *cs_a_{nullptr}, *cs_b_{nullptr}, *oe_{nullptr};
  uint32_t actual_a_{0}, actual_b_{0}, desired_a_{0}, desired_b_{0};
  uint32_t break_ms_{10};
  bool ready_{false};
  std::vector<MatrixSelect *> selects_;
};

class MatrixSelect : public select::Select {
 public:
  void set_parent(Matrix *parent) { parent_ = parent; }
  void set_channel(uint8_t channel) { channel_ = channel; }
  uint8_t channel() const { return channel_; }
 protected:
  void control(const std::string &value) override { parent_->set_channel(channel_, value); }
  Matrix *parent_{nullptr};
  uint8_t channel_{1};
};

template<typename... Ts> class SetMasksAction : public Action<Ts...> {
 public:
  explicit SetMasksAction(Matrix *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(std::string, mask_a)
  TEMPLATABLE_VALUE(std::string, mask_b)
  void play(const Ts &...x) override { parent_->set_masks(mask_a_.value(x...), mask_b_.value(x...)); }
 protected:
  Matrix *parent_;
};
template<typename... Ts> class BypassAllAction : public Action<Ts...> {
 public:
  explicit BypassAllAction(Matrix *parent) : parent_(parent) {}
  void play(const Ts &...x) override { parent_->set_masks(uint32_t(0), uint32_t(0)); }
 protected:
  Matrix *parent_;
};
}  // namespace esphome::switch_matrix_32_2
