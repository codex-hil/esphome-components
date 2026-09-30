#pragma once
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/moduliq_cpld_gpio/moduliq_cpld_gpio.h"
#include <string>

namespace esphome::moduliq_cpld_flash {
class CPLDFlash : public Component,
    public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                         spi::CLOCK_PHASE_LEADING, static_cast<spi::SPIDataRate>(100000)> {
 public:
  void set_gpio(moduliq_cpld_gpio::CPLDGPIO *gpio) { gpio_ = gpio; }
  void set_target_profile(const std::string &profile) { profile_ = profile; }
  void set_status(text_sensor::TextSensor *sensor) { status_ = sensor; }
  void setup() override;
  void loop() override;
  void on_shutdown() override { release(); }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  // target_ready asserts the caller completed the exact board's isolation/boot procedure.
  bool acquire(bool target_ready);
  bool transfer(const uint8_t *tx, uint8_t *rx, size_t size);
  bool release();
  // Recommended bounded transaction: always attempts release, including invalid-buffer paths.
  bool transaction(const uint8_t *tx, uint8_t *rx, size_t size, bool target_ready);
  bool owned() const { return owned_; }
 protected:
  void report_(const char *state, bool warning);
  moduliq_cpld_gpio::CPLDGPIO *gpio_{nullptr};
  text_sensor::TextSensor *status_{nullptr};
  std::string profile_;
  bool owned_{false}, active_{false}, cleanup_pending_{false};
  uint32_t retried_ms_{0};
};
}  // namespace esphome::moduliq_cpld_flash
