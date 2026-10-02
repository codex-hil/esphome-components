#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome::i2c {
enum ErrorCode { ERROR_OK=0, ERROR_INVALID_ARGUMENT=1, ERROR_NOT_ACKNOWLEDGED=2,
ERROR_TIMEOUT=3, ERROR_NOT_INITIALIZED=4, ERROR_TOO_LARGE=5, ERROR_UNKNOWN=6 };
class I2CBus {
 public:
 virtual ~I2CBus() = default;
 virtual ErrorCode write_readv(uint8_t, const uint8_t *, size_t, uint8_t *, size_t) { return ERROR_UNKNOWN; }
 virtual ErrorCode read_register(uint8_t, uint8_t, uint8_t *, size_t) { return ERROR_UNKNOWN; }
 virtual ErrorCode write_register(uint8_t, uint8_t, const uint8_t *, size_t) { return ERROR_UNKNOWN; }
};
}
