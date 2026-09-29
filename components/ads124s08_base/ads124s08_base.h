#pragma once
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "protocol.h"
#include <vector>
#include "esphome/core/defines.h"
namespace esphome::ads124s08_base {
// Each entity owns a bounded, coalescing request; SPI runs only in the parent.
class ADS124S08Channel : public sensor::Sensor, public PollingComponent {
 public:
  void update() override { pending=true; }
  void configure(uint8_t mux, unsigned gain, bool pga, uint8_t reference,
                 float reference_voltage, uint8_t current, uint8_t idac_mux,
                 uint32_t settling, float rate, bool sinc3) {
    this->mux=mux; this->gain=gain; this->pga=pga; this->reference=reference;
    this->reference_voltage=reference_voltage; this->current=current;
    this->idac_mux=idac_mux; this->settling=settling; this->rate=rate; this->sinc3=sinc3;
  }
  bool pending{false};
  uint8_t mux{0x01}, reference{0x3A}, current{0}, idac_mux{0xFF};
  unsigned gain{1};
  bool pga{false}, sinc3{false};
  float reference_voltage{2.5f}, rate{20};
  uint32_t settling{100};
};
class ADS124S08Base : public PollingComponent,
 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,spi::CLOCK_POLARITY_LOW,spi::CLOCK_PHASE_TRAILING,static_cast<spi::SPIDataRate>(100000)> {
 public:
  void setup() override;
  void on_shutdown() override { stop_excitation_(); }
  void add_channel(ADS124S08Channel *s) { channels_.push_back(s); }
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  // Runtime reconfiguration is supported only while idle (also used by matrix fixture).
  bool is_idle() const { return phase_==Phase::IDLE; }
  unsigned sample_count() const { return sample_count_; }
  void set_continuous(bool v) { continuous_=v; }
  void set_samples_per_source(unsigned v) { samples_per_source_=v; }
  void set_miso_ready_pin(GPIOPin *pin) { ready_pin_=pin; }
  void set_gain(unsigned v) { gain_=v; }
  void set_sample_rate(float v) { sample_rate_=v; }
  void set_sinc3(bool v) { sinc3_=v; }
  void set_scan_offset(unsigned v) { source_index_=v; }
  void set_crc_errors(sensor::Sensor *s) { crc_errors_sensor_=s; }
#ifdef ADS124S08_CRC_FAULT_TEST
  void inject_crc_fault_once() { inject_crc_fault_=true; }
#endif
  void set_raw(sensor::Sensor *s) { raw_=s; }
  void set_short_raw(sensor::Sensor *s) { short_=s; }
  void set_avdd_voltage(sensor::Sensor *s) { avdd_=s; }
  void set_dvdd_voltage(sensor::Sensor *s) { dvdd_=s; }
  void set_die_temperature(sensor::Sensor *s) { temp_=s; }
  void set_status(text_sensor::TextSensor *s) { status_=s; }
  void set_source(text_sensor::TextSensor *s) { source_=s; }
 protected:
  enum class Phase { RESET_WAIT, IDLE, SETTLE, CONVERT, RETRY };
  Phase phase_{Phase::RESET_WAIT};
  uint32_t since_{0};
  unsigned source_index_{0}, gain_{1}, sample_count_{0};
  float sample_rate_{20};
  bool sinc3_{false}, continuous_{false};
  unsigned samples_per_source_{16}, burst_count_{0}, high_polls_{0};
  uint32_t armed_us_{0};
  uint32_t crc_errors_{0};
  sensor::Sensor *crc_errors_sensor_{nullptr};
#ifdef ADS124S08_CRC_FAULT_TEST
  bool inject_crc_fault_{false};
#endif
  GPIOPin *ready_pin_{nullptr};
  std::vector<ADS124S08Channel *> channels_;
  ADS124S08Channel *active_{nullptr};
  size_t next_channel_{0};
  bool diagnostic_pending_{false}, prefer_external_{true};
  uint8_t expected_[8]{};
  bool active_continuous_() const { return continuous_ && active_==nullptr; }
  float active_rate_() const { return active_?active_->rate:sample_rate_; }
  bool active_sinc3_() const { return active_?active_->sinc3:sinc3_; }
  void stop_excitation_();
  void start_next_();
  void configure_();
  uint8_t datarate_() const { return rate_register(sample_rate_,sinc3_) & (continuous_?0xDF:0xFF); }
  bool arm_ready_();
  sensor::Sensor *raw_{nullptr}, *short_{nullptr}, *avdd_{nullptr}, *dvdd_{nullptr}, *temp_{nullptr};
  text_sensor::TextSensor *status_{nullptr}, *source_{nullptr};
  void command_(uint8_t c);
  void write_(uint8_t r,uint8_t v);
  uint8_t read_(uint8_t r);
  bool verify_();
  void fail_(const char *why);
};
}
