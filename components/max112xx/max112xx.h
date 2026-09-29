#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace max112xx {

enum Max112xxGain : uint8_t {
  MAX112XX_GAIN_1 = 0x00,
  MAX112XX_GAIN_2 = 0x01,
  MAX112XX_GAIN_4 = 0x02,
  MAX112XX_GAIN_8 = 0x03,
  MAX112XX_GAIN_16 = 0x04,
  MAX112XX_GAIN_32 = 0x05,
  MAX112XX_GAIN_64 = 0x06,
  MAX112XX_GAIN_128 = 0x07,
};

enum Max112xxRate : uint8_t {
  MAX112XX_RATE_1SPS = 0x00,
  MAX112XX_RATE_2_5SPS = 0x01,
  MAX112XX_RATE_5SPS = 0x02,
  MAX112XX_RATE_10SPS = 0x03,
  MAX112XX_RATE_15SPS = 0x04,
  MAX112XX_RATE_30SPS = 0x05,
  MAX112XX_RATE_60SPS = 0x06,
  MAX112XX_RATE_120SPS = 0x07,
};

// SPI MODE 0: CPOL=0, CPHA=0 (leading edge sample), MSB first, max 5MHz
class MAX112xxSensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_gain(Max112xxGain gain) { this->gain_ = gain; }
  void set_rate(Max112xxRate rate) { this->rate_ = rate; }
  void set_line_frequency_50hz(bool line_50hz) { this->line_50hz_ = line_50hz; }
  void set_reference_voltage(float vref) { this->reference_voltage_ = vref; }
  void set_bipolar(bool bipolar) { this->bipolar_ = bipolar; }
  void set_enable_sig_buf(bool enable) { this->sig_buf_ = enable; }
  void set_enable_ref_buf(bool enable) { this->ref_buf_ = enable; }
  void set_self_calibrate(bool cal) { this->self_calibrate_ = cal; }

 protected:
  // SPI command/register helpers
  void send_command_(uint8_t cmd);
  uint8_t read_register8_(uint8_t reg);
  void write_register8_(uint8_t reg, uint8_t value);
  int32_t read_register24_(uint8_t reg);

  void read_data_();

  Max112xxGain gain_{MAX112XX_GAIN_1};
  Max112xxRate rate_{MAX112XX_RATE_10SPS};
  bool line_50hz_{true};
  float reference_voltage_{2.5f};
  bool bipolar_{true};
  bool sig_buf_{false};
  bool ref_buf_{false};
  bool self_calibrate_{true};
};

}  // namespace max112xx
}  // namespace esphome
