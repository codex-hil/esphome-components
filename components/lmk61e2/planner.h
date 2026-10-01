#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <initializer_list>
namespace lmk61e2_core {
enum class Format : uint8_t { LVPECL=1, LVDS=2, HCSL=3 };
struct Plan { uint16_t outdiv{}, integer{}; uint32_t numerator{}, denominator{1}; double actual_hz{}, vco_hz{}, error_ppm{}; };
inline double max_frequency(Format f) { return f==Format::LVPECL ? 1e9 : f==Format::LVDS ? 9e8 : f==Format::HCSL ? 4e8 : 0; }
// Continued fraction with bounded denominator, including final semiconvergent.
inline void rational(double x, uint32_t &num, uint32_t &den) {
 constexpr uint64_t limit=4194303; uint64_t p0=0,q0=1,p1=1,q1=0;
 double remainder=x;
 for (int iteration=0; iteration<48; ++iteration) {
  uint64_t a=static_cast<uint64_t>(std::floor(remainder));
  if (q1 && a>(limit-q0)/q1) break;
  uint64_t p2=p0+a*p1,q2=q0+a*q1;
  p0=p1;q0=q1;p1=p2;q1=q2;
  double fraction=remainder-static_cast<double>(a);
  if (fraction<1e-12) { num=static_cast<uint32_t>(p1);den=static_cast<uint32_t>(q1);return; }
  remainder=1.0/fraction;
 }
 uint64_t k=(limit-q0)/q1;
 uint64_t pa=p0+k*p1,qa=q0+k*q1;
 if (std::abs(static_cast<double>(pa)/qa-x)<std::abs(static_cast<double>(p1)/q1-x)) {p1=pa;q1=qa;}
 num=static_cast<uint32_t>(p1);den=static_cast<uint32_t>(q1);
}
inline bool plan_frequency(double hz, Format format, Plan &best) {
 if (!std::isfinite(hz) || hz<1e7 || hz>max_frequency(format)) return false;
 bool found=false; double best_error=std::numeric_limits<double>::infinity();
 for (uint16_t divider=5;divider<=511;++divider) {
  double desired=hz*divider;
  if (desired<4.6e9 || desired>5.6e9) continue;
  double ratio=desired/1e8;uint16_t integer=static_cast<uint16_t>(std::floor(ratio));
  uint32_t num,den;rational(ratio-integer,num,den);
  if (num==den) {++integer;num=0;den=1;}
  double vco=1e8*(integer+static_cast<double>(num)/den);
  if (vco<4.6e9 || vco>5.6e9) continue;
  double actual=vco/divider,error=std::abs(actual-hz);
  bool better=!found || error<best_error-1e-6;
  if (found && std::abs(error-best_error)<=1e-6) {
   if ((num==0)!=(best.numerator==0)) better=num==0;
   else if (std::abs(vco-5e9)!=std::abs(best.vco_hz-5e9)) better=std::abs(vco-5e9)<std::abs(best.vco_hz-5e9);
   else better=den<best.denominator;
  }
  if (better) {best={divider,integer,num,den,actual,vco,(actual/hz-1)*1e6};best_error=error;found=true;}
 }
 return found;
}
struct Patch { uint8_t address,mask,value; };
// Profiles match hardware-tested functional settings, not a TI jitter optimizer.
inline void patches(const Plan &p, Patch (&r)[17]) {
 bool frac=p.numerator!=0;
 r[0]={22,1,static_cast<uint8_t>(p.outdiv>>8)};r[1]={23,255,static_cast<uint8_t>(p.outdiv)};
 r[2]={25,15,static_cast<uint8_t>(p.integer>>8)};r[3]={26,255,static_cast<uint8_t>(p.integer)};
 r[4]={27,63,static_cast<uint8_t>(p.numerator>>16)};r[5]={28,255,static_cast<uint8_t>(p.numerator>>8)};r[6]={29,255,static_cast<uint8_t>(p.numerator)};
 r[7]={30,63,static_cast<uint8_t>(p.denominator>>16)};r[8]={31,255,static_cast<uint8_t>(p.denominator>>8)};r[9]={32,255,static_cast<uint8_t>(p.denominator)};
 r[10]={33,15,static_cast<uint8_t>(frac?3:12)};r[11]={34,47,static_cast<uint8_t>(frac?0x24:0x28)};
 // R35[1:0] preserved. Integer profile matches TI EVM export; fractional C3 enabled.
 r[12]={35,0x74,static_cast<uint8_t>(frac?0x24:0)};
 r[13]={36,255,4};r[14]={37,7,0};r[15]={38,127,0};r[16]={39,7,static_cast<uint8_t>(frac?1:0)};
}
class Transport { public: virtual ~Transport()=default; virtual bool read(uint8_t address,uint8_t &value)=0; virtual bool write(uint8_t address,uint8_t value)=0; };
class Device {
 public:
 explicit Device(Transport &io):io_(io){}
 bool identify() {uint8_t a,b,c;return io_.read(0,a)&&io_.read(1,b)&&io_.read(2,c)&&a==16&&b==11&&c==51;}
 bool status(uint8_t &value) {return io_.read(66,value);}
 bool read_configuration(Plan &p,Format &format,bool &enabled) {
  uint8_t r[35]{};
  for(uint8_t address: {uint8_t(21),uint8_t(22),uint8_t(23),uint8_t(25),uint8_t(26),uint8_t(27),uint8_t(28),uint8_t(29),uint8_t(30),uint8_t(31),uint8_t(32),uint8_t(33),uint8_t(34)}) if(!io_.read(address,r[address]))return false;
  format=static_cast<Format>((r[21]&3)?(r[21]&3):1);enabled=!(r[21]&128)&&(r[21]&3);
  p.outdiv=((r[22]&1)<<8)|r[23];p.integer=((r[25]&15)<<8)|r[26];
  p.numerator=((r[27]&63)<<16)|(r[28]<<8)|r[29];p.denominator=((r[30]&63)<<16)|(r[31]<<8)|r[32];
  if(!std::isfinite(p.actual_hz)||p.integer<1||p.integer>4095||p.outdiv<5||!p.denominator||((r[33]&3)!=0&&(r[33]&3)!=3)) return false;
  p.vco_hz=5e7*((r[34]&32)?2:1)*(p.integer+((r[33]&3)?static_cast<double>(p.numerator)/p.denominator:0));
  p.actual_hz=p.vco_hz/p.outdiv;return true;
 }
 bool apply(const Plan &p,Format format,bool enabled) {
  if(!std::isfinite(p.actual_hz)||p.integer<1||p.integer>4095||p.outdiv<5||p.outdiv>511||!p.denominator||p.denominator>4194303||p.numerator>=p.denominator||p.actual_hz<1e7||p.actual_hz>max_frequency(format))return false;
  Patch patch[17];patches(p,patch);uint8_t before[17],values[17],control;
  if(!io_.read(21,control))return false;
  for(int i=0;i<17;++i) {if(!io_.read(patch[i].address,before[i]))return false;values[i]=(before[i]&~patch[i].mask)|(patch[i].value&patch[i].mask);}
  rollback_ok_=true;
  // Mute while programming. There is no glitchless retune guarantee.
  bool ok=io_.write(21,control|128);
  for(int i=0;i<17&&ok;++i)ok=io_.write(patch[i].address,values[i]);
  for(int i=0;i<17&&ok;++i){uint8_t readback;ok=io_.read(patch[i].address,readback)&&readback==values[i];}
  if(ok)ok=io_.write(72,2);
  uint8_t new_control=(control&0x7c)|static_cast<uint8_t>(format)|(enabled?0:128);
  if(ok)ok=io_.write(21,new_control);
  if(ok){uint8_t check;ok=io_.read(21,check)&&check==new_control;}
  if(!ok) {
   rollback_ok_=io_.write(21,control|128);
   for(int i=0;i<17;++i)if(!io_.write(patch[i].address,before[i]))rollback_ok_=false;
   if(!io_.write(72,2))rollback_ok_=false;
   if(!io_.write(21,control))rollback_ok_=false;
   for(int i=0;i<17;++i){uint8_t check;if(!io_.read(patch[i].address,check)||check!=before[i])rollback_ok_=false;}
   uint8_t check;if(!io_.read(21,check)||check!=control)rollback_ok_=false;
  }
  return ok;
 }
 bool set_output(Format format,bool enabled) {
  if(static_cast<uint8_t>(format)<1||static_cast<uint8_t>(format)>3)return false;
  uint8_t r;if(!io_.read(21,r))return false;
  uint8_t v=(r&0x7c)|static_cast<uint8_t>(format)|(enabled?0:128),check;
  if(io_.write(21,v)&&io_.read(21,check)&&v==check)return true;
  rollback_ok_=io_.write(21,r)&&io_.read(21,check)&&check==r;return false;
 }
 bool rollback_ok()const{return rollback_ok_;}
 private:Transport &io_;bool rollback_ok_{true};
};
}
