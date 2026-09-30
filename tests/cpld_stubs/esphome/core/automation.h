#pragma once
#include <functional>
namespace esphome {
template<typename... Args> class Trigger {
 public:
  std::function<void(Args...)> callback;
  void trigger(Args... args) { if (callback) callback(args...); }
};
}
