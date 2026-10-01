#include "lmk61e2.h"
#include "esphome/core/log.h"
namespace esphome::lmk61e2 {
static const char *const TAG="lmk61e2";
static const char*format_name(lmk61e2_core::Format f){return f==lmk61e2_core::Format::LVPECL?"LVPECL":f==lmk61e2_core::Format::LVDS?"LVDS":"HCSL";}
void LMK61E2Component::setup(){
 if(!device_.identify()||!device_.read_configuration(current_,format_,enabled_)){ESP_LOGE(TAG,"LMK61E2 identification/configuration read failed");mark_failed();return;}
 ready_=true;format_=initial_format_;
 // Apply desired frequency, format and output state as one transaction.
 enabled_=initial_enabled_;
 if(!set_frequency(initial_hz_)){ESP_LOGE(TAG,"Initial configuration failed");mark_failed();}
}
void LMK61E2Component::dump_config(){
 ESP_LOGCONFIG(TAG,"LMK61E2: %.6f MHz, %s, output %s",current_.actual_hz/1e6,format_name(format_),enabled_?"on":"off");
 ESP_LOGCONFIG(TAG,"Fractional profile: %s (experimental, not jitter optimized)",allow_fractional_?"allowed":"disabled");
 LOG_I2C_DEVICE(this);LOG_UPDATE_INTERVAL(this);
}
bool LMK61E2Component::set_frequency(double hz){
 if(!ready_||settling_||is_failed())return false;
 lmk61e2_core::Plan plan;
 if(!lmk61e2_core::plan_frequency(hz,format_,plan)){ESP_LOGW(TAG,"Frequency outside %s range",format_name(format_));return false;}
 if(plan.numerator&&!allow_fractional_){ESP_LOGW(TAG,"Frequency requires experimental fractional profile; enable allow_fractional");return false;}
 if(!device_.apply(plan,format_,enabled_)){
  ESP_LOGE(TAG,"Configuration transaction failed; rollback %s",device_.rollback_ok()?"verified":"FAILED");status_set_warning();
  if(!device_.rollback_ok()){ready_=false;mark_failed();}
  return false;
 }
 current_=plan;settling_=true;
 ESP_LOGI(TAG,"Planned %.6f MHz -> %.6f MHz, OUTDIV %u, N %u + %u/%u",hz/1e6,plan.actual_hz/1e6,plan.outdiv,plan.integer,plan.numerator,plan.denominator);
 set_timeout("calibration",10,[this](){check_calibration_(0);});return true;
}
void LMK61E2Component::check_calibration_(uint8_t attempt){
 uint8_t status;
 if(!device_.status(status)){status_set_warning();settling_=false;return;}
 if((status&3)&&attempt<20){set_timeout("calibration",10,[this,attempt](){check_calibration_(attempt+1);});return;}
 settling_=false;
 if(status&3){ESP_LOGW(TAG,"PLL status after calibration: 0x%02X",status);status_set_warning();if(lock_sensor_)lock_sensor_->publish_state(false);return;}
 status_clear_warning();publish_settings();if(lock_sensor_)lock_sensor_->publish_state(true);
}
bool LMK61E2Component::set_format(uint8_t format){
 auto f=static_cast<lmk61e2_core::Format>(format);
 if(!ready_||settling_||is_failed()||current_.actual_hz>lmk61e2_core::max_frequency(f)||format<1||format>3)return false;
 if(!device_.set_output(f,enabled_)){status_set_warning();return false;}
 format_=f;publish_settings();return true;
}
bool LMK61E2Component::set_enabled(bool enabled){
 if(!ready_||settling_||is_failed())return false;
 if(!device_.set_output(format_,enabled)){status_set_warning();return false;}
 enabled_=enabled;publish_settings();return true;
}
void LMK61E2Component::publish_settings(){
 if(frequency_number_)frequency_number_->publish_state(current_.actual_hz/1e6);
 if(frequency_sensor_)frequency_sensor_->publish_state(current_.actual_hz/1e6);
 if(format_select_)format_select_->publish_state(format_name(format_));
 if(output_switch_)output_switch_->publish_state(enabled_);
}
void LMK61E2Component::update(){
 if(!ready_||settling_||is_failed())return;
 uint8_t status;
 if(!device_.status(status)){status_set_warning();return;}
 if(lock_sensor_)lock_sensor_->publish_state(!(status&3));
 if(status&3)status_set_warning();else status_clear_warning();
 // This status is sampled; absence of loss-of-lock is not jitter qualification.
}
void FrequencyNumber::control(float value){if(!parent_->set_frequency(static_cast<double>(value)*1e6))parent_->publish_settings();}
void FormatSelect::control(const std::string &value){uint8_t f=value=="LVPECL"?1:value=="LVDS"?2:value=="HCSL"?3:0;if(!parent_->set_format(f))parent_->publish_settings();}
void OutputSwitch::write_state(bool value){if(!parent_->set_enabled(value))parent_->publish_settings();}
}
