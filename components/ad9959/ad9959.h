#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace ad9959 {

// Register addresses
namespace reg {
static const uint8_t CSR = 0x00;   // 1 byte, Channel Select Register
static const uint8_t FR1 = 0x01;   // 3 bytes, Function Register 1
static const uint8_t FR2 = 0x02;   // 2 bytes, Function Register 2
static const uint8_t CFR = 0x03;   // 3 bytes, Channel Function Register
static const uint8_t CFTW = 0x04;  // 4 bytes, Channel Frequency Tuning Word
static const uint8_t CPOW = 0x05;  // 2 bytes, Channel Phase Offset Word
static const uint8_t ACR = 0x06;   // 3 bytes, Amplitude Control Register
static const uint8_t LSRR = 0x07;  // 2 bytes, Linear Sweep Rate Register
static const uint8_t RDW = 0x08;   // 4 bytes, Rising Delta Word
static const uint8_t FDW = 0x09;   // 4 bytes, Falling Delta Word
static const uint8_t CW1 = 0x0A;   // 4 bytes, Channel Word 1
}  // namespace reg

// Register lengths
static const uint8_t REG_LENGTHS[] = {1, 3, 2, 3, 4, 2, 3, 2, 4, 4, 4};

// CSR bits
namespace csr {
static const uint8_t MSB_FIRST = 0x00;
static const uint8_t LSB_FIRST = 0x01;
static const uint8_t IO_2WIRE = 0x00;
static const uint8_t IO_3WIRE = 0x02;
static const uint8_t CHANNEL_0 = 0x10;
static const uint8_t CHANNEL_1 = 0x20;
static const uint8_t CHANNEL_2 = 0x40;
static const uint8_t CHANNEL_3 = 0x80;
static const uint8_t CHANNEL_ALL = 0xF0;
}  // namespace csr

// FR1 bits
namespace fr1 {
static const uint8_t CHARGE_PUMP_0 = 0x00;
static const uint8_t CHARGE_PUMP_1 = 0x01;
static const uint8_t CHARGE_PUMP_2 = 0x02;
static const uint8_t CHARGE_PUMP_3 = 0x03;
static const uint8_t PLL_DIVIDER = 0x04;
static const uint8_t VCO_GAIN = 0x80;
static const uint8_t MOD_LEVELS_2 = 0x00;
static const uint8_t SYNC_CLK_DISABLE = 0x20;
}  // namespace fr1

// CFR bits
namespace cfr {
static const uint32_t DAC_FULL_SCALE = 0x000300;
static const uint32_t MATCH_PIPE_DELAY = 0x000020;
static const uint32_t OUTPUT_SINE_WAVE = 0x000001;
static const uint32_t SWEEP_ENABLE = 0x004000;
static const uint32_t FREQ_MODULATION = 0x800000;
static const uint32_t PHASE_MODULATION = 0xC00000;
static const uint32_t AMP_MODULATION = 0x400000;
}  // namespace cfr

// ACR bits
namespace acr {
static const uint32_t MULTIPLIER_ENABLE = 0x001000;
}  // namespace acr

static const uint8_t NUM_CHANNELS = 4;

class AD9959 : public Component,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_io_update_pin(GPIOPin *pin) { this->io_update_pin_ = pin; }
  void set_ref_clock(uint32_t clock) { this->ref_clock_ = clock; }
  void set_pll_multiplier(uint8_t mult) { this->pll_multiplier_ = mult; }

  void set_frequency(uint8_t channel, uint32_t frequency);
  void set_phase(uint8_t channel, uint16_t phase);
  void set_amplitude(uint8_t channel, uint16_t amplitude);

  // Most AD9959 writes are buffered until I/O_UPDATE transfers them into active registers.
  void update_registers();
  uint32_t get_core_clock() const { return this->core_clock_; }

 protected:
  void reset_();
  void pulse_io_update_();
  void write_register_(uint8_t reg, uint32_t value);
  uint32_t read_register_(uint8_t reg);
  void select_channel_(uint8_t channel);
  void configure_pll_();
  void configure_channels_();
  uint32_t frequency_to_tuning_word_(uint32_t frequency);

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *io_update_pin_{nullptr};
  uint32_t ref_clock_{25000000};
  uint8_t pll_multiplier_{20};
  uint32_t core_clock_{0};
  uint8_t last_channel_{0xFF};

  uint32_t channel_frequency_[NUM_CHANNELS]{0};
  uint16_t channel_phase_[NUM_CHANNELS]{0};
  uint16_t channel_amplitude_[NUM_CHANNELS]{1023};
};

class AD9959Channel : public output::FloatOutput, public Component {
 public:
  void set_parent(AD9959 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void write_state(float state) override {
    if (state != state) {
      state = 0.0f;
    } else if (state < 0.0f) {
      state = 0.0f;
    } else if (state > 1.0f) {
      state = 1.0f;
    }
    uint32_t max_freq = this->parent_->get_core_clock() / 2;
    uint32_t freq = static_cast<uint32_t>(state * max_freq);
    this->parent_->set_frequency(this->channel_, freq);
    this->parent_->update_registers();
  }

 protected:
  AD9959 *parent_{nullptr};
  uint8_t channel_{0};
};

}  // namespace ad9959
}  // namespace esphome
