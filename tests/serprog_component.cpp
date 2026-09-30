#define SERPROG_HARNESS
#include "cpld_components.cpp"
#include "esphome/components/moduliq_serprog/moduliq_serprog.h"
#include <chrono>
#include <thread>
using moduliq_serprog::Serprog;
struct Server : Serprog {
  bool idle() const { return state_==IDLE; }
  bool preparing() const { return state_==PREPARING; }
  bool listening() const { return bool(listener_); }
  uint32_t generation() const { return session_; }
};
int connect_client(int port) {
  int fd=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); assert(fd>=0);
  sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  assert(::connect(fd,reinterpret_cast<sockaddr *>(&addr),sizeof(addr))==0);
  fcntl(fd,F_SETFL,O_NONBLOCK); return fd;
}
void tick(Server &s,uint32_t ms=10) { test_millis+=ms; s.loop(); }
void send_bytes(int fd,std::initializer_list<uint8_t> bytes) {
  std::vector<uint8_t> b(bytes); assert(::send(fd,b.data(),b.size(),MSG_NOSIGNAL)==ssize_t(b.size()));
}
void shared_bus_sessions() {
  SPIBus bus; TestPin ga[4];
  addrspi::ADDRSPIComponent modules, chip_mux[2];
  addrspi::ADDRSPIChannel module_channels[2], chips[2][2];
  modules.set_spi_parent(&bus); modules.set_address_pins({&ga[0],&ga[1],&ga[2],&ga[3]}); modules.setup();
  // Two independent module register banks on one physical SPI bus, with shared CS address wires.
  std::array<std::array<uint8_t,256>,2> banks={bus.reg,bus.reg};
  uint8_t selected=0;
  bus.before_begin=[&]() {
    uint8_t module=ga[0].value; assert(!ga[1].value && !ga[2].value && !ga[3].value);
    if(module!=selected) { banks[selected]=bus.reg; bus.reg=banks[module]; selected=module; }
  };
  CPLDGPIO gpio[2]; CPLDFlash flash[2]; int owners[2];
  for(uint8_t i=0;i<2;i++) {
    module_channels[i].set_parent(&modules); module_channels[i].set_channel(i);
    chip_mux[i].set_spi_parent(&module_channels[i]); chip_mux[i].set_address_pins({&bus.address[0],&bus.address[1]}); chip_mux[i].setup();
    for(uint8_t j=0;j<2;j++) { chips[i][j].set_parent(&chip_mux[i]); chips[i][j].set_channel(j+2); }
    gpio[i].set_initial_enabled(true); gpio[i].set_spi_parent(&chips[i][1]); gpio[i].set_cs_pin(&bus.cs); gpio[i].setup();
    flash[i].set_enabled(true); flash[i].set_gpio(&gpio[i]); flash[i].set_spi_parent(&chips[i][0]); flash[i].set_cs_pin(&bus.cs);
    assert(flash[i].reserve_session(&owners[i])); assert(flash[i].acquire_session(&owners[i],true));
  }
  uint8_t command=0x9f,answer[3];
  for(int n=0;n<6;n++) {
    auto i=n%2; assert(flash[i].transfer_session(&owners[i],&command,1,answer,3));
    assert(selected==i && bus.frames.back().channel==2 && bus.frames.back().tx==std::vector<uint8_t>({0x9f,0,0,0}));
    assert(flash[0].owned() && flash[1].owned());
  }
  assert(flash[0].release_session(&owners[0],true)); assert(flash[1].owned());
  assert(flash[1].transfer_session(&owners[1],&command,1,answer,3));
  assert(flash[1].release_session(&owners[1],true));
  for(uint8_t i=0;i<2;i++) assert(flash[i].end_session(&owners[i]));
}
int main() {
  shared_bus_sessions();
  Fixture f; CPLDFlash flash; f.init_flash(flash);
  Server server; server.set_flash(&flash); server.set_port(16054); server.setup();
  tick(server,1000); assert(!server.listening()); // disabled means no listener, no SPI
  server.set_enabled(true); tick(server,1000); assert(server.listening());
  int fd=connect_client(16054); tick(server); assert(server.preparing());
  auto generation=server.generation();
  assert(!flash.acquire(true) && !flash.release()); // memory reserved before preparation
  int competitor=connect_client(16054); tick(server); uint8_t buffer[32];
  assert(::recv(competitor,buffer,sizeof(buffer),0)==0); ::close(competitor);
  size_t before=f.bus.frames.size();
  send_bytes(fd,{19,1,0,0,3,0,0,0x9f}); tick(server);
  assert(f.bus.frames.size()==before && !flash.owned()); // waits, no premature ACK or takeover
  assert(::recv(fd,buffer,sizeof(buffer),0)<0 && errno==EAGAIN);
  assert(!server.confirm_target_ready(generation+1));
  assert(server.confirm_target_ready(generation)); tick(server);
  assert(::recv(fd,buffer,sizeof(buffer),0)==4 && buffer[0]==6);
  auto frame=f.bus.frames.back(); assert(frame.channel==2 && frame.tx==std::vector<uint8_t>({0x9f,0,0,0}));
  assert(f.bus.cs.value && flash.owned()); // single CS frame for both phases
  assert(!flash.set_enabled(false) && flash.owned()); // administrative disable cannot abandon busy session
  flash.set_enabled(true);
  ::close(fd); tick(server); assert(server.release_pending() && flash.owned());
  tick(server,600000); assert(flash.owned()); // no guessed timeout hands a programming chip back
  assert(!server.confirm_release_ready(generation+1));
  uint8_t sr_command=5,sr=0;
  assert(server.cleanup_transfer(generation,&sr_command,1,&sr,1));
  f.bus.ignore_release=true; assert(server.confirm_release_ready(generation)); tick(server,100);
  assert(server.release_pending() && flash.owned());
  f.bus.ignore_release=false; tick(server,100); assert(server.idle() && !flash.owned());
  assert(!server.confirm_target_ready(generation));
  // Partial frames never clock SPI; cancelled preparation restores YAML only after lease release.
  fd=connect_client(16054); tick(server); auto next=server.generation(); assert(next!=generation);
  send_bytes(fd,{19,1}); tick(server); ::close(fd); tick(server); tick(server,100);
  assert(server.idle() && !flash.owned());
  // Disable during a real transfer leaves cleanup available and closes the listener.
  fd=connect_client(16054); tick(server); next=server.generation(); assert(server.confirm_target_ready(next));
  send_bytes(fd,{19,1,0,0,0,0,0,6}); tick(server);
  server.set_enabled(false); assert(server.release_pending() && flash.owned() && !server.listening());
  assert(server.confirm_release_ready(next)); tick(server,100); assert(server.idle()); ::close(fd);
  // Driver-level owner checks and bus guard across two memory instances.
  int owner,other; assert(flash.reserve_session(&owner)); assert(!flash.reserve_session(&other));
  assert(!flash.acquire_session(&other,true)); assert(flash.acquire_session(&owner,true));
  assert(!flash.transfer_session(&other,&sr_command,1,&sr,1));
  assert(flash.transfer_session(&owner,&sr_command,1,&sr,1));
  assert(!flash.release_session(&owner,false)); assert(flash.release_session(&owner,true)); assert(flash.end_session(&owner));
  Fixture other_module; CPLDFlash second; other_module.init_flash(second); assert(second.acquire(true));
  assert(flash.acquire(true));
  size_t other_frames=other_module.bus.frames.size();
  f.bus.during_flash=[&]() {
    assert(!second.transfer(&sr_command,&sr,1));
    assert(!second.release());
    assert(other_module.bus.frames.size()==other_frames);
  };
  assert(flash.transfer(&sr_command,&sr,1)); f.bus.during_flash={};
  assert(flash.release() && second.release());
  // A refused preparation expires even while negotiation commands keep arriving.
  server.set_enabled(true); tick(server,1000); fd=connect_client(16054); tick(server);
  auto timed_out=server.generation();
  send_bytes(fd,{0}); tick(server,10000); send_bytes(fd,{0}); tick(server,10000);
  send_bytes(fd,{0}); tick(server,10000); tick(server,100);
  assert(server.idle() && !server.confirm_target_ready(timed_out)); ::close(fd);
  // Configuration rejection drains NAK before closing and never clocks the payload.
  fd=connect_client(16054); tick(server); assert(server.confirm_target_ready(server.generation()));
  before=f.bus.frames.size(); send_bytes(fd,{19,1,4,0,0,0,0}); tick(server);
  assert(::recv(fd,buffer,sizeof(buffer),0)==1 && buffer[0]==21);
  tick(server,100); assert(server.idle());
  for(size_t i=before;i<f.bus.frames.size();i++) assert(f.bus.frames[i].channel!=2);
  ::close(fd);
  std::cout << "PASS: actual TCP component + Flash + addrspi; readiness, CS, lease, busy disconnect, disable, release failure\n";
}
