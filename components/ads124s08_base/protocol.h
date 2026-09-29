#pragma once
#include <cstdint>
#include <cstddef>
namespace esphome::ads124s08_base {
// ADS124S08 polynomial x^8+x^2+x+1; initial remainder 0, MSB first,
// no reflection or final XOR. With SENDSTAT=1 cover STATUS then all 3 data bytes.
inline uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc=0;
  for(size_t i=0;i<length;i++) {
    crc^=data[i];
    for(unsigned bit=0;bit<8;bit++) crc=(crc&0x80)?uint8_t((crc<<1)^0x07):uint8_t(crc<<1);
  }
  return crc;
}
inline bool valid_frame(const uint8_t *frame, size_t length) {
  return length==5 && crc8(frame,4)==frame[4];
}
inline int32_t decode24(const uint8_t *b) {
  int32_t n=(uint32_t(b[0])<<16)|(uint32_t(b[1])<<8)|b[2];
  return (n&0x800000)?n-0x1000000:n;
}
inline float input_volts(int32_t raw, unsigned gain=1, float reference=2.5f) { return raw*(reference/8388608.0f)/gain; }
inline float die_celsius(int32_t raw, unsigned gain=1) { return 25.0f+(input_volts(raw,gain)-0.129f)/0.000403f; }
// Only rates covered by the current hardware matrix are exposed.
inline uint8_t rate_code(float sps) {
  return sps==2.5f?0:sps==20?4:sps==100?7:sps==1000?11:13;
}
inline uint8_t pga_register(unsigned gain, bool enabled=true) {
  uint8_t code=0;
  for(; gain>1; gain>>=1) code++;
  return (enabled?0x08:0) | code;
}
inline uint8_t rate_register(float sps, bool sinc3) { return 0x20 | (sinc3?0:0x10) | rate_code(sps); }
inline uint32_t conversion_wait_ms(float sps, bool sinc3) {
  // TI tables 13/15, first conversion, DELAY=14 tMOD. Conservative bound
  // includes 1.5% oscillator tolerance, low-latency overhead and scheduler margin.
  return uint32_t((sinc3?3100.0f:1100.0f)/sps)+12;
}
inline uint8_t system_register(unsigned source) {
  // Scan order: short, analog supply, digital supply, die temperature.
  constexpr uint8_t regs[]={0x31,0x71,0x91,0x51};
  return regs[source%4] | 0x02; // status + CRC enabled; no burnout current
}
}
