#pragma once
namespace esphome::output { class FloatOutput { public: virtual ~FloatOutput()=default; protected: virtual void write_state(float)=0; }; }
