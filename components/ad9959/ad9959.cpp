#include "ad9959.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ad9959 {

static const char *const TAG = "ad9959";

void AD9959::setup() {
  ESP_LOGCONFIG(TAG, "Setting up AD9959...");

  this->spi_setup();

  this->reset_pin_->setup();
  this->reset_pin_->digital_write(false);

  this->io_update_pin_->setup();
  this->io_update_pin_->digital_write(false);

  this->reset_();
  delay(1);

  this->configure_pll_();
  delay(1);

  this->configure_channels_();
  this->pulse_io_update_();

  ESP_LOGD(TAG, "AD9959 initialized, core clock: %u Hz", this->core_clock_);
}

void AD9959::dump_config() {
  ESP_LOGCONFIG(TAG, "AD9959:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  I/O Update Pin: ", this->io_update_pin_);
  ESP_LOGCONFIG(TAG, "  Reference Clock: %u Hz", this->ref_clock_);
  ESP_LOGCONFIG(TAG, "  PLL Multiplier: %u", this->pll_multiplier_);
  ESP_LOGCONFIG(TAG, "  Core Clock: %u Hz", this->core_clock_);
}

void AD9959::reset_() {
  this->reset_pin_->digital_write(true);
  delayMicroseconds(1);
  this->reset_pin_->digital_write(false);
  delayMicroseconds(1);

  this->pulse_io_update_();

  this->last_channel_ = 0xFF;
  this->select_channel_(csr::CHANNEL_ALL);
  this->write_register_(reg::CFR, cfr::DAC_FULL_SCALE | cfr::MATCH_PIPE_DELAY | cfr::OUTPUT_SINE_WAVE);

  this->select_channel_(0);
  this->pulse_io_update_();
}

void AD9959::configure_pll_() {
  this->core_clock_ = this->ref_clock_ * this->pll_multiplier_;

  uint8_t fr1_high = (this->pll_multiplier_ * fr1::PLL_DIVIDER) | fr1::CHARGE_PUMP_3;
  // VCO gain: LOW for <160MHz, HIGH for >255MHz (160-255MHz is undefined per datasheet)
  if (this->core_clock_ > 255000000) {
    fr1_high |= fr1::VCO_GAIN;
  }

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(reg::FR1);
  this->transfer_byte(fr1_high);
  this->transfer_byte(fr1::MOD_LEVELS_2);
  this->transfer_byte(fr1::SYNC_CLK_DISABLE);
  this->disable();

  this->pulse_io_update_();
}

void AD9959::configure_channels_() {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    this->select_channel_(1 << (4 + i));
    this->write_register_(reg::CFR, cfr::DAC_FULL_SCALE | cfr::MATCH_PIPE_DELAY | cfr::OUTPUT_SINE_WAVE);
  }
}

void AD9959::pulse_io_update_() {
  this->io_update_pin_->digital_write(true);
  delayMicroseconds(1);
  this->io_update_pin_->digital_write(false);
}

void AD9959::select_channel_(uint8_t channel) {
  if (this->last_channel_ != channel) {
    this->write_register_(reg::CSR, channel | csr::MSB_FIRST | csr::IO_3WIRE);
    this->last_channel_ = channel;
  }
}

void AD9959::write_register_(uint8_t reg, uint32_t value) {
  uint8_t len = (reg < sizeof(REG_LENGTHS)) ? REG_LENGTHS[reg] : 4;

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(reg);
  for (int i = len - 1; i >= 0; i--) {
    this->transfer_byte((value >> (i * 8)) & 0xFF);
  }
  this->disable();
}

uint32_t AD9959::read_register_(uint8_t reg) {
  uint8_t len = (reg < sizeof(REG_LENGTHS)) ? REG_LENGTHS[reg] : 4;
  uint32_t value = 0;

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(0x80 | reg);
  for (int i = 0; i < len; i++) {
    value = (value << 8) | this->transfer_byte(0xFF);
  }
  this->disable();

  return value;
}

uint32_t AD9959::frequency_to_tuning_word_(uint32_t frequency) {
  return (uint32_t) (((uint64_t) frequency << 32) / this->core_clock_);
}

void AD9959::set_frequency(uint8_t channel, uint32_t frequency) {
  if (channel >= NUM_CHANNELS)
    return;

  this->channel_frequency_[channel] = frequency;

  uint8_t chan_mask = 1 << (4 + channel);
  this->select_channel_(chan_mask);

  uint32_t ftw = this->frequency_to_tuning_word_(frequency);
  this->write_register_(reg::CFTW, ftw);

  ESP_LOGD(TAG, "Channel %d frequency: %u Hz (FTW: 0x%08X)", channel, frequency, ftw);
}

void AD9959::set_phase(uint8_t channel, uint16_t phase) {
  if (channel >= NUM_CHANNELS)
    return;

  this->channel_phase_[channel] = phase & 0x3FFF;

  uint8_t chan_mask = 1 << (4 + channel);
  this->select_channel_(chan_mask);
  this->write_register_(reg::CPOW, this->channel_phase_[channel]);

  ESP_LOGD(TAG, "Channel %d phase: %u", channel, this->channel_phase_[channel]);
}

void AD9959::set_amplitude(uint8_t channel, uint16_t amplitude) {
  if (channel >= NUM_CHANNELS)
    return;

  if (amplitude > 1023)
    amplitude = 1023;

  this->channel_amplitude_[channel] = amplitude;

  uint8_t chan_mask = 1 << (4 + channel);
  this->select_channel_(chan_mask);

  uint32_t acr_value = 0;
  if (amplitude < 1024) {
    acr_value = acr::MULTIPLIER_ENABLE | amplitude;
  }

  this->enable();
  delayMicroseconds(1);
  this->transfer_byte(reg::ACR);
  this->transfer_byte(0);
  this->transfer_byte((acr_value >> 8) & 0xFF);
  this->transfer_byte(acr_value & 0xFF);
  this->disable();

  ESP_LOGD(TAG, "Channel %d amplitude: %u", channel, amplitude);
}

void AD9959::update_registers() { this->pulse_io_update_(); }

}  // namespace ad9959
}  // namespace esphome
