#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace dac8775 {

// Register addresses
namespace reg {
static const uint8_t NOP = 0x00;
static const uint8_t RESET = 0x01;
static const uint8_t RESET_CONFIG = 0x02;
static const uint8_t SELECT_DAC = 0x03;
static const uint8_t CONFIG_DAC = 0x04;
static const uint8_t DAC_DATA = 0x05;
static const uint8_t SELECT_BUCK_BOOST = 0x06;
static const uint8_t CONFIG_BUCK_BOOST = 0x07;
static const uint8_t CAL_ENABLE = 0x08;
static const uint8_t CAL_GAIN = 0x09;
static const uint8_t CAL_OFFSET = 0x0A;
static const uint8_t STATUS = 0x0B;
static const uint8_t STATUS_MASK = 0x0C;
static const uint8_t ALARM_ACTION = 0x0D;
static const uint8_t ALARM_CODE = 0x0E;
static const uint8_t WATCHDOG = 0x10;
static const uint8_t DEVICE_ID = 0x11;
}  // namespace reg

// Reset Config Register (0x02) bits
namespace reset_cfg {
static const uint16_t REF_EN = 0x0010;  // Internal reference enable
static const uint16_t CLRENA = 0x0200;  // Clear enable channel A
static const uint16_t CLRENB = 0x0400;  // Clear enable channel B
static const uint16_t CLRENC = 0x0800;  // Clear enable channel C
static const uint16_t CLREND = 0x1000;  // Clear enable channel D
}  // namespace reset_cfg

// Select DAC Register (0x03) bits
namespace select_dac {
static const uint16_t CHA = 0x0020;  // Channel A selected
static const uint16_t CHB = 0x0040;  // Channel B selected
static const uint16_t CHC = 0x0080;  // Channel C selected
static const uint16_t CHD = 0x0100;  // Channel D selected
}  // namespace select_dac

// Config DAC Register (0x04) bits
namespace config_dac {
// Range settings (bits 3:0)
static const uint16_t RANGE_V_0_5 = 0x0000;       // 0 to +5V
static const uint16_t RANGE_V_0_10 = 0x0001;      // 0 to +10V
static const uint16_t RANGE_V_PM5 = 0x0002;       // +/-5V
static const uint16_t RANGE_V_PM10 = 0x0003;      // +/-10V
static const uint16_t RANGE_I_3_5_23_5 = 0x0004;  // 3.5 to 23.5mA
static const uint16_t RANGE_I_0_20 = 0x0005;      // 0 to 20mA
static const uint16_t RANGE_I_0_24 = 0x0006;      // 0 to 24mA
static const uint16_t RANGE_I_PM24 = 0x0007;      // +/-24mA
static const uint16_t RANGE_V_0_6 = 0x0008;       // 0 to +6V (20% over)
static const uint16_t RANGE_V_0_12 = 0x0009;      // 0 to +12V (20% over)
static const uint16_t RANGE_V_PM6 = 0x000A;       // +/-6V (20% over)
static const uint16_t RANGE_V_PM12 = 0x000B;      // +/-12V (20% over)
static const uint16_t RANGE_I_4_20 = 0x000C;      // 4 to 20mA
static const uint16_t SREN = 0x0010;              // Slew rate enable
static const uint16_t OTEN = 0x1000;              // Output enable
static const uint16_t HTEN = 0x2000;              // HART enable
}  // namespace config_dac

// Select Buck-Boost Register (0x06) bits
namespace select_bb {
static const uint16_t DCA = 0x0001;  // Buck-boost A selected
static const uint16_t DCB = 0x0002;  // Buck-boost B selected
static const uint16_t DCC = 0x0004;  // Buck-boost C selected
static const uint16_t DCD = 0x0008;  // Buck-boost D selected
}  // namespace select_bb

// Config Buck-Boost Register (0x07) bits
namespace config_bb {
static const uint16_t PNSEL_DISABLED = 0x0000;  // Both arms disabled
static const uint16_t PNSEL_POS_ONLY = 0x0001;  // Positive arm only
static const uint16_t PNSEL_NEG_ONLY = 0x0002;  // Negative arm only
static const uint16_t PNSEL_BOTH = 0x0003;      // Both arms enabled
}  // namespace config_bb

static const uint8_t NUM_CHANNELS = 4;

enum class OutputRange : uint8_t {
  VOLTAGE_0_5V = 0,
  VOLTAGE_0_10V = 1,
  VOLTAGE_PM5V = 2,
  VOLTAGE_PM10V = 3,
  CURRENT_3_5_23_5MA = 4,
  CURRENT_0_20MA = 5,
  CURRENT_0_24MA = 6,
  CURRENT_PM24MA = 7,
  VOLTAGE_0_6V = 8,
  VOLTAGE_0_12V = 9,
  VOLTAGE_PM6V = 10,
  VOLTAGE_PM12V = 11,
  CURRENT_4_20MA = 12,
};

class DAC8775 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_ldac_pin(GPIOPin *pin) { this->ldac_pin_ = pin; }
  void set_clr_pin(GPIOPin *pin) { this->clr_pin_ = pin; }
  void set_use_internal_reference(bool use) { this->use_internal_reference_ = use; }

  void set_channel_range(uint8_t channel, OutputRange range);
  void set_channel_value(uint8_t channel, uint16_t value);
  void set_channel_enabled(uint8_t channel, bool enabled);
  void enable_buck_boost(uint8_t channel, bool positive, bool negative);
  void update_outputs();

  bool is_current_output(OutputRange range);
  bool is_bipolar(OutputRange range);
  void configure_channel_(uint8_t channel);

 protected:
  void reset_();
  void write_register_(uint8_t reg, uint16_t value);
  uint16_t read_register_(uint8_t reg);
  void select_channel_(uint8_t channel);
  void select_buck_boost_(uint8_t channel);

  GPIOPin *reset_pin_{nullptr};
  GPIOPin *ldac_pin_{nullptr};
  GPIOPin *clr_pin_{nullptr};
  bool use_internal_reference_{true};

  OutputRange channel_range_[NUM_CHANNELS]{OutputRange::VOLTAGE_0_10V};
  uint16_t channel_value_[NUM_CHANNELS]{0};
  bool channel_enabled_[NUM_CHANNELS]{false};
  bool buck_boost_pos_[NUM_CHANNELS]{false};
  bool buck_boost_neg_[NUM_CHANNELS]{false};
};

class DAC8775Channel : public output::FloatOutput, public Component {
 public:
  void set_parent(DAC8775 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_range(OutputRange range) { this->range_ = range; }

  void setup() override;
  void write_state(float state) override;

 protected:
  DAC8775 *parent_{nullptr};
  uint8_t channel_{0};
  OutputRange range_{OutputRange::VOLTAGE_0_10V};
};

}  // namespace dac8775
}  // namespace esphome
