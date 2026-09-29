#pragma once

#include <vector>

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ads124s08 {

enum ADS124S08Reference : uint8_t {
  ADS124S08_REF0 = 0x00,
  ADS124S08_REF1 = 0x01,
  ADS124S08_INTERNAL = 0x02,
};

enum ADS124S08Model : uint8_t {
  ADS124S08 = 0x00,
  ADS124S06 = 0x01,
};

enum ADS124S08DataRate : uint8_t {
  ADS124S08_RATE_2_5SPS = 0x00,
  ADS124S08_RATE_5SPS = 0x01,
  ADS124S08_RATE_10SPS = 0x02,
  ADS124S08_RATE_16_6SPS = 0x03,
  ADS124S08_RATE_20SPS = 0x04,
  ADS124S08_RATE_50SPS = 0x05,
  ADS124S08_RATE_60SPS = 0x06,
  ADS124S08_RATE_100SPS = 0x07,
  ADS124S08_RATE_200SPS = 0x08,
  ADS124S08_RATE_400SPS = 0x09,
  ADS124S08_RATE_800SPS = 0x0A,
  ADS124S08_RATE_1000SPS = 0x0B,
  ADS124S08_RATE_2000SPS = 0x0C,
  ADS124S08_RATE_4000SPS = 0x0D,
};

enum ADS124S08ChannelType : uint8_t {
  ADS124S08_CHANNEL_RTD = 0,
  ADS124S08_CHANNEL_NTC = 1,
};

struct ADS124S08Channel {
  ADS124S08ChannelType type;
  uint8_t ain_p;
  uint8_t ain_n;
  uint8_t idac_pin;
  float idac_current_a;
  float reference_resistance;
  float nominal_resistance;
  float beta;
  float reference_temperature_c;
  uint8_t rtd_wires;
  sensor::Sensor *temperature_sensor;
  sensor::Sensor *resistance_sensor;
  sensor::Sensor *raw_sensor;
  text_sensor::TextSensor *status_sensor;
};

class ADS124S08Component : public PollingComponent,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_model(ADS124S08Model model) { this->model_ = model; }
  void set_reference(ADS124S08Reference reference) { this->reference_ = reference; }
  void set_pga_gain(uint8_t gain_code) { this->pga_gain_code_ = gain_code; }
  void set_data_rate(ADS124S08DataRate data_rate) { this->data_rate_ = data_rate; }
  void set_conversion_time_ms(uint32_t conversion_time_ms) { this->conversion_time_ms_ = conversion_time_ms; }
  void set_drdy_pin(GPIOPin *pin) { this->drdy_pin_ = pin; }
  void set_start_sync_pin(GPIOPin *pin) { this->start_sync_pin_ = pin; }

  void add_rtd_channel(uint8_t ain_p, uint8_t ain_n, uint8_t idac_pin, float idac_current_a, float reference_resistance,
                       float nominal_resistance, uint8_t rtd_wires, sensor::Sensor *temperature_sensor,
                       sensor::Sensor *resistance_sensor, sensor::Sensor *raw_sensor,
                       text_sensor::TextSensor *status_sensor);

  void add_ntc_channel(uint8_t ain_p, uint8_t ain_n, float reference_resistance, float nominal_resistance, float beta,
                       float reference_temperature_c, sensor::Sensor *temperature_sensor,
                       sensor::Sensor *resistance_sensor, sensor::Sensor *raw_sensor,
                       text_sensor::TextSensor *status_sensor);

 protected:
  enum State : uint8_t {
    STATE_IDLE,
    STATE_CONFIGURING,
    STATE_CONVERTING,
    STATE_READING,
  };

  void command_(uint8_t command);
  uint8_t read_register_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value);
  int32_t read_data_();

  void start_next_channel_();
  void configure_channel_(const ADS124S08Channel &channel);
  void finish_channel_();
  void publish_status_(const ADS124S08Channel &channel, const char *status);
  void publish_nan_(const ADS124S08Channel &channel, const char *status);

  uint8_t idac_current_code_(float current_a) const;
  uint8_t reference_register_() const;
  float gain_value_() const;
  float code_to_ratio_(int32_t raw) const;
  float rtd_temperature_(float resistance) const;
  float ntc_temperature_(float resistance, const ADS124S08Channel &channel) const;

  ADS124S08Model model_{ADS124S08};
  ADS124S08Reference reference_{ADS124S08_REF0};
  uint8_t pga_gain_code_{0};
  ADS124S08DataRate data_rate_{ADS124S08_RATE_20SPS};
  uint32_t conversion_time_ms_{120};
  GPIOPin *drdy_pin_{nullptr};
  GPIOPin *start_sync_pin_{nullptr};

  std::vector<ADS124S08Channel> channels_;
  State state_{STATE_IDLE};
  size_t current_channel_{0};
};

}  // namespace ads124s08
}  // namespace esphome
