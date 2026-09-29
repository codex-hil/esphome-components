#include "tmc5130.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tmc5130 {

static const char *const TAG = "tmc5130.stepper";

void TMC5130::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TMC5130...");

  this->spi_setup();

  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(false);
  }

  this->read_register_(reg::GSTAT);

  uint32_t version = (this->read_register_(reg::IOIN) & ioin::VERSION_MASK) >> ioin::VERSION_SHIFT;
  if (version != ioin::TMC5130_VERSION) {
    ESP_LOGE(TAG, "TMC5130 not found, got version 0x%02X (expected 0x%02X)", version, ioin::TMC5130_VERSION);
    this->mark_failed();
    return;
  }

  ESP_LOGD(TAG, "TMC5130 found, version 0x%02X", version);

  this->configure_driver_();
  this->configure_current_();
  this->configure_ramp_();

  this->write_register_(reg::XACTUAL, 0);
  this->write_register_(reg::XTARGET, 0);
  this->write_register_(reg::RAMPMODE, rampmode::POSITION);
}

void TMC5130::dump_config() {
  ESP_LOGCONFIG(TAG, "TMC5130:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Enable Pin: ", this->enable_pin_);
  ESP_LOGCONFIG(TAG, "  RSense: %.3f ohm", this->rsense_);
  ESP_LOGCONFIG(TAG, "  RMS Current: %d mA", this->rms_current_);
  ESP_LOGCONFIG(TAG, "  Hold Current: %d%%", this->hold_current_percent_);
  ESP_LOGCONFIG(TAG, "  Microsteps: %d", 256 >> this->mres_);
  ESP_LOGCONFIG(TAG, "  StealthChop: %s", this->stealthchop_ ? "enabled" : "disabled");
  LOG_STEPPER(this);
}

void TMC5130::loop() {
  if (this->target_position != this->last_target_) {
    this->update_target_();
    this->last_target_ = this->target_position;
  }

  this->current_position = static_cast<int32_t>(this->read_register_(reg::XACTUAL));
}

void TMC5130::on_update_speed() { this->configure_ramp_(); }

void TMC5130::write_register_(uint8_t address, uint32_t data) {
  this->enable();
  this->status_response_ = this->transfer_byte(address | spi_const::WRITE_BIT);
  this->transfer_byte((data >> 24) & 0xFF);
  this->transfer_byte((data >> 16) & 0xFF);
  this->transfer_byte((data >> 8) & 0xFF);
  this->transfer_byte(data & 0xFF);
  this->disable();
}

uint32_t TMC5130::read_register_(uint8_t address) {
  this->enable();
  this->status_response_ = this->transfer_byte(address & spi_const::READ_MASK);
  this->transfer_byte(0);
  this->transfer_byte(0);
  this->transfer_byte(0);
  this->transfer_byte(0);
  this->disable();

  this->enable();
  this->status_response_ = this->transfer_byte(address & spi_const::READ_MASK);
  uint32_t data = 0;
  data |= static_cast<uint32_t>(this->transfer_byte(0)) << 24;
  data |= static_cast<uint32_t>(this->transfer_byte(0)) << 16;
  data |= static_cast<uint32_t>(this->transfer_byte(0)) << 8;
  data |= static_cast<uint32_t>(this->transfer_byte(0));
  this->disable();

  return data;
}

