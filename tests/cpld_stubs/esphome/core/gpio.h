#pragma once
#include <cstddef>
#include <cstdint>
namespace esphome {
namespace gpio {
enum Flags : uint8_t { FLAG_NONE=0, FLAG_INPUT=1, FLAG_OUTPUT=2, FLAG_OPEN_DRAIN=4, FLAG_PULLUP=8 };
inline Flags operator|(Flags a, Flags b) { return Flags(uint8_t(a)|uint8_t(b)); }
}
class GPIOPin {
 public:
  virtual ~GPIOPin() = default;
  virtual void setup() {}
  virtual void pin_mode(gpio::Flags) {}
  virtual bool digital_read() { return false; }
  virtual void digital_write(bool) {}
  virtual gpio::Flags get_flags() const { return gpio::FLAG_INPUT; }
  virtual size_t dump_summary(char *, size_t) const { return 0; }
};
}
