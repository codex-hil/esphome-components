#pragma once
#include "esphome/core/hal.h"
namespace esphome { inline void delay(uint32_t ms) { test_millis += ms; } }
