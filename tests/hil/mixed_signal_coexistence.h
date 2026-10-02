#pragma once
#include <cmath>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/mcp3208/mcp3208.h"
#include "esphome/components/dacx0504/dacx0504.h"
#include "esphome/components/spi_shift_register/spi_shift_register.h"
#include "esphome/components/mmc5983_spi/mmc5983_spi.h"

// Runs real configured components. Counts only explicit diagnostic frames;
// setup, the independent sampling loop and background sensor polling are extra.
inline void mixed_signal_hil_step(esphome::mcp3208::MCP3208 *x,
                                 esphome::mcp3208::MCP3208 *y,
                                 esphome::mcp3208::MCP3208 *z,
                                 esphome::dacx0504::DACX0504 *dac,
                                 esphome::spi_shift_register::SPIShiftRegister *out,
                                 esphome::mmc5983_spi::MMC5983SPIComponent *mag,
                                 esphome::sensor::Sensor *mx,
                                 esphome::sensor::Sensor *my,
                                 esphome::sensor::Sensor *mz,
                                 esphome::sensor::Sensor *temp) {
  static unsigned tick = 0, frames = 0, errors = 0;
  static uint32_t startup_mag_errors = 0;
  static bool initialized = false, stopped = false;
  if (esphome::millis() < 12000 || stopped) return;
  auto restore = [&]() {
    dac->set_channel_voltage(2, 1.5f); dac->set_channel_voltage(3, 1.5f);
    dac->commit(); out->write_byte_value(0x07); frames += 4;
  };
  auto fail = [&](const char *why) {
    errors++; restore(); stopped = true;
    ESP_LOGE("coexist_hil", "FINAL FAIL reason=%s ticks=%u frames_min=%u errors=%u mag_errors=%u", why, tick, frames, errors, mag->get_error_count());
  };
  auto pid = [&]() {
    uint8_t value = mag->read_register(0x2F); frames++;
    if (value != 0x30) {
      ESP_LOGE("coexist_hil", "Unexpected magnetometer PID=%02X tick=%u", value, tick);
      return false;
    }
    return true;
  };
  auto voltage = [&](esphome::mcp3208::MCP3208 *adc, unsigned channel) {
    frames++; return adc->read_voltage(channel, false);
  };
  auto match = [&](float volts, unsigned code) {
    return std::isfinite(volts) && std::fabs(volts * 4096.0f / 3.0f - code / 16.0f) <= 6.0f;
  };
  if (!initialized) {
    if (dac->is_failed() || x->is_failed() || y->is_failed() || z->is_failed() || out->is_failed() || mag->is_failed()) { fail("setup_failed"); return; }
    startup_mag_errors = mag->get_error_count();
    ESP_LOGI("coexist_hil", "START module=0 head_chip=2 mag_chip=1 serial_optional=1 startup_mag_errors=%u", startup_mag_errors);
    unsigned id = dac->read_register(1), cfg = dac->read_register(3), gain = dac->read_register(4), sync = dac->read_register(2), status = dac->read_register(7);
    frames += 10;
    ESP_LOGI("coexist_hil", "DAC id=%04X cfg=%04X gain=%04X sync=%04X status=%04X", id, cfg, gain, sync, status);
    if (id != 0x0497 || cfg != 0x0500 || gain != 0x010F || sync != 0x0F0F || (status & 1)) { fail("dac_registers"); return; }
    initialized = true;
  }
  // Baseline: magnetometer only, followed by simultaneous head traffic.
  if (tick < 10) {
    mag->update();
    if (!pid() || mag->get_error_count() != startup_mag_errors || !std::isfinite(mx->state) || !std::isfinite(my->state) || !std::isfinite(mz->state) || !std::isfinite(temp->state)) { fail("mag_baseline"); return; }
    ESP_LOGI("coexist_hil", "BASE tick=%u xyz=%.6f,%.6f,%.6f temp=%.2f mag_errors=%u", tick, mx->state, my->state, mz->state, temp->state, mag->get_error_count());
  } else if (tick < 60) {
    const unsigned codes[] = {0, 16384, 32768, 49152, 65535};
    const unsigned point = (tick - 10) / 10, code = codes[point];
    if ((tick - 10) % 10 == 0) {
      dac->set_channel_voltage(2, 3.0f * code / 65536.0f);
      dac->set_channel_voltage(3, 3.0f * (65535-code) / 65536.0f);
      dac->commit(); frames += 3;
      ESP_LOGI("coexist_hil", "SWEEP_SET point=%u code2=%u code3=%u", point, code, 65535-code);
    }
    if (!pid()) { fail("mag_after_dac"); return; }
    esphome::delay(1);
    const float vp = voltage(z,3), vn = voltage(z,4);
    if (!match(vp, code) || !match(vn, 65535-code)) { ESP_LOGE("coexist_hil", "LOOP vp=%.6f vn=%.6f code=%u",vp,vn,code); fail("sweep_loopback"); return; }
    if ((tick-10)%10 == 2) ESP_LOGI("coexist_hil", "SWEEP point=%u code2=%u code3=%u adc_p=%.6f adc_n=%.6f",point,code,65535-code,vp,vn);
    mag->update();
  } else if (tick < 185) {
    const unsigned codes[] = {0,16384,32768,49152,65535};
    const unsigned iteration = tick-60, c2 = codes[iteration%5], c3 = codes[(iteration+2)%5];
    dac->set_channel_voltage(2,3.0f*c2/65536.0f); frames++;
    if (!pid()) { fail("mag_after_stage2"); return; }
    dac->set_channel_voltage(3,3.0f*c3/65536.0f); frames++;
    if (!pid()) { fail("mag_after_stage3"); return; }
    dac->commit(); frames++;
    esphome::delay(1);
    for (unsigned chip=0; chip<3; chip++) {
      auto *adc = chip==0 ? x : chip==1 ? y : z;
      float data[8];
      for (unsigned channel=0; channel<8; channel++) {
        data[channel]=voltage(adc,channel);
        if (!std::isfinite(data[channel]) || data[channel]<0 || data[channel]>=3.0f) { fail("adc_range"); return; }
        // Return to the parallel-only path between EVERY ADC frame.
        if (!pid()) { fail("mag_between_adc"); return; }
      }
      if (chip==2 && (!match(data[3],c2) || !match(data[4],c3))) { ESP_LOGE("coexist_hil","LOOP p=%.6f n=%.6f code2=%u code3=%u",data[3],data[4],c2,c3); fail("stress_loopback"); return; }
      if (iteration%25==0) ESP_LOGI("coexist_hil", "ADC cycle=%u chip=%u values=%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",iteration,chip,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]);
    }
    const uint16_t r2=dac->read_register(0x0A), r3=dac->read_register(0x0B); frames+=4;
    if (r2!=c2 || r3!=c3) { fail("dac_readback"); return; }
    out->write_byte_value(iteration%2 ? 0x07 : 0x05); frames++;
    if (!pid()) { fail("mag_after_gpio"); return; }
    mag->update();
    if (iteration%10==0) ESP_LOGI("coexist_hil", "STRESS cycle=%u frames_min=%u xyz=%.6f,%.6f,%.6f temp=%.2f mag_errors=%u",iteration,frames,mx->state,my->state,mz->state,temp->state,mag->get_error_count());
  } else if (tick == 185) {
    restore(); esphome::delay(1);
    float before_p=voltage(z,3), before_n=voltage(z,4);
    dac->set_channel_voltage(2,0.75f); dac->set_channel_voltage(3,2.25f); frames+=2;
    if (!pid()) { fail("mag_sync_hold"); return; }
    float held_p=voltage(z,3), held_n=voltage(z,4);
    dac->commit(); frames++; esphome::delay(1);
    float after_p=voltage(z,3), after_n=voltage(z,4);
    ESP_LOGI("coexist_hil", "SYNC before=%.6f,%.6f held=%.6f,%.6f after=%.6f,%.6f",before_p,before_n,held_p,held_n,after_p,after_n);
    if (!match(held_p,32768) || !match(held_n,32768) || !match(after_p,16384) || !match(after_n,49152)) { fail("sync_hold_commit"); return; }
    restore();
    z->set_sample_rate(100.0f);
    ESP_LOGI("coexist_hil", "CACHE_START sample_rate=100Hz update_interval=1s registered_channels=3,4,6");
  } else if (tick < 235) {
    if (tick>190) {
      const float p=z->read_voltage(3,false), n=z->read_voltage(4,false);
      if (!match(p,32768) || !match(n,32768)) { fail("cache_values"); return; }
      if (tick%10==0) ESP_LOGI("coexist_hil", "CACHE tick=%u p=%.6f n=%.6f",tick,p,n);
    }
    if (!pid()) { fail("mag_cache"); return; }
    mag->update();
  } else {
    restore();
    ESP_LOGI("coexist_hil", "FINAL PASS cycles=125 frames_min=%u errors=%u mag_startup_errors=%u mag_runtime_errors=%u hc165=NOT_TESTED dac2=8000 dac3=8000 gpio=07",frames,errors,startup_mag_errors,mag->get_error_count()-startup_mag_errors);
    stopped=true; return;
  }
  if (mag->get_error_count()!=startup_mag_errors || !std::isfinite(mx->state) || !std::isfinite(my->state) || !std::isfinite(mz->state) || std::fabs(mx->state)>=8 || std::fabs(my->state)>=8 || std::fabs(mz->state)>=8) { fail("mag_measurement"); return; }
  tick++;
}
