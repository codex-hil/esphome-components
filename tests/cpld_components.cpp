// Actual production classes and actual addrspi delegate, with electrically inert bus emulators.
#include "esphome/components/moduliq_cpld_gpio/moduliq_cpld_gpio.h"
#include "esphome/components/moduliq_cpld_i2c/moduliq_cpld_i2c.h"
#include "esphome/components/moduliq_cpld_flash/moduliq_cpld_flash.h"
#include "esphome/components/addrspi/addrspi.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
#include <functional>

namespace esphome { uint32_t test_millis=0; }
using namespace esphome;
using moduliq_cpld_gpio::CPLDGPIO;
using moduliq_cpld_i2c::CPLDReadout;
using moduliq_cpld_flash::CPLDFlash;
struct TestPin : GPIOPin {
  bool value=false;
  void digital_write(bool v) override { value=v; }
  bool digital_read() override { return value; }
};
struct Frame { uint8_t channel; spi::SPIMode mode; std::vector<uint8_t> tx; };
struct SPIBus;
struct Delegate : spi::SPIDelegate {
  SPIBus *bus; spi::SPIMode mode; Frame frame;
  Delegate(SPIBus *b,spi::SPIMode m):bus(b),mode(m) {}
  void begin_transaction() override;
  void end_transaction() override;
  uint8_t transfer(uint8_t value) override;
};
struct SPIBus : spi::SPIComponent {
  TestPin address[2], cs;
  bool active=false;
  uint8_t straps=0x30;
  std::array<uint8_t,256> reg{};
  std::vector<Frame> frames;
  std::vector<std::unique_ptr<Delegate>> delegates;
  bool ignore_release=false, ignore_data=false;
  int corrupt_cfg_reads=0, corrupt_after_acquire=0;
  std::function<void()> during_flash, before_begin;
  std::function<uint8_t(const std::vector<uint8_t> &)> flash_read;
  std::function<void(const std::vector<uint8_t> &)> flash_end;
  SPIBus() {
    reg[0x21]=0xA5; reg[0x22]=0x5A; reg[0x23]=0xA6;
    reg[0x26]=0xFF; reg[0x27]=0xFF; reg[0x28]=0xA0; reg[0x29]=0x11;
    reg[0x20]=0x90; reg[0x25]=0x3C;
  }
  uint8_t channel() const { return address[0].value | (address[1].value << 1); }
  spi::SPIDelegate *register_device(spi::SPIClient *,spi::SPIMode m,spi::SPIBitOrder,uint32_t rate,GPIOPin *,bool,bool) override {
    assert(rate==100000); delegates.emplace_back(new Delegate(this,m)); return delegates.back().get();
  }
  std::vector<Frame> writes() const {
    std::vector<Frame> out;
    for (const auto &f: frames) if(f.channel==3 && f.tx[0]>=0x10 && f.tx[0]<=0x16) out.push_back(f);
    return out;
  }
};
void Delegate::begin_transaction() {
  assert(!bus->active); if (bus->before_begin) bus->before_begin(); bus->active=true; bus->cs.value=false;
  frame={bus->channel(),mode,{}};
  if(frame.channel==3) assert(mode==spi::MODE0);
}
void Delegate::end_transaction() {
  assert(bus->active);
  if(frame.channel==3) assert(frame.tx.size()==2);
  bus->active=false; bus->cs.value=true; bus->frames.push_back(frame);
  if (frame.channel==2 && bus->flash_end) bus->flash_end(frame.tx);
}
uint8_t Delegate::transfer(uint8_t value) {
  assert(bus->active); frame.tx.push_back(value);
  if(frame.channel!=3) {
    if(frame.channel==2 && bus->during_flash) bus->during_flash();
    if (frame.channel==2 && bus->flash_read) return bus->flash_read(frame.tx);
    return value ^ 0xFF;
  }
  if(frame.tx.size()!=2) return 0;
  uint8_t command=frame.tx[0];
  static const uint8_t reads[]={0x21,0x22,0x23,0x26,0x27,0x28,0x29};
  if(command>=0x10 && command<=0x16) {
    if(command==0x10 && bus->ignore_data) return 0;
    if(command==0x12) {
      if(bus->ignore_release && !(value&1)) return 0;
      if(value&1) bus->corrupt_cfg_reads=bus->corrupt_after_acquire;
    }
    bus->reg[reads[command-0x10]]=value;
    return 0;
  }
  if(command==0x24) return bus->straps | ((bus->reg[0x23]&1)<<3);
  if(command==0x23 && bus->corrupt_cfg_reads>0) { bus->corrupt_cfg_reads--; return bus->reg[command]^1; }
  return bus->reg[command];
}
struct Fixture {
  SPIBus bus;
  addrspi::ADDRSPIComponent mux;
  addrspi::ADDRSPIChannel channels[4];
  CPLDGPIO gpio;
  Fixture(uint8_t straps=0x30,uint8_t upper_mask=0x88) {
    bus.straps=straps;
    mux.set_spi_parent(&bus);
    mux.set_address_pins({&bus.address[0],&bus.address[1]}); mux.setup();
    for(uint8_t i=0;i<4;i++) { channels[i].set_parent(&mux); channels[i].set_channel(i); }
    gpio.set_initial_enabled(true);
    gpio.set_spi_parent(&channels[3]); gpio.set_cs_pin(&bus.cs);
    gpio.set_input_masks(0,upper_mask); gpio.setup();
  }
  void init_flash(CPLDFlash &flash) {
    flash.set_enabled(true); flash.set_gpio(&gpio); flash.set_spi_parent(&channels[2]); flash.set_cs_pin(&bus.cs);
    flash.set_target_profile("test_part"); flash.set_mode(spi::MODE3); flash.setup();
  }
};
struct I2COp { bool write; uint8_t reg; size_t size; uint8_t value; };
struct I2CBus : i2c::I2CBus {
  std::array<uint8_t,256> reg{};
  std::vector<I2COp> ops;
  uint8_t fail_read_reg=0xFF, fail_write_reg=0xFF;
  int fail_reads=0, fail_writes=0;
  std::function<void(uint8_t)> after_read;
  I2CBus() { reg[0]=3; reg[1]=0x10; reg[2]=0xB5; reg[3]=0x21; reg[4]=0x5A; reg[0x21]=0xCF; }
  i2c::ErrorCode read_register(uint8_t address,uint8_t r,uint8_t *data,size_t size) override {
    assert(address==0x50); ops.push_back({false,r,size,0});
    bool fail=r==fail_read_reg && fail_reads-- > 0;
    for(size_t i=0;i<size;i++) {
      data[i]=reg[r+i];
      if(r+i>=0x10 && r+i<=0x13) reg[r+i]=0;
    }
    if(after_read) after_read(r);
    return fail ? i2c::ERROR_UNKNOWN : i2c::ERROR_OK;
  }
  i2c::ErrorCode write_register(uint8_t address,uint8_t r,const uint8_t *data,size_t size) override {
    assert(address==0x50 && size==1); ops.push_back({true,r,size,*data});
    if(r==fail_write_reg && fail_writes-- > 0) return i2c::ERROR_UNKNOWN;
    reg[r]=*data;
    if(r==0x30) {
      reg[r]&=2;
      if(*data==3) reg[0x31]=1;
      if(*data==0) { reg[0x31]=0; for(size_t i=0x32;i<=0x41;i++) reg[i]=0; }
    }
    return i2c::ERROR_OK;
  }
  void done(uint16_t base=300) {
    reg[0x31]=2;
    for(uint8_t i=0;i<8;i++) { uint16_t v=base+i; reg[0x32+2*i]=v&255; reg[0x33+2*i]=v>>8; }
  }
  int reads(uint8_t r) const { int n=0; for(const auto &o:ops) n+=!o.write && o.reg==r; return n; }
};
void init_reader(CPLDReadout &reader,I2CBus &bus,Fixture *fixture=nullptr,bool adc=false) {
  reader.set_i2c_bus(&bus);
  if(fixture) reader.set_gpio(&fixture->gpio);
  if(adc) { reader.configure_adc(10,500); reader.set_adc_enabled(true); }
  reader.setup(); assert(!reader.is_failed());
}
int groups=0;
void gpio_tests() {
  Fixture f(0x10);
  assert(f.gpio.ready()); assert(f.bus.reg[0x27]==0x77);
  assert(f.bus.reg[0x21]==0xA5 && f.bus.reg[0x22]==0x5A && f.bus.reg[0x23]==0xA6);
  auto writes=f.bus.writes(); assert(writes.size()==1 && writes[0].tx==std::vector<uint8_t>({0x14,0x77}));
  assert(!f.gpio.configure_pin(11,gpio::FLAG_OUTPUT)); assert(!f.gpio.configure_pin(15,gpio::FLAG_OUTPUT));
  assert(f.gpio.configure_pin(8,gpio::FLAG_OUTPUT|gpio::FLAG_OPEN_DRAIN));
  writes=f.bus.writes();
  size_t n=writes.size();
  assert(writes[n-4].tx[0]==0x14 && writes[n-3].tx[0]==0x11 && writes[n-2].tx[0]==0x16 && writes[n-1].tx[0]==0x14);
  assert((writes[n-4].tx[1]&1)==0 && (writes[n-1].tx[1]&1)==1);
  assert(f.gpio.write_pin(0,false)); assert(f.bus.reg[0x21]==0xA4);
  bool pad=false; assert(f.gpio.read_pin(2,pad) && pad); // Pad is high, latch bit2 also high.
  assert(f.gpio.read_pin(0,pad) && !pad);
  assert(f.gpio.write_pin(0,true)); assert(f.gpio.read_pin(0,pad) && !pad); // DATA=1 is not pad=1.
  int owner1,owner2;
  assert(f.gpio.reserve_upper(&owner1)); assert(!f.gpio.reserve_upper(&owner2));
  size_t frames=f.bus.frames.size();
  assert(!f.gpio.write_pin(8,true)); assert(!f.gpio.configure_pin(9,gpio::FLAG_OUTPUT));
  assert(f.bus.frames.size()==frames); assert(f.gpio.write_pin(5,true));
  f.gpio.release_upper(&owner2); assert(f.gpio.upper_reserved());
  f.gpio.release_upper(&owner1); assert(f.gpio.write_pin(8,true));
  assert(!f.gpio.configure_pin(9,gpio::FLAG_INPUT|gpio::FLAG_OUTPUT));
  Fixture unavailable(0); assert(!unavailable.gpio.ready());
  Fixture failed_bank;
  CPLDFlash flash; failed_bank.init_flash(flash); assert(flash.acquire(true));
  failed_bank.bus.ignore_data=true;
  assert(!failed_bank.gpio.write_pin(0,false));
  failed_bank.bus.ignore_data=false;
  assert(!failed_bank.gpio.write_pin(5,true)); // No byte update from stale shadow after failure.
  assert(failed_bank.gpio.write_pin(8,true)); // Unrelated bank still works.
  assert(flash.release()); // CFG cleanup remains available even when a GPIO bank failed.
  failed_bank.gpio.setup(); assert(failed_bank.gpio.write_pin(0,false)); // Explicit resynchronization restores the bank.
  groups+=5;
}
void adc_tests() {
  Fixture f; I2CBus bus; CPLDReadout r; sensor::Sensor raw[8]; text_sensor::TextSensor status,id;
  for(uint8_t i=0;i<8;i++) r.set_adc_raw(i,&raw[i]);
  r.set_status(&status); r.set_adc_id(0,&id); r.add_adc_band(0,300,350,"known");
  init_reader(r,bus,&f,true); assert(!f.gpio.upper_reserved());
  r.update(); assert(f.gpio.upper_reserved() && bus.reg[0x30]==2);
  assert(!f.gpio.write_pin(8,true)); assert(f.gpio.write_pin(0,true));
  bus.done(); bool burst_before_disable=false;
  bus.after_read=[&](uint8_t reg) { if(reg==0x32) { burst_before_disable=bus.reg[0x30]==2; assert(raw[0].states.empty()); } };
  raw[0].callback=[&](float value) { if(!std::isnan(value)) { assert(!f.gpio.upper_reserved()); assert(bus.reg[0x30]==0); } };
  test_millis+=5; r.loop();
  assert(burst_before_disable && bus.reads(0x32)==1);
  for(uint8_t i=0;i<8;i++) assert(raw[i].states.back()==300+i);
  assert(id.states.back()=="known" && !f.gpio.upper_reserved() && bus.reg[0x32]==0);
  for(const auto &op:bus.ops) if(!op.write && op.reg==0x32) assert(op.size==16);
  bus.after_read={};
  // Timeout across uint32_t millis rollover, followed by complete cleanup.
  test_millis=0xFFFFFF00; r.update(); test_millis+=501; r.loop();
  assert(std::isnan(raw[0].states.back()) && !f.gpio.upper_reserved() && status.states.back()=="adc_timeout");
  // Failed burst is never published as a valid partial frame or zero.
  r.update(); bus.done(); bus.fail_read_reg=0x32; bus.fail_reads=1; test_millis+=5; r.loop();
  assert(std::isnan(raw[7].states.back()) && status.states.back()=="adc_burst_failed");
  // Disable failure keeps lease, rejects writes, and postpones publication until retry confirmation.
  bus.fail_read_reg=0xFF; r.update(); bus.done(400);
  size_t published=raw[0].states.size(); bus.fail_write_reg=0x30; bus.fail_writes=1;
  test_millis+=5; r.loop(); assert(f.gpio.upper_reserved() && raw[0].states.size()==published);
  assert(!f.gpio.write_pin(8,true)); test_millis+=100; r.loop();
  assert(!f.gpio.upper_reserved() && raw[0].states.back()==400);
  // Bad CTRL readback on start still follows disable/release path.
  bus.fail_read_reg=0x30; bus.fail_reads=1; r.update();
  assert(!f.gpio.upper_reserved() && status.states.back()=="adc_start_failed");
  bus.fail_read_reg=0xFF; r.update(); bus.done(1024); test_millis+=5; r.loop();
  assert(std::isnan(raw[0].states.back()) && status.states.back()=="adc_code_out_of_range");
  // Successful disable write but failed readback retains reservation until explicit confirmation.
  r.update(); bus.done(500);
  bus.after_read = [&](uint8_t reg) { if (reg == 0x32) { bus.fail_read_reg=0x30; bus.fail_reads=1; } };
  test_millis += 5; r.loop(); assert(f.gpio.upper_reserved() && bus.reg[0x30]==0);
  bus.after_read = {}; test_millis += 100; r.loop();
  assert(!f.gpio.upper_reserved() && raw[0].states.back()==500);
  // Sensor callbacks must not begin another conversion halfway through publishing a frame.
  raw[0].callback = [&](float) { r.update(); };
  bus.fail_read_reg=0xFF; r.update(); bus.done(600); test_millis += 5; r.loop();
  assert(!f.gpio.upper_reserved() && raw[7].states.back()==607 && bus.reg[0x30]==0);
  raw[0].callback = {};
  // Shutdown aborts without publishing a fabricated conversion.
  r.update(); r.on_shutdown(); assert(!f.gpio.upper_reserved() && bus.reg[0x30]==0);
  groups+=9;
}
void readout_tests() {
  I2CBus bus; CPLDReadout r; sensor::Sensor count,pwr,err,fault; text_sensor::TextSensor id;
  r.set_counter(0,&count); r.set_numeric(2,&pwr); r.set_numeric(3,&err); r.set_numeric(7,&fault);
  r.add_digital_id(true,0x70,4,0x70,&id); r.add_digital_code(0,5,"mezz5");
  init_reader(r,bus);
  bus.reg[0x10]=255; r.update(); assert(count.states.back()==255 && bus.reg[0x10]==0);
  assert(pwr.states.back()==5 && err.states.back()==11 && fault.states.back()==0x8F);
  assert(id.states.back()=="mezz5");
  r.update(); assert(count.states.back()==0);
  bus.fail_read_reg=0x10; bus.fail_reads=1; bus.reg[0x10]=9;
  int reads=bus.reads(0x10); r.update(); assert(bus.reads(0x10)==reads+1 && std::isnan(count.states.back()));
  for(const auto &op:bus.ops) assert(op.reg<0x30); // Without explicit ADC activation, only the I2C readout block is touched.
  I2CBus other_board; other_board.reg[0]=0x42; other_board.reg[1]=0x99;
  CPLDReadout generic; init_reader(generic,other_board);
  assert(!generic.is_failed()); // All PROJECT_ID/REV_ID values are data for YAML.
  // ADC readout is also valid without a GPIO/SPI component or reservation helper.
  I2CBus standalone; CPLDReadout adc; sensor::Sensor raw;
  adc.set_adc_raw(0,&raw); init_reader(adc,standalone,nullptr,true);
  adc.update(); standalone.done(1); test_millis+=5; adc.loop();
  assert(raw.states.back()==1 && standalone.reg[0x30]==0);
  groups+=4;
}
void flash_tests() {
  Fixture f; CPLDFlash flash; f.init_flash(flash); assert(!flash.is_failed());
  uint8_t tx[]={0x9F,0,0,0},rx[4]{};
  assert(!flash.acquire(false)); assert(f.bus.reg[0x23]==0xA6);
  assert(flash.acquire(true)); assert(f.bus.reg[0x23]==0xA7);
  assert(!f.gpio.write_pin(1,true) && !f.gpio.write_pin(2,true) && !f.gpio.write_pin(3,true) && !f.gpio.write_pin(4,true));
  assert(f.gpio.write_pin(5,true));
  assert(!f.gpio.set_target_controls(0));
  f.bus.during_flash=[&]() { assert(!flash.release()); assert(f.bus.reg[0x23]==0xA7); };
  assert(flash.transfer(tx,rx,4)); assert(f.bus.cs.value && f.bus.reg[0x23]==0xA7 && rx[0]==0x60);
  f.bus.during_flash={};
  assert(flash.release()); assert(f.bus.cs.value && f.bus.reg[0x23]==0xA6);
  assert(f.bus.frames[f.bus.frames.size()-4].channel==3);
  assert(f.gpio.set_target_controls(0)); assert(f.bus.reg[0x23]==0xA0);
  uint8_t previous_dir = f.bus.reg[0x26], previous_od = f.bus.reg[0x28];
  assert(f.gpio.configure_target_controls(0, 0x06, 0x02));
  assert(f.bus.reg[0x26] == uint8_t((previous_dir & ~6) | 6));
  assert(f.bus.reg[0x28] == uint8_t((previous_od & ~6) | 2));
  assert(!f.gpio.configure_target_controls(1, 6, 0));
  assert(flash.transaction(tx,rx,4,true)); assert(f.bus.reg[0x23]==0xA0);
  assert(!flash.transaction(nullptr,rx,4,true) && f.bus.reg[0x23]==0xA0); // Invalid-buffer cleanup.
  // Once acquired, failed release retains owner and retries without changing reset/boot levels.
  assert(flash.acquire(true)); f.bus.ignore_release=true;
  assert(!flash.release() && flash.owned()); assert(f.bus.reg[0x23]==0xA1);
  assert(!flash.transfer(tx,rx,4)); f.bus.ignore_release=false; test_millis+=100; flash.loop();
  assert(!flash.owned() && f.bus.reg[0x23]==0xA0);
  // Failed acquire verification after a successful hardware write also retains cleanup obligation.
  f.bus.corrupt_after_acquire=2; f.bus.ignore_release=true;
  assert(!flash.acquire(true) && flash.owned() && f.bus.reg[0x23]==0xA1);
  f.bus.corrupt_after_acquire=0; f.bus.ignore_release=false; test_millis+=100; flash.loop();
  assert(!flash.owned() && f.bus.reg[0x23]==0xA0);
  // A CPLD reset/lost ownership between acquire and transfer must not run a CS2 transaction.
  assert(flash.acquire(true)); f.bus.reg[0x23] &= ~1;
  size_t before = f.bus.frames.size();
  assert(!flash.transfer(tx,rx,4) && !flash.owned());
  for(size_t i=before; i<f.bus.frames.size(); i++) assert(f.bus.frames[i].channel==3);
  // ADC owns upper bank while Flash owns lower pins/CFG, without bus held during conversion.
  I2CBus i2c; CPLDReadout r; init_reader(r,i2c,&f,true); r.update(); assert(f.gpio.upper_reserved());
  assert(flash.transaction(tx,rx,4,true)); assert(f.gpio.upper_reserved());
  i2c.done(); test_millis+=5; r.loop(); assert(!f.gpio.upper_reserved());
  // Separate addrspi clients maintain their own device modes.
  spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST,spi::CLOCK_POLARITY_LOW,spi::CLOCK_PHASE_TRAILING,static_cast<spi::SPIDataRate>(100000)> clients[2];
  for(uint8_t i=0;i<2;i++) { clients[i].set_spi_parent(&f.channels[i]); clients[i].spi_setup(); clients[i].enable(); clients[i].transfer_byte(0x12); clients[i].disable(); }
  auto n=f.bus.frames.size(); assert(f.bus.frames[n-2].channel==0 && f.bus.frames[n-2].mode==spi::MODE1);
  assert(f.bus.frames[n-1].channel==1 && f.bus.frames[n-1].mode==spi::MODE1);
  bool saw_flash_mode3=false; for(const auto &frame:f.bus.frames) if(frame.channel==2) { assert(frame.mode==spi::MODE3); saw_flash_mode3=true; }
  assert(saw_flash_mode3);
  // Already-owned hardware must not be stolen or cleared by a failed acquire.
  f.bus.reg[0x23]=0xA1; assert(!flash.acquire(true)); assert(f.bus.reg[0x23]==0xA1);
  Fixture no_flash(0x10); CPLDFlash unavailable; no_flash.init_flash(unavailable); assert(!unavailable.acquire(true));
  groups+=7;
}
void nested_address_test() {
  SPIBus bus;
  TestPin ga[4];
  addrspi::ADDRSPIComponent module_mux, chip_mux;
  addrspi::ADDRSPIChannel module9, chip3, chip2;
  module_mux.set_spi_parent(&bus);
  module_mux.set_address_pins({&ga[0],&ga[1],&ga[2],&ga[3]}); module_mux.setup();
  module9.set_parent(&module_mux); module9.set_channel(9);
  chip_mux.set_spi_parent(&module9);
  chip_mux.set_address_pins({&bus.address[0],&bus.address[1]}); chip_mux.setup();
  chip3.set_parent(&chip_mux); chip3.set_channel(3);
  chip2.set_parent(&chip_mux); chip2.set_channel(2);
  bus.before_begin = [&]() {
    assert(ga[0].value && !ga[1].value && !ga[2].value && ga[3].value);
  };
  CPLDGPIO gpio; gpio.set_initial_enabled(true); gpio.set_spi_parent(&chip3); gpio.set_input_masks(0,0xFF); gpio.setup();
  assert(gpio.ready());
  CPLDFlash flash; flash.set_enabled(true); flash.set_gpio(&gpio); flash.set_spi_parent(&chip2); flash.setup();
  uint8_t tx[]={0x9F,0},rx[2];
  assert(flash.transaction(tx,rx,2,true));
  assert(rx[0]==0x60 && bus.reg[0x23]==0xA6);
  groups++;
}
void yaml_activation_tests() {
  Fixture f;
  assert(f.gpio.set_enabled(false));
  f.bus.frames.clear();
  CPLDFlash flash; flash.set_gpio(&f.gpio); flash.set_spi_parent(&f.channels[2]); flash.setup();
  assert(!flash.acquire(true) && !f.gpio.read_pin(0,f.bus.address[0].value));
  assert(!f.gpio.write_pin(0,true) && f.bus.frames.empty());
  moduliq_cpld_gpio::CPLDGPIOPin pin; pin.set_parent(&f.gpio); pin.set_pin(5);
  pin.set_flags(gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN); pin.setup(); pin.digital_write(false);
  assert(f.bus.frames.empty());
  assert(f.gpio.set_enabled(true));
  assert((f.bus.reg[0x21]&0x20)==0 && (f.bus.reg[0x26]&0x20) && (f.bus.reg[0x28]&0x20));
  flash.set_enabled(true); assert(flash.acquire(true));
  assert(!f.gpio.set_enabled(false)); // Disabling the shared owner cannot abandon an active lease.
  assert(flash.set_enabled(false)); assert(f.gpio.set_enabled(false));
  f.bus.frames.clear();
  assert(!flash.acquire(true) && f.bus.frames.empty());
  assert(f.gpio.set_enabled(true)); flash.set_enabled(true); assert(flash.acquire(true));
  f.bus.during_flash = [&]() { assert(!flash.set_enabled(false)); };
  uint8_t tx[]={0x9F,0},rx[2]; assert(flash.transfer(tx,rx,2));
  f.bus.during_flash = {}; test_millis += 100; flash.loop();
  assert(!flash.owned() && (f.bus.reg[0x23]&1)==0);
  I2CBus bus; CPLDReadout r; r.configure_adc(10,500); init_reader(r,bus);
  r.update(); for (auto &op : bus.ops) assert(op.reg < 0x30);
  assert(r.set_adc_enabled(true)); r.update();
  bus.fail_write_reg=0x30; bus.fail_writes=1;
  assert(!r.set_adc_enabled(false)); // Cancellation must continue cleanup even after software deactivation.
  test_millis += 100; r.loop(); assert(bus.reg[0x30]==0);
  bus.ops.clear(); r.update(); for (auto &op : bus.ops) assert(op.reg < 0x30);
  // A fresh disabled GPIO never even registers a device or probes a disabled expander.
  auto registrations = f.bus.delegates.size();
  CPLDGPIO untouched; untouched.set_spi_parent(&f.channels[3]); untouched.setup();
  assert(f.bus.delegates.size() == registrations);
  f.bus.frames.clear(); assert(!untouched.ready()); assert(!untouched.write_pin(1,true));
  assert(f.bus.frames.empty());
  groups += 3;
}

#ifndef SERPROG_HARNESS
int main() {
  gpio_tests(); adc_tests(); readout_tests(); flash_tests(); nested_address_test(); yaml_activation_tests();
  std::cout << "PASS: " << groups << " CPLD C++ scenario groups; actual drivers + addrspi; no hardware\n";
}

#endif
