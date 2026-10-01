#include <cassert>
#include <cmath>
#include <vector>
#include "esphome/components/mmc5983_spi/mmc5983_spi.h"
namespace esphome { uint32_t test_millis=0; }
using namespace esphome;
struct Bus : spi::SPIComponent, spi::SPIDelegate {
  uint8_t regs[64]={};
  std::vector<std::vector<uint8_t>> frames;
  std::vector<uint8_t> frame;
  bool active=false; unsigned writes=0; unsigned bad_initial_ids=0;
  Bus() { regs[0x2F]=0x30; regs[8]=0x10; regs[0]=0x80; regs[2]=0x80; regs[4]=0x80; regs[6]=0x6C; regs[7]=125; }
  spi::SPIDelegate *register_device(spi::SPIClient *,spi::SPIMode mode,spi::SPIBitOrder,uint32_t rate,GPIOPin *,bool,bool) override {
    assert(mode==spi::MODE3 && rate==100000); return this;
  }
  void begin_transaction() override { assert(!active); active=true; frame.clear(); }
  void end_transaction() override { assert(active && (frame.size()==2 || frame.size()==8)); active=false; frames.push_back(frame); }
  uint8_t transfer(uint8_t data) override {
    assert(active); frame.push_back(data); if(frame.size()==1) return 0;
    uint8_t reg=(frame[0]&0x3F)+frame.size()-2;
    if(frame[0]&0x80) { if(reg==0x2F && bad_initial_ids) { bad_initial_ids--; return 0xFF; } return regs[reg]; }
    writes++;
    if(reg==9 && data==1) regs[8]=0x11;
    if(reg==9 && data==2) regs[8]=0x12;
    return 0;
  }
};
int main() {
  { Bus b; b.bad_initial_ids=1; mmc5983_spi::MMC5983SPIComponent d; d.set_spi_parent(&b); d.setup(); assert(!d.is_failed() && b.writes==5); }
  { Bus b; b.regs[8]=0xFF; mmc5983_spi::MMC5983SPIComponent d; d.set_spi_parent(&b); d.setup(); assert(d.is_failed() && b.writes==1); }
  { Bus b; b.regs[0x2F]=0xFF; mmc5983_spi::MMC5983SPIComponent d; d.set_spi_parent(&b); d.setup(); d.update(); assert(d.is_failed() && b.writes==0 && b.frames.size()==5); }
  { Bus b; b.regs[8]=0; mmc5983_spi::MMC5983SPIComponent d; d.set_spi_parent(&b); d.setup(); assert(d.is_failed()); assert(b.writes==1); }
  { Bus b; mmc5983_spi::MMC5983SPIComponent d; sensor::Sensor x,y,z,t; d.set_spi_parent(&b); d.set_x_sensor(&x); d.set_y_sensor(&y); d.set_z_sensor(&z); d.set_temperature_sensor(&t); d.setup(); assert(!d.is_failed()); d.update();
    assert(x.states.size()==1 && y.states.size()==1 && z.states.size()==1 && t.states.size()==1);
    assert(std::abs(x.states[0]-1.0f/16384)<1e-7 && std::abs(y.states[0]-2.0f/16384)<1e-7 && std::abs(z.states[0]-3.0f/16384)<1e-7);
    assert(t.states[0]==25.0f && !d.warning());
    unsigned bursts=0; for(auto &f:b.frames) if(f.size()==8) { bursts++; assert(f[0]==0x80); } assert(bursts==1);
    b.regs[0x2F]=0; auto writes=b.writes; d.update(); assert(d.warning() && b.writes==writes && x.states.size()==1);
  }
  { Bus b; mmc5983_spi::MMC5983SPIComponent d; sensor::Sensor x; d.set_spi_parent(&b); d.set_x_sensor(&x); d.setup(); b.regs[6]=0xFF; d.update(); assert(d.warning() && x.states.empty()); }
}
