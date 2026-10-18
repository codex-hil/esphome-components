#pragma once
#include "esphome/core/component.h"
#include <functional>
namespace esphome::spi {
enum SPIBitOrder { BIT_ORDER_MSB_FIRST }; enum SPIClockPolarity { CLOCK_POLARITY_LOW };
enum SPIClockPhase { CLOCK_PHASE_LEADING }; enum SPIDataRate { DATA_RATE_200KHZ };
template<SPIBitOrder, SPIClockPolarity, SPIClockPhase, SPIDataRate> class SPIDevice {
 public: std::function<void(uint8_t)> transfer; void spi_setup() {} void enable() {} void disable() {}
 void write_byte(uint8_t x) { transfer(x); }
}; }
