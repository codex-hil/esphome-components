#pragma once
#include <vector>
#include <functional>
namespace esphome::sensor {
class Sensor {
 public:
  std::vector<float> states;
  std::function<void(float)> callback;
  void publish_state(float value) { states.push_back(value); if (callback) callback(value); }
};
}
