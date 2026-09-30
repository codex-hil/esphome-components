#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <array>
#include <string>
#include <vector>
namespace esphome::moduliq_cpld_gpio { class CPLDGPIO; }
namespace esphome::moduliq_cpld_i2c {
struct DigitalID {
  bool upper;
  uint8_t mask, shift, xor_mask;
  text_sensor::TextSensor *sensor;
  std::vector<std::pair<uint8_t, std::string>> codes;
};
struct ADCBand { uint16_t min, max; std::string id; };
class CPLDReadout : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void on_shutdown() override;
  void dump_config() override;
  void set_gpio(moduliq_cpld_gpio::CPLDGPIO *gpio) { gpio_ = gpio; }
  void set_rtd16(bool value) { rtd16_ = value; }
  void set_numeric(uint8_t index, sensor::Sensor *value) { numeric_[index] = value; }
  void set_counter(uint8_t channel, sensor::Sensor *value) { counters_[channel] = value; }
  void set_status(text_sensor::TextSensor *value) { status_ = value; }
  void add_digital_id(bool upper, uint8_t mask, uint8_t shift, uint8_t xor_mask, text_sensor::TextSensor *sensor) {
    digital_.push_back({upper, mask, shift, xor_mask, sensor, {}});
  }
  void add_digital_code(size_t index, uint8_t code, const std::string &id) { digital_[index].codes.emplace_back(code, id); }
  void configure_adc(uint8_t bits, uint32_t timeout) { adc_enabled_ = true; adc_bits_ = bits; timeout_ms_ = timeout; }
  void set_adc_raw(uint8_t channel, sensor::Sensor *value) { adc_raw_[channel] = value; }
  void set_adc_id(uint8_t channel, text_sensor::TextSensor *value) { adc_ids_[channel] = value; }
  void add_adc_band(uint8_t channel, uint16_t min, uint16_t max, const std::string &id) {
    bands_[channel].push_back({min, max, id});
  }
 protected:
  enum class ADCState { IDLE, WAITING, CLEANUP };
  bool reserve_();
  void unreserve_();
  bool disable_adc_();
  void finish_adc_(bool valid, const char *reason);
  void publish_adc_();
  void report_(const char *state, bool warning);
  bool read_(uint8_t reg, uint8_t *data, size_t size) { return read_register(reg, data, size) == i2c::ERROR_OK; }
  sensor::Sensor *numeric_[8]{};
  sensor::Sensor *counters_[4]{};
  sensor::Sensor *adc_raw_[8]{};
  text_sensor::TextSensor *adc_ids_[8]{};
  text_sensor::TextSensor *status_{nullptr};
  moduliq_cpld_gpio::CPLDGPIO *gpio_{nullptr};
  std::vector<DigitalID> digital_;
  std::vector<ADCBand> bands_[8];
  std::array<uint16_t, 8> results_{};
  ADCState state_{ADCState::IDLE};
  bool adc_enabled_{false}, rtd16_{false}, reserved_{false}, pending_valid_{false}, measurement_pending_{false};
  bool readout_valid_{true}, updating_{false}, publishing_{false};
  uint8_t adc_bits_{8};
  uint32_t timeout_ms_{500}, started_ms_{0}, polled_ms_{0};
  const char *pending_reason_{"idle"};
};
}  // namespace esphome::moduliq_cpld_i2c
