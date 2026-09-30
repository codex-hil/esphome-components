#include "moduliq_cpld_flash.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::moduliq_cpld_flash {
static const char *const TAG = "moduliq_cpld_flash";
void CPLDFlash::report_(const char *state, bool warning) {
  if (warning) { status_set_warning(); ESP_LOGW(TAG, "%s", state); }
  else status_clear_warning();
  if (status_) status_->publish_state(state);
}
void CPLDFlash::setup() { report_(enabled_ ? "idle" : "disabled", false); }
bool CPLDFlash::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled && active_) {
    cleanup_pending_ = true; retried_ms_ = millis(); return false;
  }
  if (!enabled && (owned_ || cleanup_pending_)) return release();
  return true;
}
bool CPLDFlash::acquire(bool target_ready) {
  if (!enabled_ || is_failed() || !gpio_ || !gpio_->ready() || !target_ready || owned_ || active_ || cleanup_pending_) return false;
  if (!spi_initialized_) { spi_setup(); spi_initialized_ = true; }
  // GPIO owns the only CFG shadow/RMW. Failure may mean the write reached hardware.
  if (!gpio_->acquire_flash(this)) {
    if (gpio_->has_flash_lease(this)) {
      owned_ = true; cleanup_pending_ = true;
      release();
    }
    report_("flash_acquire_failed", true);
    return false;
  }
  owned_ = true;
  report_("owned", false);
  return true;
}
bool CPLDFlash::transfer(const uint8_t *tx, uint8_t *rx, size_t size) {
  if (!enabled_ || !owned_ || cleanup_pending_ || active_) return false;
  if (!tx || size == 0 || size > 4096 || !gpio_->verify_flash_owner(this)) { release(); return false; }
  active_ = true;
  enable();
  for (size_t i = 0; i < size; i++) {
    uint8_t received = transfer_byte(tx[i]);
    if (rx) rx[i] = received;
  }
  disable();  // CS high before any CFG write, also between successive transactions.
  active_ = false;
  return true;  // SPI delegate has no transport error indication; verify memory protocol in caller.
}
bool CPLDFlash::release() {
  if (active_) return false;
  if (!owned_ && !cleanup_pending_) return true;
  if (!gpio_->release_flash(this)) {
    cleanup_pending_ = true; retried_ms_ = millis();
    report_("flash_release_failed", true); return false;
  }
  owned_ = false; cleanup_pending_ = false;
  report_("released", false); return true;
}
bool CPLDFlash::transaction(const uint8_t *tx, uint8_t *rx, size_t size, bool target_ready) {
  if (!acquire(target_ready)) return false;
  bool ok = transfer(tx, rx, size);
  bool released = release();
  return ok && released;
}
void CPLDFlash::loop() {
  if (cleanup_pending_ && millis() - retried_ms_ >= 100) release();
}
void CPLDFlash::dump_config() {
  ESP_LOGCONFIG(TAG, "CPLD shared Flash CS2, target profile %s", profile_.c_str());
  ESP_LOGCONFIG(TAG, "  Target isolation and reset/boot sequence must be completed by caller");
}
}  // namespace esphome::moduliq_cpld_flash
