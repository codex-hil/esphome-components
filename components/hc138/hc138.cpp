#include "esphome/core/log.h"
#include "hc138.h"

namespace esphome {
namespace hc138 {

static const char *TAG = "hc138";

spi::SPIDelegate *HC138Channel::register_device(spi::SPIClient *device, spi::SPIMode mode, spi::SPIBitOrder bit_order,
                                                uint32_t data_rate, GPIOPin *cs_pin) {
  if (parent_ == nullptr) {
    ESP_LOGE(TAG, "Parent not set for channel %d", channel_);
    return nullptr;
  }

  this->delegate_ = this->parent_->parent_->register_device(device, mode, bit_order, data_rate, cs_pin);
  return new HC138SPIDelegate(this->channel_, this->parent_, this->delegate_);
}

void HC138Component::setup() {
  a0_->setup();
  a1_->setup();
  a2_->setup();
}

void HC138Component::select_channel(uint8_t channel) {
  ESP_LOGI(TAG, "selecting channel %d", channel);

  a0_->digital_write(channel & 0x01);
  a1_->digital_write((channel >> 1) & 0x01);
  a2_->digital_write((channel >> 2) & 0x01);
}

void HC138SPIDelegate::transfer(uint8_t *data, size_t length) { parent_delegate_->transfer(data, length); }

uint8_t HC138SPIDelegate::transfer(uint8_t data) { return parent_delegate_->transfer(data); }

void HC138SPIDelegate::write(uint16_t data, size_t bits) { parent_delegate_->write(data, bits); }

void HC138SPIDelegate::read_array(uint8_t *data, size_t length) { parent_delegate_->read_array(data, length); }

void HC138SPIDelegate::write_array(const uint8_t *data, size_t length) { parent_delegate_->write_array(data, length); }

void HC138SPIDelegate::begin_transaction() {
  parent_->select_channel(this->channel_);
  parent_delegate_->begin_transaction();
}

void HC138SPIDelegate::end_transaction() { parent_delegate_->end_transaction(); }

}  // namespace hc138
}  // namespace esphome
