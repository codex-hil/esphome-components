#define SERPROG_HARNESS
#include "cpld_components.cpp"
#include "esphome/components/moduliq_cpld_soft_i2c/moduliq_cpld_soft_i2c.h"
#include "esphome/core/hal.h"
using moduliq_cpld_soft_i2c::CPLDSoftI2C;
// Slave electrical model: latch master data on rising SCL, change slave SDA only while SCL low.
struct Slave {
  uint8_t latch=0xFF, bits=0, byte=0;
  bool address_phase=true;
  bool active=false, reading=false, slave_low=false, nack=false, master_ack=false;
  int starts=0, stops=0, clocks=0, stretch=0;
  bool stuck_scl=false, stuck_sda=false;
  int recover_edges=0, release_after=0;
  std::vector<uint8_t> received;
  std::vector<bool> acks;
  uint8_t response=0xA5;
  void change(uint8_t value) {
    bool old_sda=latch&1, old_scl=latch&2, sda=value&1, scl=value&2;
    latch=value;
    if (old_scl && scl && old_sda != sda && !slave_low) {
      if (!sda) { starts++; active=true; address_phase=true; reading=false; bits=0; byte=0; }
      else { stops++; active=false; slave_low=false; }
      return;
    }
    if (stuck_sda && !old_scl && scl) {
      recover_edges++;
      if (release_after && recover_edges >= release_after) stuck_sda=false;
    }
    if (!active) return;
    if (!old_scl && scl) {
      clocks++;
      if (bits<8) { byte=(byte<<1)|sda; bits++; }
      else {
        if (reading) acks.push_back(!sda);
        else received.push_back(byte);
        master_ack=!sda;
        bits=9;
      }
    }
    if (old_scl && !scl) {
      if (bits==8) slave_low = reading ? false : !nack;
      else if (bits==9) {
        if (address_phase) { reading=byte&1; address_phase=false; }
        bits=0; byte=0;
        slave_low=reading && !(response&0x80);
      } else slave_low=reading && !(response & (0x80>>bits));
    }
  }
  uint8_t pads() {
    uint8_t value=latch;
    if (slave_low || stuck_sda) value&=~1;
    if (stuck_scl || stretch-- > 0) value&=~2;
    return value;
  }
};
int main() {
  // Actual CPLD transport writes, ACK sampling and repeated START read sequence.
  Fixture f(0x10,0); Slave slave;
  f.bus.gpio_write=[&](uint8_t cmd,uint8_t value) { if(cmd==0x11) slave.change(value); };
  f.bus.gpio_read=[&](uint8_t cmd) { return cmd==0x20 ? slave.pads() : uint8_t(0xFF); };
  CPLDSoftI2C bus; bus.set_parent(&f.gpio); bus.set_pins(8,9); bus.setup();
  uint8_t reg=0x10, rx[2]{};
  auto first=bus.write_readv(0x20,&reg,1,rx,2);
  assert(first==i2c::ERROR_OK);
  assert(slave.starts==2 && slave.stops==1);
  assert((slave.received==std::vector<uint8_t>{0x40,0x10,0x41}));
  assert(rx[0]==0xA5 && rx[1]==0xA5);
  assert((slave.acks==std::vector<bool>{true,false}));
  // All four pairs share the bank without clobbering unrelated latch bits.
  CPLDSoftI2C more[3];
  for(int i=0;i<3;i++) { more[i].set_parent(&f.gpio); more[i].set_pins(10+2*i,11+2*i); more[i].setup(); assert(!more[i].is_failed()); }
  assert(!f.gpio.write_pin(8,false)); assert(!f.gpio.reserve_upper(&slave));
  assert(!f.gpio.configure_pin(8,gpio::FLAG_OUTPUT));
  assert(!f.gpio.reserve_pins(&slave,1U<<8));
  assert(f.gpio.reserve_pins(&slave,1U<<0));
  assert(f.gpio.configure_reserved(&slave,1U<<0));
  assert(f.gpio.write_reserved(&slave,1U<<0,0));
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_OK);
  assert((f.bus.reg[0x22]&0xFC)==0xFC && !(f.bus.reg[0x21]&1));
  // NACK is returned, STOP is sent, no write is retried.
  slave.nack=true; auto before=slave.starts;
  assert(bus.write_readv(0x20,&reg,1,nullptr,0)==i2c::ERROR_NOT_ACKNOWLEDGED);
  assert(slave.starts==before+1); assert((slave.latch&3)==3);
  slave.nack=false;
  // Physical low SCL, finite stretching, stuck SDA recovery and input bounds.
  slave.stuck_scl=true; uint32_t start=micros();
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_TIMEOUT);
  assert(micros()-start<30000); assert((slave.latch&3)==3);
  slave.stuck_scl=false; slave.stretch=3;
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_OK);
  slave.stuck_sda=true;
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)!=i2c::ERROR_OK);
  assert(slave.recover_edges>=9);
  slave.stuck_sda=true; slave.recover_edges=0; slave.release_after=4;
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_OK);
  assert(!slave.stuck_sda && slave.recover_edges==4);
  slave.release_after=0;
  assert(bus.write_readv(0x80,nullptr,0,nullptr,0)==i2c::ERROR_INVALID_ARGUMENT);
  assert(bus.write_readv(0x20,nullptr,1,nullptr,0)==i2c::ERROR_INVALID_ARGUMENT);
  assert(bus.write_readv(0x20,&reg,33,nullptr,0)==i2c::ERROR_TOO_LARGE);
  // A transaction-wide deadline bounds repeated stretches/large slow transfers.
  bus.set_frequency(100);
  uint8_t long_write[32]{};
  assert(bus.write_readv(0x20,long_write,32,nullptr,0)==i2c::ERROR_TIMEOUT);
  assert((slave.latch&3)==3);
  bus.set_frequency(1000);
  // Runtime disable/re-enable reapplies OD configuration through generation tracking.
  assert(f.gpio.set_enabled(false));
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_NOT_INITIALIZED);
  f.bus.reg[0x29]=0;
  assert(f.gpio.set_enabled(true));
  assert(bus.write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_OK);
  assert((f.bus.reg[0x29]&3)==3);
  assert(f.gpio.begin_pin_transaction(&bus));
  assert(!f.gpio.set_enabled(false));
  assert(more[0].write_readv(0x20,nullptr,0,nullptr,0)==i2c::ERROR_NOT_INITIALIZED);
  f.gpio.end_pin_transaction(&bus);
  Fixture lower(0x10,0); Slave low_slave;
  lower.bus.gpio_write=[&](uint8_t cmd,uint8_t value) { if(cmd==0x10) low_slave.change(value); };
  lower.bus.gpio_read=[&](uint8_t cmd) { return cmd==0x25 ? low_slave.pads() : uint8_t(0xFF); };
  CPLDSoftI2C low_bus; low_bus.set_parent(&lower.gpio); low_bus.set_pins(0,1); low_bus.setup();
  assert(low_bus.write_readv(0x20,&reg,1,rx,1)==i2c::ERROR_OK && rx[0]==0xA5);
  // Bus declared while GPIO is disabled reserves pins without SPI access.
  assert(lower.gpio.set_enabled(false)); lower.bus.frames.clear();
  CPLDSoftI2C delayed; delayed.set_parent(&lower.gpio); delayed.set_pins(2,3); delayed.setup();
  assert(lower.bus.frames.empty() && !delayed.is_failed());
  // Failed register write cannot turn into a slave NACK or successful transfer.
  f.bus.ignore_data=true;
  assert(bus.write_readv(0x20,&reg,1,nullptr,0)==i2c::ERROR_UNKNOWN);
  std::cout << "PASS: soft I2C electrical model, repeated START, read ACK/NACK, ownership, recovery, timeout, re-enable, SPI fault\n";
}
