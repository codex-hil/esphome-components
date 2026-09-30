// Production TCP server, Flash and addrspi with inert GPIO/SPI; only the NOR is emulated.
#define SERPROG_HARNESS
#include "cpld_components.cpp"
#include "esphome/components/moduliq_serprog/moduliq_serprog.h"
#include <chrono>
#include <thread>
#include <csignal>
#include <fstream>
static volatile std::sig_atomic_t running=1;
static void stop(int) { running=0; }
struct NOR {
  std::vector<uint8_t> memory=std::vector<uint8_t>(128*1024,0xff);
  bool wel=false;
  uint32_t busy_until=0;
  unsigned reads=0, programs=0, erases=0;
  bool busy() const { return int32_t(busy_until-esphome::test_millis)>0; }
  uint8_t status() const { return uint8_t(busy() ? 1 : 0) | (wel ? 2 : 0); }
  static size_t addr(const std::vector<uint8_t> &v) { return (size_t(v[1])<<16) | (size_t(v[2])<<8) | v[3]; }
  uint8_t read(const std::vector<uint8_t> &v) {
    size_t i=v.size()-1;
    if(v[0]==0x9f && i>=1) { const uint8_t id[]={0xef,0x30,0x11}; return i<=3 ? id[i-1] : 0xff; }
    if(v[0]==5 && i>=1) return status();
    if(v[0]==3 && i>=4) { assert(!busy()); return memory[(addr(v)+i-4)%memory.size()]; }
    return 0xff;
  }
  void end(const std::vector<uint8_t> &v) {
    if(v.empty()) return;
    if(v[0]==6) { assert(!busy()); wel=true; }
    if(v[0]==4) wel=false;
    if(v[0]==3) reads++;
    if(v[0]==2 && v.size()>4) {
      assert(wel && !busy()); size_t a=addr(v); assert(a+v.size()-4<=memory.size());
      assert(a/256==(a+v.size()-5)/256);
      for(size_t i=4;i<v.size();i++) memory[a+i-4]&=v[i];
      programs++; wel=false; busy_until=esphome::test_millis+2;
    }
    if(v[0]==0x20 || v[0]==0xd8 || v[0]==0xc7 || v[0]==0x60) {
      assert(wel && !busy());
      size_t size=v[0]==0x20 ? 4096 : v[0]==0xd8 ? 65536 : memory.size();
      size_t start=size==memory.size() ? 0 : addr(v)/size*size;
      assert(start+size<=memory.size()); std::fill_n(memory.begin()+start,size,0xff);
      erases++; wel=false; busy_until=esphome::test_millis+1000;
    }
  }
};
int main(int argc,char **argv) {
  assert(argc==3); int port=std::stoi(argv[1]);
  std::signal(SIGTERM,stop); std::signal(SIGINT,stop);
  Fixture f; CPLDFlash flash; f.init_flash(flash); NOR nor;
  f.bus.flash_read=[&](const auto &v){ return nor.read(v); };
  f.bus.flash_end=[&](const auto &v){ nor.end(v); };
  moduliq_serprog::Serprog server; server.set_flash(&flash); server.set_port(port); server.set_enabled(true);
  unsigned released=0; uint32_t cleanup_session=0;
  server.get_prepare_trigger()->callback=[&](uint32_t generation) {
    assert(f.bus.cs.value); assert(server.confirm_target_ready(generation));
  };
  server.get_release_requested_trigger()->callback=[&](uint32_t generation) { cleanup_session=generation; };
  server.get_released_trigger()->callback=[&](uint32_t) { assert(!flash.owned() && !nor.busy()); released++; cleanup_session=0; };
  test_millis=1000; server.loop(); std::cout << "READY\n" << std::flush;
  auto start=std::chrono::steady_clock::now();
  while(running || server.release_pending()) {
    test_millis=1000+std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    server.loop();
    if(cleanup_session && server.release_pending()) {
      uint8_t command=5,status=0xff;
      if(server.cleanup_transfer(cleanup_session,&command,1,&status,1) && !(status&1))
        assert(server.confirm_release_ready(cleanup_session));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // Keep memory bounded for full-flash runs; assertions already checked complete CS frames.
    if(f.bus.frames.size()>1000) f.bus.frames.clear();
  }
  server.on_shutdown();
  std::ofstream image(argv[2],std::ios::binary); image.write(reinterpret_cast<const char *>(nor.memory.data()),nor.memory.size());
  std::cout << "{\"reads\":" << nor.reads << ",\"programs\":" << nor.programs << ",\"erases\":" << nor.erases << ",\"released\":" << released << "}\n";
}
