#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace esphome {
namespace setup_priority { constexpr float DATA=600; }
class GPIOPin { public: virtual ~GPIOPin()=default; virtual void setup() {} virtual void digital_write(bool)=0; };
class Component { public:
 virtual ~Component()=default; virtual void setup() {} virtual void dump_config() {} virtual void on_shutdown() {}
 virtual float get_setup_priority() const { return 0; }
 std::function<void()> pending; uint32_t timeout_ms=0;
 void cancel_timeout(const char *) { pending=nullptr; }
 void set_timeout(const char *, uint32_t ms, std::function<void()> fn) { timeout_ms=ms; pending=fn; }
 void finish() { auto fn=pending; pending=nullptr; if(fn) fn(); }
};
}
