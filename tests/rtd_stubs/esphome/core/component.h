#pragma once
namespace esphome {
class Component {
 public:
  virtual ~Component()=default;
  virtual void setup() {}
  virtual void dump_config() {}
  void status_set_warning(){warning=true;}
  void status_clear_warning(){warning=false;}
  bool warning{false};
};
}
