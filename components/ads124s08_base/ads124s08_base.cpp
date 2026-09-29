#include "ads124s08_base.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>
namespace esphome::ads124s08_base {
static const char *const TAG="ads124s08_base";
static const char *const SOURCES[]={"internal_short","avdd_div4","dvdd_div4","die_temperature"};
void ADS124S08Base::command_(uint8_t c) { enable(); write_byte(c); disable(); }
void ADS124S08Base::write_(uint8_t r,uint8_t v) {
  enable(); write_byte(0x40|r); write_byte(0); write_byte(v); disable();
}
uint8_t ADS124S08Base::read_(uint8_t r) {
  uint8_t v=0; enable(); write_byte(0x20|r); write_byte(0); read_array(&v,1); disable(); return v;
}
void ADS124S08Base::setup() {
  if(crc_errors_sensor_) crc_errors_sensor_->publish_state(0);
  spi_setup(); command_(0x06); since_=millis(); status_->publish_state("initializing");
}
void ADS124S08Base::stop_excitation_() {
  command_(0x0A); write_(6,0); write_(7,0xFF);
}
void ADS124S08Base::fail_(const char *why) {
  stop_excitation_();
  for(auto *s:channels_) s->publish_state(NAN);
  active_=nullptr;
  for(auto *s:{raw_,short_,avdd_,dvdd_,temp_}) if(s) s->publish_state(NAN);
  status_->publish_state(why); status_set_warning();
  ESP_LOGW(TAG,"%s: %s; retry in 5s",raw_->get_name().c_str(),why);
  phase_=Phase::RETRY; since_=millis();
}
bool ADS124S08Base::verify_() {
  for(unsigned i=0;i<8;i++) if(read_(i+2)!=expected_[i]) return false;
  return true;
}
bool ADS124S08Base::arm_ready_() {
  // TI 9.5.5: RREG with LSB=1 leaves DOUT high after readback. Shared GPIO
  // is already configured by SPI; do not reset/reconfigure the pin mux here.
  enable(); write_byte(0x29); write_byte(0); uint8_t v=0; read_array(&v,1);
  delayMicroseconds(2);
  bool high=ready_pin_->digital_read(); disable();
  high_polls_=0; armed_us_=micros(); since_=millis();
  return v==expected_[7] && high; // SYS SENDSTAT=1 guarantees the final bit is high.
}
void ADS124S08Base::configure_() {
  // Break before make: STOP alone does not switch off the IDACs.
  stop_excitation_();
  expected_[0]=active_?active_->mux:0x01;
  expected_[1]=active_?pga_register(active_->gain,active_->pga):pga_register(gain_);
  expected_[2]=active_?rate_register(active_->rate,active_->sinc3):datarate_();
  expected_[3]=active_?active_->reference:0x3A;
  expected_[4]=active_?active_->current:0;
  expected_[5]=active_?active_->idac_mux:0xFF;
  expected_[6]=0; // VBIAS disconnected
  expected_[7]=active_?0x03:system_register(source_index_); // external mux, STATUS + CRC
  // Configure everything while current is off; enable magnitude last.
  for(unsigned i=0;i<8;i++) if(i!=4) write_(i+2,expected_[i]);
  write_(6,expected_[4]);
}
void ADS124S08Base::start_next_() {
  if(phase_!=Phase::IDLE) return;
  ADS124S08Channel *requested=nullptr;
  size_t index=0;
  for(size_t n=0;n<channels_.size();n++) {
    index=(next_channel_+n)%channels_.size();
    if(channels_[index]->pending) { requested=channels_[index]; break; }
  }
  if(!requested && !diagnostic_pending_) return;
  if(requested && (prefer_external_ || !diagnostic_pending_)) {
    active_=requested; active_->pending=false;
    next_channel_=(index+1)%channels_.size(); prefer_external_=false;
  } else {
    active_=nullptr; diagnostic_pending_=false; prefer_external_=true;
  }
  configure_();
  if(!verify_()) {fail_("config_mismatch");return;}
  burst_count_=0;
  phase_=Phase::SETTLE; since_=millis();
}
void ADS124S08Base::update() {
  diagnostic_pending_=true;
  start_next_();
}
void ADS124S08Base::loop() {
  auto elapsed=uint32_t(millis()-since_);
  if(phase_==Phase::RETRY && elapsed>=5000) {
    command_(0x06); phase_=Phase::RESET_WAIT; since_=millis(); return;
  }
  if(phase_==Phase::RESET_WAIT && elapsed>=10) {
    if((read_(0)&7)!=0 || read_(7)!=0xFF || read_(4)!=0x14) {fail_("identity_or_reset_error");return;}
    command_(0x0A); write_(1,0);
    active_=nullptr; configure_();
    if(!verify_()) {fail_("config_mismatch");return;}
    phase_=Phase::IDLE; update(); return;
  }
  if(phase_==Phase::IDLE) { start_next_(); return; }
  if(phase_==Phase::SETTLE && elapsed>=(active_?active_->settling:100)) {
    command_(0x08); phase_=Phase::CONVERT; since_=millis();
    if(active_continuous_() && !arm_ready_()) fail_("ready_arm_failed");
    return;
  }
  if(phase_!=Phase::CONVERT) return;
  const uint32_t limit=conversion_wait_ms(active_rate_(),active_sinc3_())+1000;
  if(elapsed>limit) {fail_(active_continuous_()?"ready_timeout":"scheduler_timeout");return;}
  if(!active_continuous_() && elapsed<conversion_wait_ms(active_rate_(),active_sinc3_())) return;
  uint8_t b[5]={};
  uint32_t ready_us=0;
  enable();
  if(active_continuous_()) {
    delayMicroseconds(2);
    if(ready_pin_->digital_read()) {high_polls_++; disable(); return;}
    ready_us=micros()-armed_us_;
  }
  write_byte(0x12); read_array(b,5); disable();
  if(!active_continuous_()) command_(0x0A);
#ifdef ADS124S08_CRC_FAULT_TEST
  if(inject_crc_fault_) {
    ESP_LOGI(TAG,"%s CRC_TEST original_valid=%u; flip one received data bit in RAM",raw_->get_name().c_str(),unsigned(valid_frame(b,5)));
    b[1]^=0x01; inject_crc_fault_=false;
  }
#endif
  if(!valid_frame(b,5)) {
    crc_errors_++;
    if(crc_errors_sensor_) crc_errors_sensor_->publish_state(crc_errors_);
    ESP_LOGW(TAG,"%s CRC mismatch received=%02X computed=%02X count=%u",raw_->get_name().c_str(),b[4],crc8(b,4),unsigned(crc_errors_));
    fail_("crc_error"); return;
  }
  ESP_LOGD(TAG,"%s FRAME %02X %02X %02X %02X %02X crc=ok",raw_->get_name().c_str(),b[0],b[1],b[2],b[3],b[4]);
  if(b[0]!=0) {fail_("adc_status_fault");return;}
  if(!verify_()) {fail_("config_mismatch");return;}
  int32_t raw=decode24(b+1);
  if(raw==8388607 || raw==-8388608) {fail_("saturated");return;}
  if(active_) {
    const float volts=input_volts(raw,active_->gain,active_->reference_voltage);
    // Turn off excitation before publishing callbacks can schedule further work.
    stop_excitation_();
    if(read_(6)!=0 || read_(7)!=0xFF) { fail_("idac_shutdown_error"); return; }
    active_->publish_state(volts);
    source_->publish_state(active_->get_name()); raw_->publish_state(raw);
    status_->publish_state("ok"); status_clear_warning();
    ESP_LOGI(TAG,"%s CHANNEL %s mux=%02X pga=%02X ref=%02X idac=%02X/%02X raw=%ld volts=%.7f crc=ok",
             raw_->get_name().c_str(),active_->get_name().c_str(),expected_[0],expected_[1],expected_[3],
             expected_[4],expected_[5],long(raw),volts);
    sample_count_++; active_=nullptr; phase_=Phase::IDLE; return;
  }
  source_->publish_state(SOURCES[source_index_]); raw_->publish_state(raw);
  switch(source_index_) {
    case 0: if(short_) short_->publish_state(raw); break;
    case 1: if(avdd_) avdd_->publish_state(input_volts(raw)*4); break;
    case 2: if(dvdd_) dvdd_->publish_state(input_volts(raw)*4); break;
    case 3: if(temp_) temp_->publish_state(die_celsius(raw,gain_)); break;
  }
  status_->publish_state("ok"); status_clear_warning();
  ESP_LOGI(TAG,"%s source=%s raw=%ld status=0x%02X gain=%u sps=%.1f filter=%s",raw_->get_name().c_str(),SOURCES[source_index_],long(raw),b[0],gain_,sample_rate_,sinc3_?"sinc3":"low_latency");
  sample_count_++;
  if(active_continuous_()) {
    ESP_LOGI(TAG,"%s READY source=%s batch_index=%u wait_us=%u high_polls=%u",
             raw_->get_name().c_str(),SOURCES[source_index_],burst_count_,unsigned(ready_us),high_polls_);
    burst_count_++;
    if(burst_count_<samples_per_source_) {
      if(!arm_ready_()) fail_("ready_arm_failed");
      return;
    }
    command_(0x0A);
  }
  source_index_=(source_index_+1)%4; phase_=Phase::IDLE;
}
void ADS124S08Base::dump_config() {
  ESP_LOGCONFIG(TAG,"ADS124S08: mode1/100kHz, gain%u, %.1fSPS, %s",gain_,sample_rate_,sinc3_?"sinc3":"low_latency");
  ESP_LOGCONFIG(TAG,"Diagnostics: %s; internal reference nominal 2.5V; IDAC/bias off",
                continuous_?"continuous, DOUT/DRDY polling":"single-shot, timed read");
  for(auto *s:channels_) {
    ESP_LOGCONFIG(TAG,"Channel %s: mux=%02X gain=%u PGA=%s reference=%.4fV IDAC=%02X/%02X settle=%ums; single-shot",
      s->get_name().c_str(),s->mux,s->gain,s->pga?"on":"bypass",s->reference_voltage,s->current,s->idac_mux,unsigned(s->settling));
  }
  LOG_SENSOR("  ","Raw",raw_);
  LOG_UPDATE_INTERVAL(this);
}
}
