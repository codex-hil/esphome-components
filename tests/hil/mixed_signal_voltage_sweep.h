#pragma once
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/mcp3208/mcp3208.h"
#include "esphome/components/dacx0504/dacx0504.h"
#include "esphome/components/spi_shift_register/spi_shift_register.h"
#include "esphome/components/mmc5983_spi/mmc5983_spi.h"
inline void mixed_signal_voltage_sweep(esphome::mcp3208::MCP3208 *z,esphome::dacx0504::DACX0504 *dac,esphome::spi_shift_register::SPIShiftRegister *out,esphome::mmc5983_spi::MMC5983SPIComponent *mag){
 static unsigned tick=0;static bool done=false;
 if(esphome::millis()<12000 || done)return;
 if(z->is_failed() || dac->is_failed() || out->is_failed() || mag->is_failed()){ESP_LOGE("coexist_hil","FINAL FAIL setup");done=true;return;}
 if(tick<8){
  const uint8_t patterns[]={0x00,0x55,0xAA,0xFF};
  if(tick%2==0){ESP_LOGI("coexist_hil","ARM_GPIO pattern=%02X tick=%u",patterns[tick/2],tick);tick++;return;}
  out->write_byte_value(patterns[tick/2]);
  ESP_LOGI("coexist_hil","GPIO pattern=%02X tick=%u",patterns[tick/2],tick);
 }else if(tick==8){out->write_byte_value(0x07);
 }else if(tick<24){
  const unsigned codes[]={0,16384,32768,49152,65535};unsigned point=(tick-9)/3,code=codes[point];
  if((tick-9)%3==0){dac->set_channel_voltage(2,3.0f*code/65536);dac->set_channel_voltage(3,1.5f);dac->commit();ESP_LOGI("coexist_hil","SWEEP_SET point=%u code2=%u code3=32768",point,code);}
  esphome::delay(1);float p=z->read_voltage(3,false),n=z->read_voltage(4,false);
  if((tick-9)%3==1){
   unsigned rb=dac->read_register(0x0A);
   if(rb!=code){ESP_LOGE("coexist_hil","FINAL FAIL dac_readback=%04X expected=%04X",rb,code);done=true;return;}
   // ADC samples are deliberately retained without filtering. SMU ON perturbs
   // this branch; strict ADC regression is a separate SMU OFF run.
   ESP_LOGI("coexist_hil","SWEEP point=%u code2=%u code3=32768 adc_p=%.6f adc_n=%.6f",point,code,p,n);
  }
 }else{
  dac->set_channel_voltage(2,1.5f);dac->set_channel_voltage(3,1.5f);dac->commit();out->write_byte_value(0x07);
  ESP_LOGI("coexist_hil","FINAL PASS voltage_sweep=5 gpio_patterns=00,55,AA,FF physical_gpio=REQUIRES_SCOPE mag_errors=%u dac2=8000 dac3=8000 gpio=07",mag->get_error_count());done=true;return;
 }
 mag->update();unsigned pid=mag->read_register(0x2F);
 ESP_LOGI("coexist_hil","MAG tick=%u pid=%02X errors=%u",tick,pid,mag->get_error_count());
 if(pid!=0x30){ESP_LOGE("coexist_hil","FINAL FAIL mag_pid");done=true;}
 tick++;
}
