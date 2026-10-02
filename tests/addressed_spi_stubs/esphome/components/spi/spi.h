#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include <cstddef>
#include <cstdint>
#include <map>
namespace esphome::spi {
enum SPIBitOrder { BIT_ORDER_MSB_FIRST, BIT_ORDER_LSB_FIRST };
enum SPIClockPolarity { CLOCK_POLARITY_LOW, CLOCK_POLARITY_HIGH };
enum SPIClockPhase { CLOCK_PHASE_LEADING, CLOCK_PHASE_TRAILING };
enum SPIMode { MODE0, MODE1, MODE2, MODE3 };
enum SPIDataRate { DATA_RATE_1KHZ=1000, DATA_RATE_200KHZ=200000, DATA_RATE_1MHZ=1000000, DATA_RATE_10MHZ=10000000 };
class SPIClient {};
class SPIDelegate {
 public:
  virtual ~SPIDelegate()=default;
  virtual bool is_ready() { return true; }
  virtual void begin_transaction() {}
  virtual void end_transaction() {}
  virtual uint8_t transfer(uint8_t)=0;
  virtual void transfer(uint8_t *p,size_t n) { transfer(p,p,n); }
  virtual void transfer(const uint8_t *tx,uint8_t *rx,size_t n) {
    for(size_t i=0;i<n;i++) { auto v=transfer(tx?tx[i]:0); if(rx)rx[i]=v; }
  }
  virtual void write(uint16_t,size_t) {}
  virtual void write16(uint16_t) {}
  virtual void write_array16(const uint16_t *,size_t) {}
  virtual void write_array(const uint8_t *p,size_t n) { transfer(p,nullptr,n); }
  virtual void read_array(uint8_t *p,size_t n) { transfer(nullptr,p,n); }
  virtual void write_cmd_addr_data(size_t,uint32_t,size_t,uint32_t,const uint8_t *,size_t,uint8_t) {}
  static SPIDelegate *const NULL_DELEGATE;
};
class Dummy : public SPIDelegate { public: uint8_t transfer(uint8_t) override { return 0; } bool is_ready() override { return false; } };
inline Dummy dummy;
inline SPIDelegate *const SPIDelegate::NULL_DELEGATE=&dummy;
class SPIComponent : public Component {
 public:
  virtual SPIDelegate *register_device(SPIClient *,SPIMode,SPIBitOrder,uint32_t,GPIOPin *,bool,bool) { return nullptr; }
  virtual void unregister_device(SPIClient *) {}
 protected:
  std::map<SPIClient *,SPIDelegate *> devices_;
};
template<SPIBitOrder O,SPIClockPolarity P,SPIClockPhase H,SPIDataRate R>
class SPIDevice : public SPIClient {
 public:
  void set_spi_parent(SPIComponent *p) { parent_=p; }
  void set_cs_pin(GPIOPin *p) { cs_=p; }
  void set_mode(SPIMode m) { mode_=m; }
  void set_data_rate(uint32_t r) { data_rate_=r; }
  void set_write_only(bool) {}
  void set_release_device(bool value) { release_=value; }
  void spi_setup() { delegate_=parent_->register_device(this,mode_,O,data_rate_,cs_,release_,false); }
  bool spi_is_ready() { return delegate_ && delegate_->is_ready(); }
  void enable() { delegate_->begin_transaction(); }
  void disable() { delegate_->end_transaction(); }
  uint8_t transfer_byte(uint8_t v) { return delegate_->transfer(v); }
  void transfer_array(uint8_t *p,size_t n) { delegate_->transfer(p,n); }
  void write_array(const uint8_t *p,size_t n) { delegate_->write_array(p,n); }
 protected:
  SPIComponent *parent_{nullptr}; SPIDelegate *delegate_{nullptr}; GPIOPin *cs_{nullptr};
  SPIMode mode_{SPIMode(2*P+H)}; uint32_t data_rate_{R}; bool release_{false};
};
}
