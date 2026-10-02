#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mcp3208 {

class MCP3208 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_reference_voltage(float voltage) { this->reference_voltage_ = voltage; }
  float read_voltage(uint8_t channel, bool differential);
  // Total conversions per second across registered channel/mode pairs.
  // Zero preserves on-demand conversion at sensor publication time.
  void set_sample_rate(float rate) {
    sample_period_us_ = rate > 0 ? static_cast<uint32_t>(1000000.0f / rate) : 0;
    update_loop_request_();
  }
  void register_channel(uint8_t channel, bool differential) {
    requested_mask_ |= 1U << (channel + (differential ? 8 : 0));
    update_loop_request_();
  }
  void on_shutdown() override { high_frequency_.stop(); }

 protected:
  void update_loop_request_() {
    if (sample_period_us_ != 0 && requested_mask_ != 0 && !this->is_failed()) high_frequency_.start();
    else high_frequency_.stop();
  }
  HighFrequencyLoopRequester high_frequency_;
  uint16_t read_raw_(uint8_t channel, bool differential);

  float reference_voltage_{3.3f};
  uint32_t sample_period_us_{0}, last_sample_us_{0};
  uint16_t requested_mask_{0}, valid_mask_{0};
  uint16_t cached_raw_[16] = {};
  uint8_t next_channel_{0};
};

class MCP3208Sensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_parent(MCP3208 *parent) { this->parent_ = parent; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_differential(bool differential) { this->differential_ = differential; }

  void dump_config() override;
  void update() override;

 protected:
  MCP3208 *parent_{nullptr};
  uint8_t channel_{0};
  bool differential_{false};
};

}  // namespace mcp3208
}  // namespace esphome
