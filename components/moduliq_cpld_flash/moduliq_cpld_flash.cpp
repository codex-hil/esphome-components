#include "moduliq_cpld_flash.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::moduliq_cpld_flash {
CPLDFlash *CPLDFlash::bus_active_ = nullptr;
static const char *const TAG = "moduliq_cpld_flash";
void CPLDFlash::report_(const char *state, bool warning) {
  if (warning) { status_set_warning(); ESP_LOGW(TAG, "%s", state); }
  else status_clear_warning();
  if (status_) status_->publish_state(state);
}
void CPLDFlash::setup() { report_(enabled_ ? "idle" : "disabled", false); }
bool CPLDFlash::set_enabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled && session_owner_) return false;
  if (!enabled && active_) {
    cleanup_pending_ = true; retried_ms_ = millis(); return false;
  }
  if (!enabled && (owned_ || cleanup_pending_)) return release();
  return true;
}
bool CPLDFlash::acquire(bool target_ready) {
  return !session_owner_ && acquire_(target_ready);
}
bool CPLDFlash::acquire_(bool target_ready) {
  if (bus_active_ || !enabled_ || is_failed() || !gpio_ || !gpio_->ready() || !target_ready || owned_ || active_ || cleanup_pending_) return false;
  if (!spi_initialized_) { spi_setup(); spi_initialized_ = true; }
  if (!spi_is_ready()) return false;
  // GPIO owns the only CFG shadow/RMW. Failure may mean the write reached hardware.
  if (!gpio_->acquire_flash(this)) {
    if (gpio_->has_flash_lease(this)) {
      owned_ = true; cleanup_pending_ = true;
      release_();
    }
    report_("flash_acquire_failed", true);
    return false;
  }
  owned_ = true;
  report_("owned", false);
  return true;
}
bool CPLDFlash::transfer(const uint8_t *tx, uint8_t *rx, size_t size) {
  if (session_owner_ || bus_active_ || !enabled_ || !owned_ || cleanup_pending_ || active_) return false;
  if (!tx || size == 0 || size > 4096 || !gpio_->verify_flash_owner(this)) { release(); return false; }
  active_ = true; bus_active_ = this;
  enable();
  for (size_t i = 0; i < size; i++) {
    uint8_t received = transfer_byte(tx[i]);
    if (rx) rx[i] = received;
  }
  disable();  // CS high before any CFG write, also between successive transactions.
  active_ = false; bus_active_ = nullptr;
  return true;  // SPI delegate has no transport error indication; verify memory protocol in caller.
}
bool CPLDFlash::release() {
  if (session_owner_) return false;
  return release_();
}
bool CPLDFlash::release_() {
  if (active_ || bus_active_) return false;
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
  if (cleanup_pending_ && !session_dirty_ && millis() - retried_ms_ >= 100) release_();
}
void CPLDFlash::dump_config() {
  ESP_LOGCONFIG(TAG, "CPLD shared Flash CS2, target profile %s", profile_.c_str());
  ESP_LOGCONFIG(TAG, "  Target isolation and reset/boot sequence must be completed by caller");
}
}  // namespace esphome::moduliq_cpld_flash

namespace esphome::moduliq_cpld_flash {
bool CPLDFlash::reserve_session(const void *owner) {
  if (!owner || session_owner_ || owned_ || active_ || cleanup_pending_ || !enabled_) return false;
  session_owner_ = owner; session_dirty_ = false;
  return true;
}
bool CPLDFlash::acquire_session(const void *owner, bool target_ready) {
  return owner && session_owner_ == owner && acquire_(target_ready);
}
bool CPLDFlash::transfer_session(const void *owner, const uint8_t *tx, size_t write_size,
                                 uint8_t *rx, size_t read_size) {
  if (!owner || session_owner_ != owner) return false;
  return transfer_split_(tx, write_size, rx, read_size);
}
bool CPLDFlash::transfer_split_(const uint8_t *tx, size_t write_size, uint8_t *rx, size_t read_size) {
  // No callbacks/yields within a frame. The host SPI delegate locks its physical bus;
  // addrspi selects the nested address before CS, and releases it only after both phases.
  // The global guard also rejects reentrant Flash calls across different instances.
  if (bus_active_ || active_ || !spi_is_ready() || !owned_ || cleanup_pending_ ||
      (!tx && write_size) || (!rx && read_size) ||
      write_size > MAX_TRANSFER || read_size > MAX_TRANSFER - write_size ||
      write_size + read_size == 0) return false;
  bus_active_ = this; active_ = true;
  if (!gpio_->verify_flash_owner(this)) {
    active_ = false; bus_active_ = nullptr;
    report_("flash_owner_uncertain", true);
    return false;  // A session must not hand back a possibly busy chip on transport failure.
  }
  session_dirty_ = true;  // Raw bytes can start an internal operation; no opcode database here.
  enable();
  if (write_size) delegate_->write_array(tx, write_size);
  if (read_size) delegate_->read_array(rx, read_size);
  disable();
  active_ = false; bus_active_ = nullptr;
  return true;
}
bool CPLDFlash::release_session(const void *owner, bool device_idle) {
  if (!owner || owner != session_owner_ || active_ || (session_dirty_ && !device_idle)) return false;
  session_dirty_ = false;
  return release_();
}
bool CPLDFlash::end_session(const void *owner) {
  if (!owner || owner != session_owner_ || owned_ || active_ || cleanup_pending_) return false;
  session_owner_ = nullptr; session_dirty_ = false;
  return true;
}
}  // namespace esphome::moduliq_cpld_flash
