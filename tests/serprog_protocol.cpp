#include "esphome/components/moduliq_serprog/protocol.h"
#include <cassert>
#include <vector>
#include <iostream>
using namespace esphome::moduliq_serprog;
struct TestBackend : Backend {
  int calls=0;
  Result result=OK;
  std::vector<uint8_t> last;
  Result spi_operation(const uint8_t *tx,size_t w,uint8_t *rx,size_t r) override {
    if (result!=OK) return result;
    calls++; last.assign(tx,tx+w);
    for(size_t i=0;i<r;i++) rx[i]=uint8_t(i);
    return result;
  }
};
std::vector<uint8_t> request(Protocol &p,const std::vector<uint8_t> &bytes) {
  for(auto b:bytes) { assert(p.bytes_requested()); p.consume(&b,1); }
  std::vector<uint8_t> out(p.output(),p.output()+p.output_size());
  while(p.output_size()) p.output_consumed(1); // fragmented writes
  return out;
}
int main() {
  TestBackend backend; Protocol p(&backend);
  assert(request(p,{0x10})==std::vector<uint8_t>({0x15,6}));
  assert(request(p,{1})==std::vector<uint8_t>({6,1,0}));
  auto map=request(p,{2}); assert(map.size()==33);
  for(int c=0;c<256;c++) {
    bool expected=c<=5 || c==8 || c==16 || c==17 || c==18 || c==19;
    assert(bool(map[1+c/8]&(1<<(c%8)))==expected);
  }
  assert(request(p,{3}).size()==17);
  assert(request(p,{4})==std::vector<uint8_t>({6,255,255}));
  assert(request(p,{5})==std::vector<uint8_t>({6,8}));
  for(auto c:{8,17}) assert(request(p,{uint8_t(c)})==std::vector<uint8_t>({6,0,4,0}));
  assert(request(p,{18,8})==std::vector<uint8_t>({6}));
  assert(request(p,{18,1})==std::vector<uint8_t>({21}));
  assert(request(p,{19,1,0,0,3,0,0,0x9f})==std::vector<uint8_t>({6,0,1,2}));
  assert(backend.calls==1 && backend.last==std::vector<uint8_t>({0x9f}));
  assert(request(p,{19,0,0,0,1,0,0})==std::vector<uint8_t>({6,0}));
  assert(request(p,{19,1,0,0,0,0,0,6})==std::vector<uint8_t>({6}));
  std::vector<uint8_t> max={19,0,4,0,0,4,0}; max.resize(7+Protocol::MAX_WRITE,0xaa);
  auto out=request(p,max); assert(out.size()==1025 && backend.last.size()==1024);
  backend.result=Backend::WAIT;
  request(p,{19,1,0,0,1,0,0,5}); assert(!p.output_size() && !p.bytes_requested());
  p.process(); assert(!p.output_size());
  backend.result=Backend::OK; p.process(); assert(p.output_size()==2); p.output_consumed(2);
  backend.result=Backend::ERROR;
  assert(request(p,{19,1,0,0,1,0,0,5})==std::vector<uint8_t>({21}));
  for(auto frame:std::vector<std::vector<uint8_t>>{{19,1,4,0,0,0,0},{19,0,0,0,1,4,0},{19,0,0,0,0,0,0},{19,255,255,255,0,0,0},{255}}) {
    p.reset(); assert(request(p,frame)==std::vector<uint8_t>({21})); assert(p.close_requested());
  }
  p.reset(); uint8_t op=19; p.consume(&op,1); assert(p.bytes_requested()==6); p.reset();
  // Deterministic malformed streams exercise length parsing and bounded buffers under sanitizers.
  p.reset(); backend.result=Backend::OK; uint32_t rng=0x12345678;
  for(int i=0;i<20000;i++) {
    rng=rng*1664525+1013904223; uint8_t b=rng>>24;
    if(p.output_size()) p.output_consumed(p.output_size());
    if(p.close_requested()) p.reset();
    if(p.bytes_requested()) p.consume(&b,1); else p.process();
    assert(p.output_size()<=1+Protocol::MAX_READ && p.bytes_requested()<=7+Protocol::MAX_WRITE);
  }
  std::cout << "PASS: serprog negotiation, boundaries, fragmentation, pending readiness, rejection\n";
}
