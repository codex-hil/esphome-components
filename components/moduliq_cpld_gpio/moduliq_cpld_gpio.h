#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/spi/spi.h"

namespace esphome::moduliq_cpld_gpio {
class CPLDGPIO : public Component,
    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                         spi::CLOCK_PHASE_LEADING, static_cast<spi::SPIDataRate>(100000)> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void set_input_masks(uint8_t lower, uint8_t upper) { input_mask_[0] = lower; input_mask_[1] = upper; }
  bool configure_pin(uint8_t pin, gpio::Flags flags);
  bool write_pin(uint8_t pin, bool value);
  bool read_pin(uint8_t pin, bool &value);
  bool reserve_upper(const void *owner);
  void release_upper(const void *owner);
  bool upper_reserved() const { return upper_owner_ != nullptr; }
  bool flash_mode() const { return flash_mode_; }
  bool ready() const { return ready_ && !is_failed(); }
  bool acquire_flash(const void *owner);
  bool release_flash(const void *owner);
  bool verify_flash_owner(const void *owner);
  bool has_flash_lease(const void *owner) const { return owner != nullptr && flash_owner_ == owner; }
  // Explicit board-specific control. Does not change DIR/OD or ownership bit.
  bool set_target_controls(uint8_t levels);
  bool configure_target_controls(uint8_t cfg_levels, uint8_t direction, uint8_t open_drain);
 protected:
  uint8_t read_reg_(uint8_t command);
  bool write_reg_(uint8_t command, uint8_t read_command, uint8_t value);
  bool write_bank_(uint8_t bank, uint8_t command, uint8_t read_command, uint8_t value);
  bool update_cfg_(uint8_t mask, uint8_t value);
  bool pin_available_(uint8_t pin);
  uint8_t data_[2]{}, dir_[2]{}, od_[2]{}, input_mask_[2]{};
  uint8_t cfg_{0};
  bool bank_valid_[2]{true, true};
  bool ready_{false}, flash_mode_{false}, cfg_busy_{false};
  const void *upper_owner_{nullptr};
  const void *flash_owner_{nullptr};
};

class CPLDGPIOPin : public GPIOPin {
 public:
  void set_parent(CPLDGPIO *parent) { parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_inverted(bool inverted) { inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { flags_ = flags; }
  void setup() override { pin_mode(flags_); }
  void pin_mode(gpio::Flags flags) override { if (parent_->configure_pin(pin_, flags)) flags_ = flags; }
  bool digital_read() override;
  void digital_write(bool value) override { parent_->write_pin(pin_, value != inverted_); }
  gpio::Flags get_flags() const override { return flags_; }
  size_t dump_summary(char *buffer, size_t len) const override;
 protected:
  CPLDGPIO *parent_{nullptr};
  uint8_t pin_{0};
  bool inverted_{false};
  gpio::Flags flags_{gpio::FLAG_INPUT};
};
}  // namespace esphome::moduliq_cpld_gpio
