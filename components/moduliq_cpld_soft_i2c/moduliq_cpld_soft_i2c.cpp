#include "moduliq_cpld_soft_i2c.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::moduliq_cpld_soft_i2c {
static const char *const TAG = "moduliq_cpld_soft_i2c";
void CPLDSoftI2C::setup() {
  if (!parent_ || sda_ > 15 || scl_ > 15 || sda_ == scl_ || sda_/8 != scl_/8) {
    mark_failed(); return;
  }
  mask_ = (1U << sda_) | (1U << scl_);
  reserved_ = parent_->reserve_pins(this, mask_);
  if (!reserved_) { ESP_LOGE(TAG, "Pin reservation failed"); mark_failed(); return; }
  if (parent_->ready()) initialize_();
}
void CPLDSoftI2C::loop() {
  if (is_failed()) return;
  if (!parent_->ready()) { initialized_ = false; return; }
  if (!initialized_ && !initialize_()) return;
  if (scan_requested_ && !scanned_) {
    const auto result = write_readv(scan_address_, nullptr, 0, nullptr, 0);
    if (result == i2c::ERROR_OK) ESP_LOGI(TAG, "Found device at 0x%02X", scan_address_);
    if (++scan_address_ == 120 || (result != i2c::ERROR_OK && result != i2c::ERROR_NOT_ACKNOWLEDGED))
      scanned_ = true;
  }
}
void CPLDSoftI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "CPLD software I2C SDA=%u SCL=%u, half-period=%u us, stretch timeout=%u us",
                sda_, scl_, (unsigned) half_period_us_, (unsigned) stretch_timeout_us_);
}
void CPLDSoftI2C::pause_() { delayMicroseconds(half_period_us_); }
bool CPLDSoftI2C::initialize_() {
  if (!reserved_ || !parent_->ready()) return false;
  initialized_ = parent_->configure_reserved(this, mask_);
  parent_generation_ = parent_->generation();
  if (!initialized_) status_set_warning();
  return initialized_;
}
bool CPLDSoftI2C::levels_(bool sda, bool scl) {
  if (parent_->write_reserved(this, mask_, (sda ? 1U << sda_ : 0) | (scl ? 1U << scl_ : 0))) return true;
  error_ = i2c::ERROR_UNKNOWN; initialized_ = false; return false;
}
bool CPLDSoftI2C::sample_(bool &sda, bool &scl) {
  uint8_t pads;
  if (!parent_->read_bank(sda_/8, pads)) { error_ = i2c::ERROR_UNKNOWN; return false; }
  sda = pads & (1U << (sda_%8)); scl = pads & (1U << (scl_%8)); return true;
}
bool CPLDSoftI2C::raise_scl_(bool sda) {
  if (in_transaction_ && uint32_t(micros() - transaction_start_) >= 500000) {
    error_ = i2c::ERROR_TIMEOUT; return false;
  }
  if (!levels_(sda, true)) return false;
  uint32_t start = micros();
  bool actual_sda, actual_scl;
  do {
    if (!sample_(actual_sda, actual_scl)) return false;
    if (actual_scl) { pause_(); return true; }
    delayMicroseconds(10);
  } while (uint32_t(micros() - start) < stretch_timeout_us_);
  error_ = i2c::ERROR_TIMEOUT; return false;
}
bool CPLDSoftI2C::start_() {
  if (!levels_(true, false)) return false;
  pause_();
  if (!raise_scl_(true)) return false;
  bool sda, scl;
  if (!sample_(sda, scl)) return false;
  if (!sda) { error_ = i2c::ERROR_UNKNOWN; return false; }
  if (!levels_(false, true)) return false;
  pause_();
  if (!levels_(false, false)) return false;
  pause_(); return true;
}
bool CPLDSoftI2C::stop_() {
  if (!levels_(false, false)) return false;
  pause_();
  if (!raise_scl_(false)) return false;
  if (!levels_(true, true)) return false;
  pause_();
  bool sda, scl;
  if (!sample_(sda, scl)) return false;
  if (!sda || !scl) { error_ = i2c::ERROR_UNKNOWN; return false; }
  return true;
}
bool CPLDSoftI2C::recover_() {
  if (!raise_scl_(true)) return false;
  bool sda, scl;
  if (!sample_(sda, scl)) return false;
  if (sda) return true;
  for (uint8_t n = 0; n < 9; n++) {
    if (!levels_(true, false)) return false;
    pause_();
    if (!raise_scl_(true) || !sample_(sda, scl)) return false;
    if (sda) break;
  }
  return stop_();
}
bool CPLDSoftI2C::write_bit_(bool bit) {
  if (!levels_(bit, false)) return false;
  pause_();
  if (!raise_scl_(bit)) return false;
  // Single-master bus: a released data bit must really be high.
  if (bit) {
    bool sda, scl;
    if (!sample_(sda, scl)) return false;
    if (!sda) { error_ = i2c::ERROR_UNKNOWN; return false; }
  }
  if (!levels_(bit, false)) return false;
  pause_(); return true;
}
bool CPLDSoftI2C::read_bit_(bool &bit) {
  if (!levels_(true, false)) return false;
  pause_();
  if (!raise_scl_(true)) return false;
  bool scl;
  if (!sample_(bit, scl)) return false;
  if (!levels_(true, false)) return false;
  pause_(); return true;
}
bool CPLDSoftI2C::write_byte_(uint8_t byte) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) if (!write_bit_(byte & mask)) return false;
  bool nack;
  if (!read_bit_(nack)) return false;
  if (nack) { error_ = i2c::ERROR_NOT_ACKNOWLEDGED; return false; }
  return true;
}
bool CPLDSoftI2C::read_byte_(uint8_t &byte, bool last) {
  byte = 0;
  for (uint8_t n = 0; n < 8; n++) {
    bool bit;
    if (!read_bit_(bit)) return false;
    byte = (byte << 1) | bit;
  }
  return write_bit_(last);
}
i2c::ErrorCode CPLDSoftI2C::write_readv(uint8_t address, const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_len) {
  if (address > 0x7F || (tx_len && !tx) || (rx_len && !rx)) return i2c::ERROR_INVALID_ARGUMENT;
  // Bound synchronous calls to keep ordinary sensor operations short.
  if (tx_len > 32 || rx_len > 32 || tx_len + rx_len > 32) return i2c::ERROR_TOO_LARGE;
  if (is_failed() || !reserved_ || !parent_->ready()) return i2c::ERROR_NOT_INITIALIZED;
  if ((!initialized_ || parent_generation_ != parent_->generation()) && !initialize_())
    return i2c::ERROR_NOT_INITIALIZED;
  if (!parent_->begin_pin_transaction(this)) return i2c::ERROR_NOT_INITIALIZED;
  error_ = i2c::ERROR_OK;
  transaction_start_ = micros(); in_transaction_ = true;
  bool ok = recover_() && start_();
  if (ok && (tx_len || !rx_len)) {
    ok = write_byte_(address << 1);
    for (size_t n = 0; ok && n < tx_len; n++) ok = write_byte_(tx[n]);
    if (ok && rx_len) ok = start_();
  }
  if (ok && rx_len) {
    ok = write_byte_((address << 1) | 1);
    for (size_t n = 0; ok && n < rx_len; n++) ok = read_byte_(rx[n], n+1 == rx_len);
  }
  i2c::ErrorCode operation_error = error_;
  in_transaction_ = false;
  bool stopped = stop_();
  if (!stopped) {
    // Best-effort open-drain release; never replay a possibly completed write.
    levels_(true, true);
    initialized_ = false;
  }
  parent_->end_pin_transaction(this);
  if (!ok) { status_set_warning(); return operation_error; }
  if (!stopped) { status_set_warning(); return error_; }
  status_clear_warning(); return i2c::ERROR_OK;
}
}
