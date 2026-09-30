#include "moduliq_cpld_i2c.h"
#ifdef USE_MODULIQ_CPLD_GPIO
#include "esphome/components/moduliq_cpld_gpio/moduliq_cpld_gpio.h"
#endif
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::moduliq_cpld_i2c {
static const char *const TAG = "moduliq_cpld_i2c";
void CPLDReadout::report_(const char *state, bool warning) {
  if (warning) { status_set_warning(); ESP_LOGW(TAG, "%s", state); }
  else status_clear_warning();
  if (status_ != nullptr) status_->publish_state(state);
}
bool CPLDReadout::reserve_() {
#ifdef USE_MODULIQ_CPLD_GPIO
  if (gpio_ != nullptr && !gpio_->reserve_upper(this)) return false;
#endif
  reserved_ = true; return true;
}
void CPLDReadout::unreserve_() {
#ifdef USE_MODULIQ_CPLD_GPIO
  if (gpio_ != nullptr) gpio_->release_upper(this);
#endif
  reserved_ = false;
}
bool CPLDReadout::disable_adc_() {
  uint8_t ctrl = 0xFF;
  return write_byte(0x30, 0) && read_(0x30, &ctrl, 1) && ctrl == 0;
}
bool CPLDReadout::set_adc_enabled(bool value) {
  if (value && !adc_configured_) return false;
  if (value && !adc_enabled_) adc_needs_cleanup_ = true;
  adc_enabled_ = value;
  if (!value && state_ == ADCState::WAITING) finish_adc_(false, "adc_cancelled");
  return state_ != ADCState::CLEANUP;
}
void CPLDReadout::setup() {
  uint8_t identity[2];
  if (!read_(0, identity, sizeof(identity))) { report_("identity_read_failed", true); mark_failed(); return; }
  // Only YAML/application activation allows access to the optional CPLD ADC block.
  if (adc_enabled_) {
    if (!reserve_()) { report_("adc_reservation_failed", true); mark_failed(); return; }
    if (!disable_adc_()) {
      state_ = ADCState::CLEANUP; pending_reason_ = "adc_initial_disable_failed";
      report_(pending_reason_, true);
    } else { adc_needs_cleanup_ = false; unreserve_(); }
  }
}
void CPLDReadout::update() {
  if (is_failed() || state_ != ADCState::IDLE || updating_ || publishing_) return;
  updating_ = true;
  struct UpdateGuard { bool &flag; ~UpdateGuard() { flag = false; } } guard{updating_};
  uint8_t snapshot[5], fault[2];
  bool base_ok = read_(0, snapshot, sizeof(snapshot));
  bool fault_ok = read_(0x20, fault, sizeof(fault));
  readout_valid_ = base_ok && fault_ok;
  float values[8] = {NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN};
  if (base_ok) {
    values[0] = snapshot[0]; values[1] = snapshot[1]; values[2] = snapshot[2] & 15;
    values[3] = snapshot[2] >> 4; values[4] = snapshot[3]; values[5] = snapshot[4];
  }
  if (fault_ok) { values[6] = fault[0] & 15; values[7] = fault[1] & 0x8F; }
  for (uint8_t i = 0; i < 8; i++) if (numeric_[i]) numeric_[i]->publish_state(values[i]);
  for (const auto &field : digital_) {
    std::string label = "unknown";
    if (base_ok) {
      uint8_t code = ((snapshot[field.upper ? 3 : 4] ^ field.xor_mask) & field.mask) >> field.shift;
      for (const auto &entry : field.codes) if (entry.first == code) { label = entry.second; break; }
    }
    field.sensor->publish_state(label);
  }
  bool wants_counters = false;
  for (auto *counter : counters_) wants_counters |= counter != nullptr;
  if (wants_counters) {
    uint8_t counts[4];
    // Exactly one destructive read, no immediate retry. A failed burst may already have cleared some registers.
    bool ok = read_(0x10, counts, sizeof(counts));
    for (uint8_t i = 0; i < 4; i++) if (counters_[i]) counters_[i]->publish_state(ok ? counts[i] : NAN);
    if (!ok) { readout_valid_ = false; report_("counters_lost", true); }
  }
  if (!readout_valid_) report_("readout_incomplete", true);
  else report_("ok", false);
  if (!adc_enabled_) return;
  if (adc_needs_cleanup_) {
    if (!reserve_()) { report_("adc_reservation_failed", true); return; }
    if (!disable_adc_()) {
      state_ = ADCState::CLEANUP; pending_reason_ = "adc_initial_disable_failed";
      polled_ms_ = millis(); report_(pending_reason_, true); return;
    }
    adc_needs_cleanup_ = false; unreserve_();
  }
  if (!reserve_()) { report_("adc_reservation_failed", true); return; }
  pending_valid_ = false; measurement_pending_ = true;
  started_ms_ = polled_ms_ = millis();
  uint8_t ctrl = 0;
  state_ = ADCState::WAITING;
  if (!write_byte(0x30, 3) || !read_(0x30, &ctrl, 1) || ctrl != 2) finish_adc_(false, "adc_start_failed");
}
void CPLDReadout::finish_adc_(bool valid, const char *reason) {
  pending_valid_ = valid; pending_reason_ = reason; state_ = ADCState::CLEANUP;
  polled_ms_ = millis();
  if (!disable_adc_()) { report_("adc_release_failed", true); return; }
  unreserve_(); state_ = ADCState::IDLE;
  if (measurement_pending_) publish_adc_();
}
void CPLDReadout::publish_adc_() {
  publishing_ = true;
  // Results have already been decoded as a complete frame, before disable cleared hardware results.
  for (uint8_t channel = 0; channel < 8; channel++) {
    if (adc_raw_[channel]) adc_raw_[channel]->publish_state(pending_valid_ ? results_[channel] : NAN);
    if (adc_ids_[channel]) {
      std::string label = "unknown";
      if (pending_valid_) for (const auto &band : bands_[channel]) {
        if (results_[channel] >= band.min && results_[channel] <= band.max) { label = band.id; break; }
      }
      adc_ids_[channel]->publish_state(label);
    }
  }
  measurement_pending_ = false;
  publishing_ = false;
  report_(pending_valid_ && !readout_valid_ ? "readout_incomplete" : pending_reason_, !pending_valid_ || !readout_valid_);
}
void CPLDReadout::loop() {
  if (is_failed() || state_ == ADCState::IDLE) return;
  uint32_t now = millis();
  if (state_ == ADCState::CLEANUP) {
    if (now - polled_ms_ < 100) return;
    polled_ms_ = now;
    if (!reserved_ && !reserve_()) return;
    if (disable_adc_()) {
      adc_needs_cleanup_ = false; unreserve_(); state_ = ADCState::IDLE;
      if (measurement_pending_) publish_adc_();
      else report_("ok", false);
    }
    return;
  }
  if (now - started_ms_ >= timeout_ms_) { finish_adc_(false, "adc_timeout"); return; }
  if (now - polled_ms_ < 5) return;
  polled_ms_ = now;
  uint8_t status = 0;
  if (!read_(0x31, &status, 1) || (status & ~3) != 0) { finish_adc_(false, "adc_status_failed"); return; }
  if ((status & 3) != 2) return;
  uint8_t bytes[16];
  if (!read_(0x32, bytes, sizeof(bytes))) { finish_adc_(false, "adc_burst_failed"); return; }
  // Decode all eight values and validate resolution before publishing any channel.
  for (uint8_t i = 0; i < 8; i++) {
    results_[i] = uint16_t(bytes[2 * i]) | (uint16_t(bytes[2 * i + 1]) << 8);
    if (results_[i] >= (1U << adc_bits_)) { finish_adc_(false, "adc_code_out_of_range"); return; }
  }
  finish_adc_(true, "ok");
}
void CPLDReadout::on_shutdown() {
  if (reserved_ && disable_adc_()) unreserve_();
}
void CPLDReadout::dump_config() {
  ESP_LOGCONFIG(TAG, "CPLD I2C readout address 0x%02X; optional ADC %s", address_, adc_enabled_ ? "enabled explicitly" : "disabled");
  ESP_LOGCONFIG(TAG, "  Error counter values are modulo-256 intervals; failed reads lose events");
}
}  // namespace esphome::moduliq_cpld_i2c
