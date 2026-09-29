#include "max112xx.h"

#include "esphome/core/log.h"

namespace esphome {
namespace max112xx {

static const char *const TAG = "max112xx";

// Command byte constants
static const uint8_t CMD_START = 0x80;
static const uint8_t CMD_MODE_REG = 0x40;  // MODE=1 for register access
static const uint8_t CMD_READ = 0x01;

// Register addresses
static const uint8_t REG_STAT1 = 0x00;
static const uint8_t REG_CTRL1 = 0x01;
static const uint8_t REG_CTRL2 = 0x02;
static const uint8_t REG_CTRL3 = 0x03;
static const uint8_t REG_DATA = 0x04;

// STAT1 bits
static const uint8_t STAT1_RDY = 0x01;
static const uint8_t STAT1_UR = 0x04;
static const uint8_t STAT1_OR = 0x08;

// CTRL1 bits
static const uint8_t CTRL1_SCYCLE = 0x02;
static const uint8_t CTRL1_FORMAT = 0x04;
static const uint8_t CTRL1_SIGBUF = 0x08;
static const uint8_t CTRL1_REFBUF = 0x10;
static const uint8_t CTRL1_UB = 0x40;
static const uint8_t CTRL1_LINEF = 0x80;

// CTRL3 bits
static const uint8_t CTRL3_NOSCO = 0x02;
static const uint8_t CTRL3_NOSCG = 0x04;
static const uint8_t CTRL3_NOSYSO = 0x08;
static const uint8_t CTRL3_NOSYSG = 0x10;
static const uint8_t CTRL3_DGAIN_SHIFT = 5;

// Calibration command (MODE=0): CAL0 bit
static const uint8_t CMD_CAL_SELF = 0x10;

void MAX112xxSensor::send_command_(uint8_t cmd) {
  this->enable();
  this->transfer_byte(CMD_START | cmd);
  this->disable();
}

uint8_t MAX112xxSensor::read_register8_(uint8_t reg) {
  uint8_t cmd = CMD_START | CMD_MODE_REG | ((reg & 0x0F) << 1) | CMD_READ;
  this->enable();
  this->transfer_byte(cmd);
  uint8_t value = this->transfer_byte(0x00);
  this->disable();
  return value;
}

void MAX112xxSensor::write_register8_(uint8_t reg, uint8_t value) {
  uint8_t cmd = CMD_START | CMD_MODE_REG | ((reg & 0x0F) << 1);
  this->enable();
  this->transfer_byte(cmd);
  this->transfer_byte(value);
  this->disable();
}

int32_t MAX112xxSensor::read_register24_(uint8_t reg) {
  uint8_t cmd = CMD_START | CMD_MODE_REG | ((reg & 0x0F) << 1) | CMD_READ;
  this->enable();
  this->transfer_byte(cmd);
  uint8_t b2 = this->transfer_byte(0x00);
  uint8_t b1 = this->transfer_byte(0x00);
  uint8_t b0 = this->transfer_byte(0x00);
  this->disable();

  int32_t value = ((int32_t) b2 << 16) | ((int32_t) b1 << 8) | b0;
  // Sign extend from 24-bit
  if (value & 0x800000) {
    value |= 0xFF000000;
  }
  return value;
}

void MAX112xxSensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX112xx...");
  this->spi_setup();

  // Build CTRL1: single-cycle mode, 2's complement, user config for buffers/polarity/line freq
  uint8_t ctrl1 = CTRL1_SCYCLE;  // single-cycle mode

  if (this->line_50hz_)
    ctrl1 |= CTRL1_LINEF;
  if (!this->bipolar_)
    ctrl1 |= CTRL1_UB;  // UB=1 means unipolar
  if (this->sig_buf_)
    ctrl1 |= CTRL1_SIGBUF;
  if (this->ref_buf_)
    ctrl1 |= CTRL1_REFBUF;
  // FORMAT=0 = two's complement (default)

  this->write_register8_(REG_CTRL1, ctrl1);

  // Build CTRL3: set gain, disable system calibration registers
  uint8_t ctrl3 = CTRL3_NOSYSO | CTRL3_NOSYSG;
  if (!this->self_calibrate_) {
    // Keep self-cal registers disabled (match POR default)
    ctrl3 |= CTRL3_NOSCG | CTRL3_NOSCO;
  }
  ctrl3 |= (this->gain_ << CTRL3_DGAIN_SHIFT);
  this->write_register8_(REG_CTRL3, ctrl3);

