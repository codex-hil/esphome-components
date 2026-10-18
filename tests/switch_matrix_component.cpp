#include "esphome/components/switch_matrix_32_2/switch_matrix_32_2.h"
#include <cassert>
#include <iostream>
using namespace esphome;
using namespace esphome::switch_matrix_32_2;
// Both 595 chains shift together; each rising latch edge captures its own word.
struct Hardware {
 uint32_t shift=0, a=0, b=0; bool enabled=false;
 void clock(uint8_t byte) { shift=(shift << 8) | byte; }
};
struct Pin : GPIOPin {
 Hardware &hw; char role; bool level=true;
 Pin(Hardware &h, char r) : hw(h), role(r) {}
 void digital_write(bool value) override {
  if(role=='o') hw.enabled=!value;
  else if(!level && value) { if(role=='a') hw.a=hw.shift; else hw.b=hw.shift; }
  level=value;
  assert(!(hw.a & hw.b));
 }
};
int main() {
 uint32_t v=123;
 for(auto s : {"", "0x", "100000000", "-1", "+1", " 1", "1 ", "0xGG", "1junk"}) {
  assert(!Matrix::parse_mask(s,v)); assert(v==123);
 }
 assert(Matrix::parse_mask("0xFFFFFFFF",v) && v==0xffffffffu);
 assert(Matrix::parse_mask("80000000",v) && v==0x80000000u);
 Hardware hw; Pin a(hw,'a'), b(hw,'b'), oe(hw,'o'); Matrix m;
 MatrixSelect first,last; first.set_parent(&m); first.set_channel(1);
 last.set_parent(&m); last.set_channel(32); m.register_select(&first); m.register_select(&last);
 m.set_cs_a_pin(&a); m.set_cs_b_pin(&b); m.set_oe_pin(&oe);
 m.transfer=[&](uint8_t x) { hw.clock(x); };
 assert(!m.set_masks(1u,0u));
 m.setup(); assert(hw.enabled && hw.a==0 && hw.b==0);
 assert(first.state=="BYPASS" && last.state=="BYPASS");
 assert(m.set_masks("0x00000005","0x8000000A"));
 assert(hw.a==0 && hw.b==0 && m.timeout_ms==10);
 m.finish(); assert(hw.a==5 && hw.b==0x8000000au);
 assert(first.state=="A" && last.state=="B");
 assert(!m.set_masks(1u,1u)); assert(!m.set_masks("bad mask","0"));
 assert(hw.a==5 && hw.b==0x8000000au);
 first.command("B"); assert(hw.a==4 && hw.b==0x8000000au);
 assert(first.state=="BYPASS");
 // A second single-channel command must retain the first command's pending target.
 last.command("A"); assert(hw.a==4 && hw.b==10);
 m.finish(); assert(hw.a==0x80000004u && hw.b==11);
 assert(first.state=="B" && last.state=="A");
 // Rapid replacement, rejected command must not cancel the valid pending target.
 assert(m.set_masks(1u,0u)); assert(!m.set_masks(2u,2u)); m.finish();
 assert(hw.a==1 && hw.b==0);
 for(unsigned ch=1;ch<=32;ch++) {
  assert(m.set_channel(ch,"B")); m.finish();
  assert(hw.b==((ch==32)?0xffffffffu:((1u<<ch)-1)));
 }
 assert(hw.a==0); assert(!m.set_channel(0,"A")); assert(!m.set_channel(33,"A"));
 assert(!m.set_channel(1,"invalid"));
 SetMasksAction<> action(&m); action.set_mask_a(std::string("80000000"));
 action.set_mask_b(std::string("1")); action.play(); m.finish();
 assert(hw.a==0x80000000u && hw.b==1);
 SetMasksAction<std::string, std::string> api(&m);
 api.set_mask_a(std::function<std::string(std::string,std::string)>([](auto x,auto) { return x; }));
 api.set_mask_b(std::function<std::string(std::string,std::string)>([](auto,auto y) { return y; }));
 api.play("2", "4"); m.finish(); assert(hw.a==2 && hw.b==4);
 BypassAllAction<> bypass(&m); bypass.play(); m.finish(); assert(hw.a==0 && hw.b==0);
 m.set_masks(1u,0u); m.on_shutdown(); m.finish();
 assert(!hw.enabled && hw.a==0 && hw.b==0); assert(!m.set_masks(1u,0u));
 std::cout << "PASS: shared shift chains, all 32 bits, interlocks, replacement, parser, selects, shutdown\n";
}
