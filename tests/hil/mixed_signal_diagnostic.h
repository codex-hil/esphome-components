#pragma once
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/mcp3208/mcp3208.h"
#include "esphome/components/dacx0504/dacx0504.h"
#include "esphome/components/mmc5983_spi/mmc5983_spi.h"
inline void mixed_signal_diagnostic(esphome::mcp3208::MCP3208 *x, esphome::mcp3208::MCP3208 *y, esphome::mcp3208::MCP3208 *z, esphome::dacx0504::DACX0504 *dac, esphome::mmc5983_spi::MMC5983SPIComponent *mag) {
 static unsigned step=0; static bool done=false;
 if(esphome::millis()<12000 || done)return;
 unsigned phase=step/100, sample=step%100;
 const char *labels[]={"ADC_ONLY_SMU_OFF","ADC_ONLY_SMU_ON","ADC_ONLY_SMU_MEASURE","MAG_PID_SMU_OFF","MAG_CONVERT_SMU_OFF","MAG_CONVERT_DELAY5MS","ALL_ADC_MAG_PID"};
 if(sample==0){
   if(phase==0){dac->set_channel_voltage(2,1.5f);dac->set_channel_voltage(3,1.5f);dac->commit();}
   if(phase==7){done=true; ESP_LOGI("coexist_diag","FINAL DIAG_DONE samples=700 mag_errors=%u",mag->get_error_count());return;}
   ESP_LOGI("coexist_diag","PHASE phase=%u label=%s",phase,labels[phase]);return step++,(void)0;
 }
 unsigned pid=0x30;
 if(phase==3 || phase==6)pid=mag->read_register(0x2F);
 if(phase==4 || phase==5){mag->update();pid=mag->read_register(0x2F);}
 if(phase==5)esphome::delay(5);
 if(phase==6){
   for(auto *adc:{x,y,z})for(unsigned ch=0;ch<8;ch++){float value=adc->read_voltage(ch,false);(void)value;pid=mag->read_register(0x2F);}
 }
 float p1=z->read_voltage(3,false),n1=z->read_voltage(4,false),p2=z->read_voltage(3,false),n2=z->read_voltage(4,false);
 ESP_LOGI("coexist_diag","ROW phase=%u sample=%u p1=%u n1=%u p2=%u n2=%u pid=%02X mag_errors=%u",phase,sample,(unsigned)std::lround(p1*4096/3),(unsigned)std::lround(n1*4096/3),(unsigned)std::lround(p2*4096/3),(unsigned)std::lround(n2*4096/3),pid,mag->get_error_count());
 step++;
}