void TMC5130::configure_driver_() {
  uint32_t gconf = 0;
  if (this->stealthchop_) {
    gconf |= gconf::EN_PWM_MODE;
  }
  this->write_register_(reg::GCONF, gconf);

  uint32_t chop = 0;
  chop |= (this->mres_ << chopconf::MRES_SHIFT);
  chop |= chopconf::INTPOL;
  chop |= (defaults::TOFF << chopconf::TOFF_SHIFT);
  chop |= (defaults::HSTRT << chopconf::HSTRT_SHIFT);
  chop |= (defaults::HEND << chopconf::HEND_SHIFT);
  chop |= (defaults::TBL << chopconf::TBL_SHIFT);
  this->chopconf_ = chop;
  this->write_register_(reg::CHOPCONF, chop);

  uint32_t pwm = 0;
  pwm |= (defaults::PWM_AMPL << pwmconf::PWM_AMPL_SHIFT);
  pwm |= (defaults::PWM_GRAD << pwmconf::PWM_GRAD_SHIFT);
  pwm |= (defaults::PWM_FREQ << pwmconf::PWM_FREQ_SHIFT);
  pwm |= pwmconf::PWM_AUTOSCALE;
  this->write_register_(reg::PWMCONF, pwm);

  this->write_register_(reg::TPWMTHRS, defaults::TPWMTHRS);

  if (this->tcool_threshold_ > 0) {
    this->write_register_(reg::TCOOLTHRS, this->tcool_threshold_);
  }
}

void TMC5130::configure_current_() {
  float current_a = this->rms_current_ / 1000.0f;
  uint8_t cs = static_cast<uint8_t>(32.0f * SQRT2 * current_a * (this->rsense_ + RSENSE_INT) / VSENSE_LOW - 1.0f);

  if (cs < CS_VSENSE_THRESHOLD) {
    this->vsense_ = true;
    cs = static_cast<uint8_t>(32.0f * SQRT2 * current_a * (this->rsense_ + RSENSE_INT) / VSENSE_HIGH - 1.0f);
  } else {
    this->vsense_ = false;
  }

  if (cs > CS_MAX)
    cs = CS_MAX;

  uint32_t chop = this->chopconf_;
  if (this->vsense_) {
    chop |= chopconf::VSENSE;
  }
  this->write_register_(reg::CHOPCONF, chop);

  uint8_t irun = cs;
  uint8_t ihold = (irun * this->hold_current_percent_) / 100;

  uint32_t ihr = 0;
  ihr |= (ihold << ihold_irun::IHOLD_SHIFT);
  ihr |= (irun << ihold_irun::IRUN_SHIFT);
  ihr |= (defaults::IHOLDDELAY << ihold_irun::IHOLDDELAY_SHIFT);

  this->write_register_(reg::IHOLD_IRUN, ihr);
  this->write_register_(reg::TPOWERDOWN, defaults::TPOWERDOWN);
}

void TMC5130::configure_ramp_() {
  uint32_t vmax = static_cast<uint32_t>((this->max_speed_ * VELOCITY_FACTOR) / FCLK);
  if (vmax > VMAX_MAX)
    vmax = VMAX_MAX;
  this->write_register_(reg::VMAX, vmax);

  uint32_t amax = static_cast<uint32_t>((this->acceleration_ * ACCEL_FACTOR) / (FCLK * FCLK));
  if (amax > AMAX_MAX)
    amax = AMAX_MAX;
  if (amax < AMAX_MIN)
    amax = AMAX_MIN;
  this->write_register_(reg::AMAX, amax);

  uint32_t dmax = static_cast<uint32_t>((this->deceleration_ * ACCEL_FACTOR) / (FCLK * FCLK));
  if (dmax > AMAX_MAX)
    dmax = AMAX_MAX;
  if (dmax < AMAX_MIN)
    dmax = AMAX_MIN;
  this->write_register_(reg::DMAX, dmax);

  this->write_register_(reg::A1, amax);
  this->write_register_(reg::V1, vmax / 2);
  this->write_register_(reg::D1, dmax);

  this->write_register_(reg::VSTART, defaults::VSTART);
  this->write_register_(reg::VSTOP, defaults::VSTOP);
  this->write_register_(reg::TZEROWAIT, 0);
}

void TMC5130::update_target_() { this->write_register_(reg::XTARGET, static_cast<uint32_t>(this->target_position)); }

}  // namespace tmc5130
}  // namespace esphome
