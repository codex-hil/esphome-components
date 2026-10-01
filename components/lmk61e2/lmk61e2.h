#pragma once
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/automation.h"
#include "planner.h"
namespace esphome::lmk61e2 {
class LMK61E2Component;
class FrequencyNumber:public number::Number {public:void set_parent(LMK61E2Component*p){parent_=p;}protected:void control(float value) override;LMK61E2Component*parent_{};};
class FormatSelect:public select::Select {public:void set_parent(LMK61E2Component*p){parent_=p;}protected:void control(const std::string &value) override;LMK61E2Component*parent_{};};
class OutputSwitch:public switch_::Switch {public:void set_parent(LMK61E2Component*p){parent_=p;}protected:void write_state(bool value) override;LMK61E2Component*parent_{};};
class LMK61E2Component:public PollingComponent,public i2c::I2CDevice,public lmk61e2_core::Transport {
 public:
 LMK61E2Component():device_(*this){}
 void setup()override;void update()override;void dump_config()override;
 float get_setup_priority()const override{return setup_priority::DATA;}
 bool read(uint8_t address,uint8_t &value)override{return this->read_byte(address,&value);}
 bool write(uint8_t address,uint8_t value)override{return this->write_byte(address,value);}
 void set_initial_frequency(double hz){initial_hz_=hz;}
 void set_initial_format(uint8_t format){initial_format_=static_cast<lmk61e2_core::Format>(format);}
 void set_initial_enabled(bool enabled){initial_enabled_=enabled;}
 void set_allow_fractional(bool allow){allow_fractional_=allow;}
 void set_frequency_number(FrequencyNumber*p){frequency_number_=p;}
 void set_format_select(FormatSelect*p){format_select_=p;}
 void set_output_switch(OutputSwitch*p){output_switch_=p;}
 void set_frequency_sensor(sensor::Sensor*p){frequency_sensor_=p;}
 void set_lock_sensor(binary_sensor::BinarySensor*p){lock_sensor_=p;}
 bool set_frequency(double hz);bool set_format(uint8_t format);bool set_enabled(bool enabled);
 void publish_settings();
 protected:
 void check_calibration_(uint8_t attempt);
 lmk61e2_core::Device device_;lmk61e2_core::Plan current_{};
 lmk61e2_core::Format format_{lmk61e2_core::Format::LVPECL},initial_format_{lmk61e2_core::Format::LVPECL};
 double initial_hz_{1e8};bool enabled_{false},initial_enabled_{true},allow_fractional_{false},ready_{false},settling_{false};
 FrequencyNumber*frequency_number_{};FormatSelect*format_select_{};OutputSwitch*output_switch_{};
 sensor::Sensor*frequency_sensor_{};binary_sensor::BinarySensor*lock_sensor_{};
};
template<typename... Ts>class SetFrequencyAction:public Action<Ts...>{public:explicit SetFrequencyAction(LMK61E2Component*p):parent_(p){}TEMPLATABLE_VALUE(double,frequency)void play(Ts...x)override{parent_->set_frequency(this->frequency_.value(x...));}protected:LMK61E2Component*parent_;};
}
