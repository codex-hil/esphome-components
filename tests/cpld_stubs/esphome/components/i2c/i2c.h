#pragma once
#include <cstdint>
#include <cstddef>
namespace esphome::i2c {
enum ErrorCode { ERROR_OK, ERROR_UNKNOWN };
class I2CBus {
 public:
  virtual ~I2CBus() = default;
  virtual ErrorCode read_register(uint8_t address, uint8_t reg, uint8_t *data, size_t size)=0;
  virtual ErrorCode write_register(uint8_t address, uint8_t reg, const uint8_t *data, size_t size)=0;
};
class I2CDevice {
 public:
  void set_i2c_bus(I2CBus *bus) { bus_=bus; }
  void set_i2c_address(uint8_t address) { address_=address; }
  ErrorCode read_register(uint8_t reg, uint8_t *data, size_t size) { return bus_->read_register(address_,reg,data,size); }
  bool write_byte(uint8_t reg, uint8_t value) { return bus_->write_register(address_,reg,&value,1)==ERROR_OK; }
 protected: uint8_t address_{0x50}; I2CBus *bus_{nullptr};
};
}
