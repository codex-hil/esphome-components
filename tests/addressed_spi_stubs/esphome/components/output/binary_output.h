#pragma once
namespace esphome::output { class BinaryOutput { public: virtual ~BinaryOutput()=default; protected: virtual void write_state(bool)=0; }; }
