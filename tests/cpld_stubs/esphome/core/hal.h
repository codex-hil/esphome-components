#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_millis;
inline uint32_t millis() { return test_millis; }
inline uint32_t test_micros=0;
inline uint32_t micros() { return test_micros; }
inline void delayMicroseconds(uint32_t us) { test_micros += us; }
}
