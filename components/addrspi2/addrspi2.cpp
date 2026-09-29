#include "addrspi2.h"

#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {
namespace addrspi2 {

static const char *const TAG = "addrspi2";

void ADDRSPI2SPIDelegate::begin_transaction() {
  this->delegate_->begin_transaction();
  this->delegate_->write_array(&this->address_, 1);
}

void ADDRSPI2SPIDelegate::end_transaction() { this->delegate_->end_transaction(); }

uint8_t ADDRSPI2SPIDelegate::transfer(uint8_t data) { return this->delegate_->transfer(data); }

void ADDRSPI2SPIDelegate::transfer(uint8_t *ptr, size_t length) { this->delegate_->transfer(ptr, length); }

void ADDRSPI2SPIDelegate::transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
  this->delegate_->transfer(txbuf, rxbuf, length);
}

void ADDRSPI2SPIDelegate::write(uint16_t data, size_t num_bits) { this->delegate_->write(data, num_bits); }

void ADDRSPI2SPIDelegate::write16(uint16_t data) { this->delegate_->write16(data); }

void ADDRSPI2SPIDelegate::write_array16(const uint16_t *data, size_t length) {
  this->delegate_->write_array16(data, length);
}

void ADDRSPI2SPIDelegate::write_array(const uint8_t *ptr, size_t length) { this->delegate_->write_array(ptr, length); }

void ADDRSPI2SPIDelegate::read_array(uint8_t *ptr, size_t length) { this->delegate_->read_array(ptr, length); }

void ADDRSPI2SPIDelegate::write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                              const uint8_t *data, size_t length, uint8_t bus_width) {
  this->delegate_->write_cmd_addr_data(cmd_bits, cmd, addr_bits, address, data, length, bus_width);
}

bool ADDRSPI2SPIDelegate::is_ready() { return this->delegate_->is_ready(); }

void ADDRSPI2Component::setup() { this->spi_setup(); }

void ADDRSPI2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADDRSPI2:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Channels: %u", static_cast<unsigned>(this->channels_.size()));
  for (auto *channel : this->channels_) {
    if (channel != nullptr) {
      ESP_LOGCONFIG(TAG, "    Address: 0x%02X", channel->get_address());
    }
  }
}

spi::SPIDelegate *ADDRSPI2Component::register_upstream_device(spi::SPIClient *device, spi::SPIMode mode,
                                                              spi::SPIBitOrder bit_order, uint32_t data_rate,
                                                              GPIOPin *cs_pin, bool release_device, bool write_only) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 has no upstream SPI bus");
    return nullptr;
  }

  // Child drivers may still declare cs_pin because many ESPHome SPI schemas require it.
  // The physical transaction must use the ADDRSPI2 hub CS so the address prefix and
  // the downstream payload remain inside one CS-low frame.
  GPIOPin *physical_cs = this->cs_ != nullptr ? this->cs_ : cs_pin;
  return this->parent_->register_device(device, mode, bit_order, data_rate, physical_cs, release_device, write_only);
}

void ADDRSPI2Component::unregister_upstream_device(spi::SPIClient *device) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 has no upstream SPI bus");
    return;
  }
  this->parent_->unregister_device(device);
}

void ADDRSPI2Component::write_transaction(uint8_t address, const uint8_t *payload, size_t length) {
  if (payload == nullptr && length != 0) {
    ESP_LOGE(TAG, "Write payload is null");
    return;
  }

  uint8_t stack_buffer[STACK_PAYLOAD_SIZE + 1];
  if (length <= STACK_PAYLOAD_SIZE) {
    stack_buffer[0] = address;
    if (length != 0) {
      std::memcpy(&stack_buffer[1], payload, length);
    }
    this->enable();
    this->delegate_->write_array(stack_buffer, length + 1);
    this->disable();
    return;
  }

  std::vector<uint8_t> buffer(length + 1);
  buffer[0] = address;
  std::memcpy(&buffer[1], payload, length);
  this->enable();
  this->delegate_->write_array(buffer.data(), buffer.size());
  this->disable();
}

void ADDRSPI2Component::transfer_transaction(uint8_t address, const uint8_t *tx_payload, uint8_t *rx_payload,
                                             size_t length) {
  if ((tx_payload == nullptr || rx_payload == nullptr) && length != 0) {
    ESP_LOGE(TAG, "Transfer payload is null");
    return;
  }

  uint8_t stack_tx[STACK_PAYLOAD_SIZE + 1];
  uint8_t stack_rx[STACK_PAYLOAD_SIZE + 1];
  if (length <= STACK_PAYLOAD_SIZE) {
    stack_tx[0] = address;
    if (length != 0) {
      std::memcpy(&stack_tx[1], tx_payload, length);
    }
    this->enable();
    this->delegate_->transfer(stack_tx, stack_rx, length + 1);
    this->disable();
    if (length != 0) {
      std::memcpy(rx_payload, &stack_rx[1], length);
    }
    return;
  }

  std::vector<uint8_t> tx_buffer(length + 1);
  std::vector<uint8_t> rx_buffer(length + 1);
  tx_buffer[0] = address;
  std::memcpy(&tx_buffer[1], tx_payload, length);
  this->enable();
  this->delegate_->transfer(tx_buffer.data(), rx_buffer.data(), tx_buffer.size());
  this->disable();
  std::memcpy(rx_payload, &rx_buffer[1], length);
}

void ADDRSPI2Channel::dump_config() {
  ESP_LOGCONFIG(TAG, "ADDRSPI2 Channel:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

spi::SPIDelegate *ADDRSPI2Channel::register_device(spi::SPIClient *device, spi::SPIMode mode,
                                                   spi::SPIBitOrder bit_order, uint32_t data_rate, GPIOPin *cs_pin,
                                                   bool release_device, bool write_only) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 channel 0x%02X has no parent", this->address_);
    return nullptr;
  }
  if (this->devices_.count(device) != 0) {
    ESP_LOGE(TAG, "ADDRSPI2 channel 0x%02X device already registered", this->address_);
    return this->devices_[device];
  }

  auto *delegate =
      this->parent_->register_upstream_device(device, mode, bit_order, data_rate, cs_pin, release_device, write_only);
  if (delegate == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 channel 0x%02X failed to register upstream SPI device", this->address_);
    return nullptr;
  }

  auto *wrapped = new ADDRSPI2SPIDelegate(this->address_, delegate);
  this->devices_[device] = wrapped;
  return wrapped;
}

void ADDRSPI2Channel::unregister_device(spi::SPIClient *device) {
  if (this->devices_.count(device) == 0) {
    ESP_LOGE(TAG, "ADDRSPI2 channel 0x%02X device not registered", this->address_);
    return;
  }

  delete this->devices_[device];
  this->devices_.erase(device);
  if (this->parent_ != nullptr) {
    this->parent_->unregister_upstream_device(device);
  }
}

void ADDRSPI2Channel::write_transaction(const uint8_t *payload, size_t length) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 device 0x%02X has no parent", this->address_);
    return;
  }
  this->parent_->write_transaction(this->address_, payload, length);
}

void ADDRSPI2Channel::transfer_transaction(const uint8_t *tx_payload, uint8_t *rx_payload, size_t length) {
  if (this->parent_ == nullptr) {
    ESP_LOGE(TAG, "ADDRSPI2 device 0x%02X has no parent", this->address_);
    return;
  }
  this->parent_->transfer_transaction(this->address_, tx_payload, rx_payload, length);
}

}  // namespace addrspi2
}  // namespace esphome
