#pragma once
#include <cstdint>
namespace esphome {
namespace setup_priority { constexpr float IO=900, DATA=600; }
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  virtual void on_shutdown() {}
  virtual float get_setup_priority() const { return setup_priority::DATA; }
  void mark_failed() { failed_=true; }
  bool is_failed() const { return failed_; }
  void status_set_warning() { warning_=true; }
  void status_clear_warning() { warning_=false; }
  bool warning() const { return warning_; }
 private: bool failed_{false}, warning_{false};
};
class PollingComponent : public Component { public: virtual void update() {} };
}
