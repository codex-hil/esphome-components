#pragma once
#include <string>
#include <vector>
namespace esphome::text_sensor {
class TextSensor {
 public:
  std::vector<std::string> states;
  void publish_state(const std::string &value) { states.push_back(value); }
};
}
