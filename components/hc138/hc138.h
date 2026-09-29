#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace hc138 {

class HC138Component;
class HC138SPIDelegate;

class HC138Channel : public spi::SPIComponent {
 public:
  void set_channel(uint8_t channel) { channel_ = channel; }
  void set_parent(HC138Component *parent) { this->parent_ = parent; }

  void setup() override{};
  void dump_config() override{};
  spi::SPIDelegate *register_device(spi::SPIClient *device, spi::SPIMode mode, spi::SPIBitOrder bit_order,
                                    uint32_t data_rate, GPIOPin *cs_pin) override;

 protected:
  uint8_t channel_;
  spi::SPIDelegate *delegate_;
  HC138Component *parent_;
  friend class HC138SPIDelegate;
};

class HC138Component : public Component,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ> {
 public:
  void set_a0(GPIOPin *pin) { a0_ = pin; }
  void set_a1(GPIOPin *pin) { a1_ = pin; }
  void set_a2(GPIOPin *pin) { a2_ = pin; }
  void setup() override;
  void select_channel(uint8_t channel);

 protected:
  GPIOPin *a0_;
  GPIOPin *a1_;
  GPIOPin *a2_;
  HC138Channel *ch;
  friend class HC138Channel;
};

class HC138SPIDelegate : public spi::SPIDelegate {
 public:
  HC138SPIDelegate(uint8_t channel, HC138Component *parent, spi::SPIDelegate *parent_delegate)
      : channel_(channel), parent_(parent), parent_delegate_(parent_delegate) {}

  void transfer(uint8_t *data, size_t length) override;
  uint8_t transfer(uint8_t data) override;
  void write(uint16_t data, size_t bits) override;
  void read_array(uint8_t *data, size_t length) override;
  void write_array(const uint8_t *data, size_t length) override;
  void begin_transaction() override;
  void end_transaction() override;

 private:
  uint8_t channel_;
  HC138Component *parent_;
  spi::SPIDelegate *parent_delegate_;
};

}  // namespace hc138
}  // namespace esphome
