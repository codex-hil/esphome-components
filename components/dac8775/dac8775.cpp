#include "dac8775.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dac8775 {

static const char *const TAG = "dac8775";

void DAC8775::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DAC8775...");

  this->spi_setup();

  this->reset_pin_->setup();
  this->reset_pin_->digital_write(true);

  if (this->ldac_pin_ != nullptr) {
    this->ldac_pin_->setup();
    this->ldac_pin_->digital_write(false);
  }

  if (this->clr_pin_ != nullptr) {
    this->clr_pin_->setup();
    this->clr_pin_->digital_write(false);
  }

  this->reset_();
  delay(10);

  if (this->use_internal_reference_) {
    this->write_register_(reg::RESET_CONFIG, reset_cfg::REF_EN);
    ESP_LOGD(TAG, "Internal reference enabled");
  }

  uint16_t device_id = this->read_register_(reg::DEVICE_ID);
  ESP_LOGD(TAG, "Device ID: 0x%04X", device_id);
}

void DAC8775::dump_config() {
  ESP_LOGCONFIG(TAG, "DAC8775:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  if (this->ldac_pin_ != nullptr) {
    LOG_PIN("  LDAC Pin: ", this->ldac_pin_);
  }
  if (this->clr_pin_ != nullptr) {
    LOG_PIN("  CLR Pin: ", this->clr_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Internal Reference: %s", this->use_internal_reference_ ? "enabled" : "disabled");
}

void DAC8775::reset_() {
  this->reset_pin_->digital_write(false);
  delayMicroseconds(10);
  this->reset_pin_->digital_write(true);
  delayMicroseconds(100);

  this->write_register_(reg::RESET, 0x0001);
  delay(1);
}

void DAC8775::write_register_(uint8_t reg, uint16_t value) {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(reg & 0x7F);
  this->transfer_byte((value >> 8) & 0xFF);
  this->transfer_byte(value & 0xFF);
  this->disable();
}

uint16_t DAC8775::read_register_(uint8_t reg) {
  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(0x80 | reg);
  this->transfer_byte(0x00);
  this->transfer_byte(0x00);
  this->disable();

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(reg::NOP);
  uint16_t value = this->transfer_byte(0x00) << 8;
  value |= this->transfer_byte(0x00);
  this->disable();

  return value;
}

void DAC8775::select_channel_(uint8_t channel) {
  uint16_t select = 0;
  switch (channel) {
    case 0:
      select = select_dac::CHA;
      break;
    case 1:
      select = select_dac::CHB;
      break;
    case 2:
      select = select_dac::CHC;
      break;
    case 3:
      select = select_dac::CHD;
      break;
    default:
      return;
  }
  this->write_register_(reg::SELECT_DAC, select);
}

void DAC8775::select_buck_boost_(uint8_t channel) {
  uint16_t select = 0;
  switch (channel) {
    case 0:
      select = select_bb::DCA;
      break;
    case 1:
      select = select_bb::DCB;
      break;
    case 2:
      select = select_bb::DCC;
      break;
    case 3:
      select = select_bb::DCD;
      break;
    default:
      return;
  }
  this->write_register_(reg::SELECT_BUCK_BOOST, select);
}

bool DAC8775::is_current_output(OutputRange range) {
  return range == OutputRange::CURRENT_0_20MA || range == OutputRange::CURRENT_0_24MA ||
         range == OutputRange::CURRENT_4_20MA || range == OutputRange::CURRENT_3_5_23_5MA ||
         range == OutputRange::CURRENT_PM24MA;
}

bool DAC8775::is_bipolar(OutputRange range) {
  return range == OutputRange::VOLTAGE_PM5V || range == OutputRange::VOLTAGE_PM10V ||
         range == OutputRange::VOLTAGE_PM6V || range == OutputRange::VOLTAGE_PM12V ||
         range == OutputRange::CURRENT_PM24MA;
}

void DAC8775::set_channel_range(uint8_t channel, OutputRange range) {
  if (channel >= NUM_CHANNELS)
    return;
  this->channel_range_[channel] = range;
}

void DAC8775::set_channel_enabled(uint8_t channel, bool enabled) {
  if (channel >= NUM_CHANNELS)
    return;
  this->channel_enabled_[channel] = enabled;
}

void DAC8775::enable_buck_boost(uint8_t channel, bool positive, bool negative) {
  if (channel >= NUM_CHANNELS)
    return;
  this->buck_boost_pos_[channel] = positive;
  this->buck_boost_neg_[channel] = negative;

  this->select_buck_boost_(channel);

  uint16_t pnsel = config_bb::PNSEL_DISABLED;
  if (positive && negative) {
    pnsel = config_bb::PNSEL_BOTH;
  } else if (positive) {
    pnsel = config_bb::PNSEL_POS_ONLY;
  } else if (negative) {
    pnsel = config_bb::PNSEL_NEG_ONLY;
  }

  this->write_register_(reg::CONFIG_BUCK_BOOST, pnsel);
  ESP_LOGD(TAG, "Channel %d buck-boost: pos=%d neg=%d", channel, positive, negative);
}

void DAC8775::configure_channel_(uint8_t channel) {
  if (channel >= NUM_CHANNELS)
    return;

  this->select_channel_(channel);

  uint16_t config = static_cast<uint16_t>(this->channel_range_[channel]);
  if (this->channel_enabled_[channel]) {
    config |= config_dac::OTEN;
  }

  this->write_register_(reg::CONFIG_DAC, config);
  ESP_LOGD(TAG, "Channel %d config: range=%d enabled=%d", channel, static_cast<int>(this->channel_range_[channel]),
           this->channel_enabled_[channel]);
}

void DAC8775::set_channel_value(uint8_t channel, uint16_t value) {
  if (channel >= NUM_CHANNELS)
    return;

  this->channel_value_[channel] = value;

  this->select_channel_(channel);
  this->write_register_(reg::DAC_DATA, value);

  ESP_LOGD(TAG, "Channel %d value: 0x%04X", channel, value);
}

void DAC8775::update_outputs() {
  if (this->ldac_pin_ != nullptr) {
    this->ldac_pin_->digital_write(true);
    delayMicroseconds(1);
    this->ldac_pin_->digital_write(false);
  }
}

void DAC8775Channel::setup() {
  bool need_neg_supply = this->parent_->is_current_output(this->range_) || this->parent_->is_bipolar(this->range_);

  this->parent_->enable_buck_boost(this->channel_, true, need_neg_supply);
  this->parent_->set_channel_range(this->channel_, this->range_);
  this->parent_->set_channel_enabled(this->channel_, true);
  this->parent_->configure_channel_(this->channel_);
}

void DAC8775Channel::write_state(float state) {
  uint16_t value;

  if (this->parent_->is_bipolar(this->range_)) {
    value = static_cast<uint16_t>((state + 1.0f) * 0.5f * 65535.0f);
  } else {
    value = static_cast<uint16_t>(state * 65535.0f);
  }

  this->parent_->set_channel_value(this->channel_, value);
  this->parent_->update_outputs();
}

}  // namespace dac8775
}  // namespace esphome
