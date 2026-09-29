#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace as6500 {

// SPI Opcodes
namespace opcode {
static const uint8_t POWER_ON_RESET = 0x30;
static const uint8_t INIT = 0x18;
static const uint8_t WRITE_CONFIG = 0x80;
static const uint8_t READ_CONFIG = 0x40;
static const uint8_t READ_RESULTS = 0x60;
}  // namespace opcode

// CFG0 register bits
namespace cfg0 {
static const uint8_t PIN_ENA_STOP1 = 0x01;
static const uint8_t PIN_ENA_STOP2 = 0x02;
static const uint8_t PIN_ENA_STOP3 = 0x04;
static const uint8_t PIN_ENA_STOP4 = 0x08;
static const uint8_t PIN_ENA_REFCLK = 0x10;
static const uint8_t PIN_ENA_DISABLE = 0x40;
static const uint8_t PIN_ENA_RSTIDX = 0x80;
}  // namespace cfg0

// CFG1 register bits
namespace cfg1 {
static const uint8_t HIT_ENA_STOP1 = 0x01;
static const uint8_t HIT_ENA_STOP2 = 0x02;
static const uint8_t HIT_ENA_STOP3 = 0x04;
static const uint8_t HIT_ENA_STOP4 = 0x08;
static const uint8_t CHANNEL_COMBINE_NORMAL = 0x00;
static const uint8_t CHANNEL_COMBINE_PULSE_DISTANCE = 0x10;
static const uint8_t CHANNEL_COMBINE_PULSE_WIDTH = 0x20;
static const uint8_t HIGH_RESOLUTION_OFF = 0x00;
static const uint8_t HIGH_RESOLUTION_X2 = 0x40;
static const uint8_t HIGH_RESOLUTION_X4 = 0x80;
}  // namespace cfg1

// CFG2 register bits
namespace cfg2 {
static const uint8_t COMMON_FIFO_READ = 0x40;
static const uint8_t BLOCKWISE_FIFO_READ = 0x80;
}  // namespace cfg2

// CFG7 register bits
namespace cfg7 {
static const uint8_t REFCLK_BY_XOSC = 0x80;
}  // namespace cfg7

// Fixed register values from datasheet
namespace defaults {
static const uint8_t CFG6 = 0xC0;
static const uint8_t CFG7 = 0x23;
static const uint8_t CFG8 = 0xA1;
static const uint8_t CFG9 = 0x13;
static const uint8_t CFG10 = 0x00;
static const uint8_t CFG11 = 0x0A;
static const uint8_t CFG12 = 0xCC;
static const uint8_t CFG13 = 0x05;
static const uint8_t CFG14 = 0xF1;
static const uint8_t CFG15 = 0x7D;
static const uint8_t CFG16 = 0x04;
}  // namespace defaults

// Result register addresses
namespace result_addr {
static const uint8_t CH1_REFIDX = 8;
static const uint8_t CH1_TSTOP = 11;
static const uint8_t CH2_REFIDX = 14;
static const uint8_t CH2_TSTOP = 17;
static const uint8_t CH3_REFIDX = 20;
static const uint8_t CH3_TSTOP = 23;
static const uint8_t CH4_REFIDX = 26;
static const uint8_t CH4_TSTOP = 29;
}  // namespace result_addr

// Number of channels
static const uint8_t NUM_CHANNELS = 4;
static const uint8_t NUM_CONFIG_REGS = 17;
static const uint8_t BYTES_PER_CHANNEL = 6;

// Default REFCLK_DIVISIONS for 5.12MHz crystal = 0x2FAF1 (195313) for 1ps resolution
static const uint32_t DEFAULT_REFCLK_DIV = 0x2FAF1;

enum HighResolutionMode : uint8_t {
  HIGH_RES_OFF = 0,
  HIGH_RES_X2 = 1,
  HIGH_RES_X4 = 2,
};

enum ChannelCombineMode : uint8_t {
  COMBINE_NORMAL = 0,
  COMBINE_PULSE_DISTANCE = 1,
  COMBINE_PULSE_WIDTH = 2,
};

class AS6500 : public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_interrupt_pin(GPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_refclk_divisions(uint32_t div) { this->refclk_divisions_ = div; }
  void set_high_resolution_mode(HighResolutionMode mode) { this->high_res_mode_ = mode; }
  void set_channel_combine_mode(ChannelCombineMode mode) { this->combine_mode_ = mode; }
  void set_use_crystal(bool use_crystal) { this->use_crystal_ = use_crystal; }

  void set_channel_sensor(uint8_t channel, sensor::Sensor *sensor) {
    if (channel < NUM_CHANNELS)
      this->channel_sensors_[channel] = sensor;
  }

  void set_stop_enabled(uint8_t channel, bool enabled) {
    if (channel < NUM_CHANNELS)
      this->stop_enabled_[channel] = enabled;
  }

 protected:
  void write_register_(uint8_t address, uint8_t data);
  uint8_t read_register_(uint8_t address);
  void write_config_();
  bool verify_config_();
  void power_on_reset_();
  void init_measurement_();
  bool read_results_();

  GPIOPin *interrupt_pin_{nullptr};
  uint32_t refclk_divisions_{DEFAULT_REFCLK_DIV};
  HighResolutionMode high_res_mode_{HIGH_RES_OFF};
  ChannelCombineMode combine_mode_{COMBINE_NORMAL};
  bool use_crystal_{true};

  sensor::Sensor *channel_sensors_[NUM_CHANNELS]{nullptr};
  bool stop_enabled_[NUM_CHANNELS]{true, true, true, true};

  uint32_t reference_index_[NUM_CHANNELS]{0};
  uint32_t stop_result_[NUM_CHANNELS]{0};
};

}  // namespace as6500
}  // namespace esphome
