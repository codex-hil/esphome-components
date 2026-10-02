#pragma once
namespace esphome::binary_sensor {
class BinarySensor { public: bool state=false, has_state=false; void publish_state(bool v) { state=v;has_state=true; } };
}
