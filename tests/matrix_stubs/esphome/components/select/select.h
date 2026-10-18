#pragma once
#include <string>
namespace esphome::select { class Select { public:
 virtual ~Select()=default; std::string state;
 void publish_state(const std::string &s) { state=s; }
 void command(const std::string &s) { control(s); }
 protected: virtual void control(const std::string &)=0;
}; }
