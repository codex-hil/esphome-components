#include "../components/lmk61e2/planner.h"
#include <cassert>
#include <array>
#include <iostream>
#include <vector>
using namespace lmk61e2_core;
struct Fake:Transport {
 std::array<uint8_t,73> regs{};int call{},fail_at{-1};std::vector<uint8_t>written;
 Fake(){regs[0]=16;regs[1]=11;regs[2]=51;regs[21]=1;regs[23]=46;regs[26]=46;regs[32]=1;regs[33]=12;regs[34]=40;regs[35]=3;regs[36]=4;regs[42]=5;regs[48]=6;regs[49]=16;}
 bool fail(){return ++call==fail_at;}
 bool read(uint8_t r,uint8_t&v)override{if(fail())return false;v=regs[r];return true;}
 bool write(uint8_t r,uint8_t v)override{written.push_back(r);if(fail())return false;if(r!=72)regs[r]=v;return true;}
};
int main(){
 int plans=0;
 for(double hz:{1e7,2e7,2.5e7,5e7,8e7,1e8,122880000.0,125000000.0,156250000.0,2e8}){
  Plan p;assert(plan_frequency(hz,Format::LVPECL,p));assert(std::abs(p.actual_hz-hz)<.001);
  assert(p.outdiv>=5&&p.outdiv<=511&&p.denominator<=4194303);
  Fake io;Device d(io);assert(d.identify());assert(d.apply(p,Format::LVPECL,true));
  Plan decoded;Format f;bool enabled;assert(d.read_configuration(decoded,f,enabled));assert(std::abs(decoded.actual_hz-hz)<.001);assert(enabled);assert(io.regs[35]&3);
  for(uint8_t r:io.written)assert(r==21||r==72||r==22||r==23||(r>=25&&r<=39));
  ++plans;
 }
 for(double hz=10000001;hz<1000000000;hz+=1234567.125){Plan p;assert(plan_frequency(hz,Format::LVPECL,p));assert(std::abs(p.error_ppm)<.01);++plans;}
 Plan p;assert(!plan_frequency(900000001,Format::LVDS,p));assert(!plan_frequency(400000001,Format::HCSL,p));assert(!plan_frequency(NAN,Format::LVPECL,p));assert(!plan_frequency(9999999,Format::LVPECL,p));
 assert(plan_frequency(122880000,Format::LVPECL,p));assert(p.outdiv==41&&p.integer==50&&p.numerator==238&&p.denominator==625);
 // Inject one failed transaction at every snapshot/write/readback point.
 for(int position=1;position<=56;++position){Fake io;auto before=io.regs;io.fail_at=position;Device d(io);bool ok=d.apply(p,Format::LVPECL,true);if(!ok){assert(io.regs==before);assert(d.rollback_ok());}}
 Fake io;Device d(io);assert(d.set_output(Format::LVDS,false));assert(io.regs[21]==130);assert(d.set_output(Format::HCSL,true));assert(io.regs[21]==3);
 std::cout<<plans<<" plans, all 10 targets, format limits and 56 injected I/O failures passed\n";
}
