#include "esphome/components/addrspi/addrspi.h"
#include "esphome/components/addrspi2/addrspi2.h"
#include "esphome/components/mcp3208/mcp3208.h"
#include "esphome/components/dacx0504/dacx0504.h"
#include "esphome/components/spi_shift_register/spi_shift_register.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

uint32_t esphome::test_millis=0;
static size_t allocations=0;
void *operator new(size_t n) { allocations++; if(auto *p=std::malloc(n)) return p; throw std::bad_alloc(); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p,size_t) noexcept { std::free(p); }
using namespace esphome;
struct Wire {
  bool active=false;
  unsigned begin=0,end=0,bursts=0,byte_calls=0;
  uint8_t tx[4092]={},reply[4092]={}; size_t length=0;
  bool emulate_dac=false;
  uint16_t registers[16]={}, pending_value=0;
  uint8_t pending_command=0;
  unsigned soft_resets=0;
};
struct Pin : GPIOPin {
  Wire &wire; bool level=false; unsigned writes=0;
  explicit Pin(Wire &w):wire(w) {}
  void digital_write(bool v) override { assert(!wire.active);level=v;writes++; }
};
struct Delegate : spi::SPIDelegate {
  Wire &wire; explicit Delegate(Wire &w):wire(w) {}
  void begin_transaction() override { assert(!wire.active);wire.active=true;wire.begin++; }
  void end_transaction() override { assert(wire.active);wire.active=false;wire.end++; }
  uint8_t transfer(uint8_t) override { wire.byte_calls++;assert(false && "must be bulk");return 0; }
  void transfer(const uint8_t *tx,uint8_t *rx,size_t n) override {
    assert(wire.active);wire.bursts++;wire.length=n;
    for(size_t i=0;i<n;i++) { wire.tx[i]=tx?tx[i]:0; if(rx)rx[i]=wire.reply[i]; }
    if (wire.emulate_dac && tx && n==4 && tx[0]==5) {
      if(rx) { rx[0]=0xFF;rx[1]=wire.pending_command;rx[2]=wire.pending_value>>8;rx[3]=wire.pending_value; }
      const auto reg=tx[1]&15;
      const uint16_t value=(uint16_t(tx[2])<<8)|tx[3];
      if (!(tx[1]&128) && reg!=0) wire.registers[reg]=value;
      if(reg==5 && value==10) wire.soft_resets++;
      wire.pending_command=tx[1];wire.pending_value=wire.registers[reg];
    }
  }
};
struct Bus : spi::SPIComponent {
  unsigned registrations=0,unregistrations=0;
  Wire &wire; explicit Bus(Wire &w):wire(w) {}
  spi::SPIDelegate *register_device(spi::SPIClient *,spi::SPIMode,spi::SPIBitOrder,uint32_t,GPIOPin *,bool,bool) override {
    registrations++;return new Delegate(wire);
  }
  void unregister_device(spi::SPIClient *) override { unregistrations++; }
};
static void expect(Wire &w,std::initializer_list<uint8_t> bytes) {
  assert(w.length==bytes.size());size_t i=0;for(auto b:bytes)assert(w.tx[i++]==b);
  assert(w.begin==w.end && !w.active && w.byte_calls==0);
}
int main() {
  Wire w;Bus bus(w);Delegate physical(w);
  addrspi2::ADDRSPI2SPIDelegate framed(4,&physical);
  uint8_t native[]={6,0xC0,0};
  w.reply[0]=0xDE;w.reply[1]=0;w.reply[2]=7;w.reply[3]=0xFE;
  auto before=allocations;
  framed.begin_transaction();assert(w.bursts==0);
  framed.transfer(native,3);framed.end_transaction();
  expect(w,{4,6,0xC0,0});assert(native[1]==7 && native[2]==0xFE);
  assert(allocations==before);  // no frame-time heap for common native packets
  const auto bursts=w.bursts;
  framed.begin_transaction();framed.transfer(0);framed.transfer(0);framed.end_transaction();
  assert(w.bursts==bursts+1);  // do not silently split a byte-stream transaction
  uint8_t rx[2]={}; framed.begin_transaction();framed.read_array(rx,2);framed.end_transaction();
  expect(w,{4,0,0}); assert(rx[0]==0 && rx[1]==7);

  // Real nested parallel delegates around the serial adapter; pin changes must
  // precede physical CS assertion, and serial header/payload remain one burst.
  Pin module_pin(w), chip0(w),chip1(w),head_cs(w);
  addrspi::ADDRSPIComponent module;module.set_spi_parent(&bus);module.set_address_pins({&module_pin});module.setup();
  addrspi::ADDRSPIChannel module_bus;module_bus.set_parent(&module);module_bus.set_channel(0);
  addrspi::ADDRSPIComponent chip;chip.set_spi_parent(&module_bus);chip.set_address_pins({&chip0,&chip1});chip.setup();
  addrspi::ADDRSPIChannel chip_bus;chip_bus.set_parent(&chip);chip_bus.set_channel(2);
  addrspi2::ADDRSPI2Component serial;serial.set_spi_parent(&chip_bus);serial.set_cs_pin(&head_cs);serial.setup();
  addrspi2::ADDRSPI2Channel adc_bus;adc_bus.set_parent(&serial);adc_bus.set_address(4);
  mcp3208::MCP3208 adc;adc.set_spi_parent(&adc_bus);adc.set_reference_voltage(3);adc.setup();
  auto writes=chip1.writes;before=allocations;
  const auto count=w.bursts;
  float voltage=adc.read_voltage(3,false);
  expect(w,{4,6,0xC0,0});assert(w.bursts==count+1 && allocations==before);
  assert(!module_pin.level && !chip0.level && chip1.level && chip1.writes>writes);
  assert(std::fabs(voltage-2046.0f*3/4096)<1e-6);
  adc.read_voltage(3,false);assert(chip1.writes>writes+1); // routing reapplied every CS
  assert(!HighFrequencyLoopRequester::is_high_frequency());
  adc.register_channel(3,false);adc.set_sample_rate(100);
  assert(HighFrequencyLoopRequester::is_high_frequency());
  assert(std::isnan(adc.read_voltage(3,false)));
  auto conversions=w.bursts;test_micros+=10000;adc.loop();assert(w.bursts==conversions+1);
  voltage=adc.read_voltage(3,false);assert(w.bursts==conversions+1); // publication does not convert
  assert(std::fabs(voltage-2046.0f*3/4096)<1e-6);

  adc.set_sample_rate(0);assert(!HighFrequencyLoopRequester::is_high_frequency());
  adc.set_sample_rate(100);adc.on_shutdown();assert(!HighFrequencyLoopRequester::is_high_frequency());

  // The same ADC works without serial addressing and without separate drivers.
  mcp3208::MCP3208 direct;direct.set_spi_parent(&chip_bus);direct.set_reference_voltage(3);direct.setup();
  w.reply[0]=0;w.reply[1]=7;w.reply[2]=0xFE;
  voltage=direct.read_voltage(3,false);expect(w,{6,0xC0,0});assert(std::fabs(voltage-2046.0f*3/4096)<1e-6);

  addrspi2::ADDRSPI2Channel dac_bus;dac_bus.set_parent(&serial);dac_bus.set_address(5);
  dacx0504::DACX0504 dac;dac.set_spi_parent(&dac_bus);dac.set_mode(spi::MODE0);
  dac.set_reference(dacx0504::DACX0504_EXTERNAL_REFERENCE);dac.set_reference_voltage(3);
  dac.set_reference_divider(2);dac.set_gain(2);dac.set_fast_sdo(true);
  w.emulate_dac=true;w.registers[1]=0x0497;dac.set_verify_registers(true);dac.setup();
  assert(!dac.is_failed() && w.registers[3]==0x0500 && w.registers[4]==0x010F && w.soft_resets==0);
  dac.set_channel_voltage(2,1.5);expect(w,{5,10,128,0});
  dac.set_channel_voltage(2,3);expect(w,{5,10,255,255});
  dac.set_channel_voltage(2,-1);expect(w,{5,10,0,0});
  dac.set_channel_voltage(2,NAN);expect(w,{5,10,0,0});
  dac.set_synchronous_update(true);dac.commit();expect(w,{5,5,0,16});
  dacx0504::DACX0504 bad;bad.set_spi_parent(&dac_bus);bad.set_mode(spi::MODE0);
  bad.set_verify_registers(true);w.registers[1]=0;bad.setup();assert(bad.is_failed());
  conversions=w.bursts;bad.set_channel_value(0,.5);assert(conversions==w.bursts);

  addrspi2::ADDRSPI2Channel gpio_bus;gpio_bus.set_parent(&serial);gpio_bus.set_address(1);
  spi_shift_register::SPIShiftRegister out;out.set_spi_parent(&gpio_bus);out.set_model(spi_shift_register::HC595);
  out.set_initial_value(7);out.setup();expect(w,{1,7});out.set_bit(1,false);expect(w,{1,5});
  addrspi2::ADDRSPI2Channel input_bus;input_bus.set_parent(&serial);input_bus.set_address(0);
  spi_shift_register::SPIShiftRegister blocked;blocked.set_spi_parent(&input_bus);blocked.setup();
  assert(blocked.is_failed());auto n=w.bursts;blocked.update();assert(n==w.bursts);
  spi_shift_register::SPIShiftRegister in;in.set_spi_parent(&input_bus);in.set_load_pulse_verified(true);
  binary_sensor::BinarySensor bit0,bit1;in.register_input(0,&bit0);in.register_input(1,&bit1);in.setup();
  w.reply[0]=0xFF;w.reply[1]=1;in.update();expect(w,{0,0});assert(bit0.state && !bit1.state);
  // Eight serial targets and a parallel-only peer use only two physical
  // handles. Removing one logical target must not unregister the shared head.
  Wire shared_wire; Bus shared_bus(shared_wire);
  addrspi2::ADDRSPI2Component shared_head;
  shared_head.set_spi_parent(&shared_bus); shared_head.set_shared_device(true);
  shared_head.set_data_rate(100000); shared_head.setup();
  addrspi2::ADDRSPI2Channel targets[8]; spi::SPIClient clients[9];
  for (unsigned i=0;i<8;i++) {
    targets[i].set_parent(&shared_head); targets[i].set_address(i);
    auto *d=targets[i].register_device(&clients[i],spi::MODE0,spi::BIT_ORDER_MSB_FIRST,100000,nullptr,false,false);
    assert(d->is_ready()); uint8_t payload=0xAA;
    d->begin_transaction(); d->write_array(&payload,1); d->end_transaction();
    expect(shared_wire,{uint8_t(i),0xAA});
  }
  assert(shared_bus.registrations==1);
  auto *peer=shared_bus.register_device(&clients[8],spi::MODE0,spi::BIT_ORDER_MSB_FIRST,100000,nullptr,false,false);
  assert(peer->is_ready() && shared_bus.registrations==2);
  targets[0].unregister_device(&clients[0]); assert(shared_bus.unregistrations==0);
  auto *invalid=shared_head.register_upstream_device(&clients[0],spi::MODE0,spi::BIT_ORDER_MSB_FIRST,200000,nullptr,false,false);
  assert(!invalid->is_ready() && shared_bus.registrations==2);
  puts("PASS: combined parallel+serial routing, atomic frames, RX discard, no frame heap, ADC/DAC scaling and GPIO gate");
}
