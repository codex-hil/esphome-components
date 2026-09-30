#pragma once
#include <cstdint>
namespace esphome {
extern uint32_t test_millis;
inline uint32_t millis() { return test_millis; }
inline void delayMicroseconds(uint32_t) {}
}
