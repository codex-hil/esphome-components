#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include <cstddef>
#include <cstdint>
namespace esphome::spi {
enum SPIBitOrder { BIT_ORDER_MSB_FIRST };
enum SPIClockPolarity { CLOCK_POLARITY_LOW, CLOCK_POLARITY_HIGH };
enum SPIClockPhase { CLOCK_PHASE_LEADING, CLOCK_PHASE_TRAILING };
enum SPIMode { MODE0, MODE1, MODE2, MODE3 };
enum SPIDataRate { DATA_RATE_1KHZ=1000, DATA_RATE_100KHZ=100000 };
class SPIClient {};
class SPIDelegate {
 public:
  virtual ~SPIDelegate()=default;
  virtual void begin_transaction()=0;
  virtual void end_transaction()=0;
  virtual uint8_t transfer(uint8_t)=0;
  virtual void transfer(uint8_t *ptr,size_t size) { for(size_t i=0;i<size;i++) ptr[i]=transfer(ptr[i]); }
  virtual void transfer(const uint8_t *tx,uint8_t *rx,size_t size) { for(size_t i=0;i<size;i++) { auto v=transfer(tx?tx[i]:0); if(rx) rx[i]=v; } }
  virtual void write(uint16_t,size_t) {}
  virtual void write16(uint16_t) {}
  virtual void read_array(uint8_t *data,size_t size) { for(size_t i=0;i<size;i++) data[i]=transfer(0); }
};
class SPIComponent : public Component {
 public:
  virtual SPIDelegate *register_device(SPIClient *,SPIMode,SPIBitOrder,uint32_t,GPIOPin *,bool,bool) { return nullptr; }
};
template<SPIBitOrder O, SPIClockPolarity P, SPIClockPhase H, SPIDataRate R>
class SPIDevice : public SPIClient {
 public:
  void set_spi_parent(SPIComponent *parent) { parent_=parent; }
  void set_cs_pin(GPIOPin *pin) { cs_=pin; }
  void set_mode(SPIMode mode) { mode_=mode; }
  void set_data_rate(uint32_t rate) { rate_=rate; }
  void spi_setup() { delegate_=parent_->register_device(this,mode_,O,rate_,cs_,false,false); }
  void enable() { delegate_->begin_transaction(); }
  void disable() { delegate_->end_transaction(); }
  uint8_t transfer_byte(uint8_t data) { return delegate_->transfer(data); }
 protected:
  SPIComponent *parent_{nullptr}; SPIDelegate *delegate_{nullptr}; GPIOPin *cs_{nullptr};
  SPIMode mode_{SPIMode(2*P+H)}; uint32_t rate_{R};
};
}
