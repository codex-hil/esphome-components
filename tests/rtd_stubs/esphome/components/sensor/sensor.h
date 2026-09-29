#pragma once
#include <functional>
#include <vector>
#include <cmath>
namespace esphome::sensor {
class Sensor {
 public:
  float state=NAN;
  std::vector<float> publications;
  void publish_state(float value){
    state=value;present_=true;publications.push_back(value);
    for(auto &callback:callbacks_)callback(value);
  }
  bool has_state()const{return present_;}
  void add_on_state_callback(std::function<void(float)> callback){callbacks_.push_back(callback);}
 protected:
  bool present_{false};
  std::vector<std::function<void(float)>> callbacks_;
};
}
