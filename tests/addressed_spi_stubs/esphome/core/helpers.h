#pragma once
namespace esphome {
class HighFrequencyLoopRequester {
 public:
  void start() { if (!started_) { started_=true; ++requests_; } }
  void stop() { if (started_) { started_=false; --requests_; } }
  static bool is_high_frequency() { return requests_ != 0; }
 protected:
  bool started_{false};
  inline static unsigned requests_{0};
};
}