  // Self-calibration
  if (this->self_calibrate_) {
    ESP_LOGD(TAG, "Running self-calibration...");
    this->send_command_(CMD_CAL_SELF);
    delay(300);  // self-cal takes ~200ms, wait 300ms
    ESP_LOGD(TAG, "Self-calibration complete");
  }

  // Verify communication by reading STAT1
  uint8_t stat = this->read_register8_(REG_STAT1);
  if (stat == 0xFF) {
    ESP_LOGE(TAG, "Failed to communicate with MAX112xx (STAT1=0xFF). Check wiring!");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "STAT1 after setup: 0x%02X", stat);
}

void MAX112xxSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX112xx:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ESP_LOGCONFIG(TAG, "  Gain: %u", 1 << this->gain_);
  ESP_LOGCONFIG(TAG, "  Rate: %u", this->rate_);
  ESP_LOGCONFIG(TAG, "  Line frequency: %s Hz", this->line_50hz_ ? "50" : "60");
  ESP_LOGCONFIG(TAG, "  Reference voltage: %.3f V", this->reference_voltage_);
  ESP_LOGCONFIG(TAG, "  Bipolar: %s", this->bipolar_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Signal buffer: %s", this->sig_buf_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Reference buffer: %s", this->ref_buf_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Self-calibration: %s", this->self_calibrate_ ? "enabled" : "disabled");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Sensor", this);
}

float MAX112xxSensor::get_setup_priority() const { return setup_priority::DATA; }

void MAX112xxSensor::update() {
  // Start single-cycle conversion with configured rate
  this->send_command_(this->rate_);

  // Schedule read after conversion completes
  // Worst case at 1 SPS with LINEF=1 is ~1200ms
  uint32_t timeout_ms;
  switch (this->rate_) {
    case MAX112XX_RATE_1SPS:
      timeout_ms = 1200;
      break;
    case MAX112XX_RATE_2_5SPS:
      timeout_ms = 500;
      break;
    case MAX112XX_RATE_5SPS:
      timeout_ms = 250;
      break;
    case MAX112XX_RATE_10SPS:
      timeout_ms = 150;
      break;
    case MAX112XX_RATE_15SPS:
      timeout_ms = 100;
      break;
    case MAX112XX_RATE_30SPS:
      timeout_ms = 50;
      break;
    case MAX112XX_RATE_60SPS:
      timeout_ms = 30;
      break;
    case MAX112XX_RATE_120SPS:
      timeout_ms = 20;
      break;
    default:
      timeout_ms = 1500;
      break;
  }

  this->set_timeout("read", timeout_ms, [this]() { this->read_data_(); });
}

void MAX112xxSensor::read_data_() {
  // Check RDY bit
  uint8_t stat = this->read_register8_(REG_STAT1);
  if (!(stat & STAT1_RDY)) {
    ESP_LOGW(TAG, "Conversion not ready (STAT1=0x%02X)", stat);
    this->publish_state(NAN);
    this->status_set_warning();
    return;
  }

  if (stat & STAT1_OR) {
    ESP_LOGW(TAG, "Input overrange (clipped to max)");
  }
  if (stat & STAT1_UR) {
    ESP_LOGW(TAG, "Input underrange (clipped to min)");
  }

  int32_t raw = this->read_register24_(REG_DATA);

  // Convert to voltage
  // DATA register is always 24-bit, MSB-aligned for all variants (16/18/20/24-bit)
  // With gain: effective input range = VREF / gain
  float gain_value = static_cast<float>(1 << this->gain_);
  float voltage;
  if (this->bipolar_) {
    // Two's complement: -8388608 to +8388607 maps to -VREF/gain to +VREF/gain
    voltage = (static_cast<float>(raw) / 8388608.0f) * (this->reference_voltage_ / gain_value);
  } else {
    // Unipolar: 0 to 16777215 maps to 0 to VREF/gain
    uint32_t uraw = static_cast<uint32_t>(raw) & 0x00FFFFFF;
    voltage = (static_cast<float>(uraw) / 16777216.0f) * (this->reference_voltage_ / gain_value);
  }

  ESP_LOGD(TAG, "Raw: %d (0x%06X), Voltage: %.6f V", raw, raw & 0x00FFFFFF, voltage);
  this->publish_state(voltage);
  this->status_clear_warning();
}

}  // namespace max112xx
}  // namespace esphome
