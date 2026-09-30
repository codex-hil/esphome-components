#include "moduliq_cpld_gpio.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace esphome::moduliq_cpld_gpio {
static const char *const TAG = "moduliq_cpld_gpio";
uint8_t CPLDGPIO::read_reg_(uint8_t command) {
  enable();
  transfer_byte(command);
  uint8_t value = transfer_byte(0);
  disable();
  return value;
}
bool CPLDGPIO::write_reg_(uint8_t command, uint8_t read_command, uint8_t value) {
  enable(); transfer_byte(command); transfer_byte(value); disable();
  if (read_reg_(read_command) == value) return true;
  ESP_LOGW(TAG, "Register 0x%02X write/readback mismatch", command);
  status_set_warning();
  return false;
}
bool CPLDGPIO::write_bank_(uint8_t bank, uint8_t command, uint8_t read_command, uint8_t value) {
  if (write_reg_(command, read_command, value)) return true;
  bank_valid_[bank] = false;  // Unknown hardware state: refuse further byte updates from a stale shadow.
  return false;
}
void CPLDGPIO::setup() {
  setup_done_ = true;
  if (enabled_) initialize_();
}
bool CPLDGPIO::set_enabled(bool enabled) {
  if (!enabled) {
    if (upper_owner_ || flash_owner_) return false;
    enabled_ = false; ready_ = false;
    return true;  // Disable software access; do not silently change physical pin levels.
  }
  enabled_ = true;
  if (!setup_done_) return true;
  return ready() || initialize_();
}
void CPLDGPIO::register_pin(uint8_t pin, gpio::Flags flags) {
  if (pin > 15) return;
  registered_[pin] = true; registered_flags_[pin] = flags;
  if (ready()) configure_pin(pin, flags);
}
void CPLDGPIO::write_registered_pin(uint8_t pin, bool value) {
  if (pin > 15) return;
  if (!enabled_) {
    pending_data_[pin] = value; pending_data_valid_[pin] = true;
    return;  // Remember YAML initial output state without a bus transaction.
  }
  write_pin(pin, value);
}
bool CPLDGPIO::initialize_() {
  ready_ = false;
  if (!spi_initialized_) { spi_setup(); spi_initialized_ = true; }
  uint8_t status = read_reg_(0x24);
  if ((status & 0xC7) != 0 || (status & 0x10) == 0) {
    ESP_LOGE(TAG, "CS3 local GPIO unavailable or invalid STATUS 0x%02X", status);
    status_set_warning(); return false;
  }
  flash_mode_ = (status & 0x20) != 0;
  data_[0] = read_reg_(0x21); data_[1] = read_reg_(0x22);
  dir_[0] = read_reg_(0x26); dir_[1] = read_reg_(0x27);
  od_[0] = read_reg_(0x28); od_[1] = read_reg_(0x29);
  cfg_ = read_reg_(0x23);
  if (((cfg_ & 1) != 0) != ((status & 8) != 0)) { status_set_warning(); return false; }
  // Repair protected externally driven inputs, preserving all unrelated bits.
  for (uint8_t bank = 0; bank < 2; bank++) {
    uint8_t safe = dir_[bank] & ~input_mask_[bank];
    if (safe != dir_[bank] && !write_reg_(0x13 + bank, 0x26 + bank, safe)) { status_set_warning(); return false; }
    dir_[bank] = safe;
  }
  bank_valid_[0] = bank_valid_[1] = true;
  ready_ = true;
  for (uint8_t pin = 0; pin < 16; pin++) {
    if (registered_[pin] && !configure_pin(pin, registered_flags_[pin],
          pending_data_valid_[pin] ? int(pending_data_[pin]) : -1)) { ready_ = false; return false; }
    pending_data_valid_[pin] = false;
  }
  status_clear_warning();
  return true;
}
void CPLDGPIO::dump_config() {
  ESP_LOGCONFIG(TAG, "CPLD GPIO CS3, mode0; protected masks lower=0x%02X upper=0x%02X", input_mask_[0], input_mask_[1]);
  ESP_LOGCONFIG(TAG, "  Flash pinmux: %s; CFG=0x%02X", flash_mode_ ? "yes" : "no", cfg_);
}
bool CPLDGPIO::pin_available_(uint8_t pin) {
  if (!ready() || pin > 15) return false;
  if (!bank_valid_[pin / 8]) {
    ESP_LOGW(TAG, "GPIO bank state uncertain after write/readback failure; setup required");
    return false;
  }
  if (pin >= 8 && upper_reserved()) {
    ESP_LOGW(TAG, "GPIO %u change rejected: ADC owns upper bank", pin);
    return false;
  }
  if (flash_mode_ && pin >= 1 && pin <= 4) {
    ESP_LOGW(TAG, "GPIO %u reserved by Flash pinmux", pin);
    return false;
  }
  return true;
}
bool CPLDGPIO::configure_pin(uint8_t pin, gpio::Flags flags, int initial_value) {
  if (!pin_available_(pin)) return false;
  uint8_t bank = pin / 8, bit = 1U << (pin % 8);
  bool output = (flags & gpio::FLAG_OUTPUT) != 0;
  if ((flags & ~(gpio::FLAG_INPUT | gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN)) != 0 ||
      output == ((flags & gpio::FLAG_INPUT) != 0) ||
      (!output && (flags & gpio::FLAG_OPEN_DRAIN)) || (output && (input_mask_[bank] & bit))) {
    ESP_LOGW(TAG, "GPIO %u invalid mode or protected input", pin); return false;
  }
  // Disconnect first, then restore latch and OD, then reconnect output.
  uint8_t disconnected = dir_[bank] & ~bit;
  if (!write_bank_(bank, 0x13 + bank, 0x26 + bank, disconnected)) return false;
  dir_[bank] = disconnected;
  uint8_t data = data_[bank];
  if (initial_value >= 0) data = (data & ~bit) | (initial_value ? bit : 0);
  if (!write_bank_(bank, 0x10 + bank, 0x21 + bank, data)) return false;
  data_[bank] = data;
  uint8_t od = (od_[bank] & ~bit) | ((flags & gpio::FLAG_OPEN_DRAIN) ? bit : 0);
  if (!write_bank_(bank, 0x15 + bank, 0x28 + bank, od)) return false;
  od_[bank] = od;
  uint8_t dir = disconnected | (output ? bit : 0);
  if (!write_bank_(bank, 0x13 + bank, 0x26 + bank, dir)) return false;
  dir_[bank] = dir;
  return true;
}
bool CPLDGPIO::write_pin(uint8_t pin, bool value) {
  if (!pin_available_(pin)) return false;
  uint8_t bank = pin / 8, bit = 1U << (pin % 8);
  uint8_t data = (data_[bank] & ~bit) | (value ? bit : 0);
  if (!write_bank_(bank, 0x10 + bank, 0x21 + bank, data)) return false;
  data_[bank] = data;
  return true;
}
bool CPLDGPIO::read_pin(uint8_t pin, bool &value) {
  if (!ready() || pin > 15) return false;
  // Physical pads, including comparators during ADC ownership; never the DATA latch.
  value = (read_reg_(pin >= 8 ? 0x20 : 0x25) & (1U << (pin % 8))) != 0;
  return true;
}
bool CPLDGPIO::reserve_upper(const void *owner) {
  if (!ready() || owner == nullptr || (upper_owner_ != nullptr && upper_owner_ != owner)) return false;
  upper_owner_ = owner; return true;
}
void CPLDGPIO::release_upper(const void *owner) { if (owner == upper_owner_) upper_owner_ = nullptr; }
bool CPLDGPIO::update_cfg_(uint8_t mask, uint8_t value) {
  if (!ready() || cfg_busy_) return false;
  // ESPHome loop is cooperative. No yields/callbacks inside this RMW section.
  cfg_busy_ = true;
  cfg_ = read_reg_(0x23);
  uint8_t desired = (cfg_ & ~mask) | (value & mask);
  bool ok = write_reg_(0x12, 0x23, desired);
  cfg_ = read_reg_(0x23);
  cfg_busy_ = false;
  return ok && cfg_ == desired;
}
bool CPLDGPIO::acquire_flash(const void *owner) {
  if (!ready() || !flash_mode_ || owner == nullptr || flash_owner_ != nullptr) return false;
  uint8_t status = read_reg_(0x24);
  cfg_ = read_reg_(0x23);
  if ((status & 0xF7) != 0x30 || (status & 8) || (cfg_ & 1)) return false;
  flash_owner_ = owner;  // Retain lease even if write verification fails; caller must release.
  return update_cfg_(1, 1);
}
bool CPLDGPIO::release_flash(const void *owner) {
  if (owner == nullptr || flash_owner_ != owner) return false;
  if (!update_cfg_(1, 0)) return false;
  if (read_reg_(0x24) != 0x30) return false;
  flash_owner_ = nullptr;
  return true;
}
bool CPLDGPIO::verify_flash_owner(const void *owner) {
  if (!ready() || !has_flash_lease(owner)) return false;
  cfg_ = read_reg_(0x23);
  return (cfg_ & 1) && read_reg_(0x24) == 0x38;
}
bool CPLDGPIO::configure_target_controls(uint8_t cfg_levels, uint8_t direction, uint8_t open_drain) {
  if (!ready() || !bank_valid_[0] || !flash_mode_ || flash_owner_ || (read_reg_(0x23) & 1) ||
      ((cfg_levels | direction | open_drain) & ~0x06) || (direction & input_mask_[0])) return false;
  // Target control DIR/OD are lower-bank pad masks (OUT1/OUT2); levels use CFG bits2/1.
  uint8_t disconnected = dir_[0] & ~0x06;
  if (!write_bank_(0, 0x13, 0x26, disconnected)) return false;
  dir_[0] = disconnected;
  if (!update_cfg_(0x06, cfg_levels)) return false;
  uint8_t od = (od_[0] & ~0x06) | open_drain;
  if (!write_bank_(0, 0x15, 0x28, od)) return false;
  od_[0] = od;
  uint8_t dir = disconnected | direction;
  if (!write_bank_(0, 0x13, 0x26, dir)) return false;
  dir_[0] = dir;
  return true;
}
bool CPLDGPIO::set_target_controls(uint8_t levels) {
  if (!ready() || (levels & ~0x06) || !flash_mode_ || flash_owner_ != nullptr || (read_reg_(0x23) & 1)) return false;
  return update_cfg_(0x06, levels);
}
bool CPLDGPIOPin::digital_read() {
  bool value = false;
  if (!parent_->read_pin(pin_, value)) return false;
  return value != inverted_;
}
size_t CPLDGPIOPin::dump_summary(char *buffer, size_t len) const { return snprintf(buffer, len, "CPLD GPIO %u", pin_); }
}  // namespace esphome::moduliq_cpld_gpio
