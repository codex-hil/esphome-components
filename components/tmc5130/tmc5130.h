#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/stepper/stepper.h"

namespace esphome {
namespace tmc5130 {

// Register addresses
namespace reg {
static const uint8_t GCONF = 0x00;
static const uint8_t GSTAT = 0x01;
static const uint8_t IOIN = 0x04;
static const uint8_t IHOLD_IRUN = 0x10;
static const uint8_t TPOWERDOWN = 0x11;
static const uint8_t TSTEP = 0x12;
static const uint8_t TPWMTHRS = 0x13;
static const uint8_t TCOOLTHRS = 0x14;
static const uint8_t THIGH = 0x15;
static const uint8_t RAMPMODE = 0x20;
static const uint8_t XACTUAL = 0x21;
static const uint8_t VACTUAL = 0x22;
static const uint8_t VSTART = 0x23;
static const uint8_t A1 = 0x24;
static const uint8_t V1 = 0x25;
static const uint8_t AMAX = 0x26;
static const uint8_t VMAX = 0x27;
static const uint8_t DMAX = 0x28;
static const uint8_t D1 = 0x2A;
static const uint8_t VSTOP = 0x2B;
static const uint8_t TZEROWAIT = 0x2C;
static const uint8_t XTARGET = 0x2D;
static const uint8_t SW_MODE = 0x34;
static const uint8_t RAMP_STAT = 0x35;
static const uint8_t CHOPCONF = 0x6C;
static const uint8_t COOLCONF = 0x6D;
static const uint8_t DRV_STATUS = 0x6F;
static const uint8_t PWMCONF = 0x70;
}  // namespace reg

// GCONF register bits
namespace gconf {
static const uint32_t I_SCALE_ANALOG = 1 << 0;
static const uint32_t INTERNAL_RSENSE = 1 << 1;
static const uint32_t EN_PWM_MODE = 1 << 2;
static const uint32_t ENC_COMMUTATION = 1 << 3;
static const uint32_t SHAFT = 1 << 4;
static const uint32_t DIAG0_ERROR = 1 << 5;
static const uint32_t DIAG0_OTPW = 1 << 6;
static const uint32_t DIAG0_STALL = 1 << 7;
static const uint32_t DIAG1_STALL = 1 << 8;
static const uint32_t DIAG1_INDEX = 1 << 9;
static const uint32_t DIAG1_ONSTATE = 1 << 10;
static const uint32_t DIAG1_STEPS_SKIPPED = 1 << 11;
static const uint32_t DIAG0_INT_PUSHPULL = 1 << 12;
static const uint32_t DIAG1_PUSHPULL = 1 << 13;
static const uint32_t SMALL_HYSTERESIS = 1 << 14;
static const uint32_t STOP_ENABLE = 1 << 15;
static const uint32_t DIRECT_MODE = 1 << 16;
}  // namespace gconf

// CHOPCONF register bits and masks
namespace chopconf {
static const uint8_t TOFF_SHIFT = 0;
static const uint32_t TOFF_MASK = 0x0F << TOFF_SHIFT;
static const uint8_t HSTRT_SHIFT = 4;
static const uint32_t HSTRT_MASK = 0x07 << HSTRT_SHIFT;
static const uint8_t HEND_SHIFT = 7;
static const uint32_t HEND_MASK = 0x0F << HEND_SHIFT;
static const uint32_t DISFDCC = 1 << 12;
static const uint32_t RNDTF = 1 << 13;
static const uint32_t CHM = 1 << 14;
static const uint8_t TBL_SHIFT = 15;
static const uint32_t TBL_MASK = 0x03 << TBL_SHIFT;
static const uint32_t VSENSE = 1 << 17;
static const uint32_t VHIGHFS = 1 << 18;
static const uint32_t VHIGHCHM = 1 << 19;
static const uint8_t SYNC_SHIFT = 20;
static const uint32_t SYNC_MASK = 0x0F << SYNC_SHIFT;
static const uint8_t MRES_SHIFT = 24;
static const uint32_t MRES_MASK = 0x0F << MRES_SHIFT;
static const uint32_t INTPOL = 1 << 28;
static const uint32_t DEDGE = 1 << 29;
static const uint32_t DISS2G = 1 << 30;
}  // namespace chopconf

// IHOLD_IRUN register bits and masks
namespace ihold_irun {
static const uint8_t IHOLD_SHIFT = 0;
static const uint32_t IHOLD_MASK = 0x1F << IHOLD_SHIFT;
static const uint8_t IRUN_SHIFT = 8;
static const uint32_t IRUN_MASK = 0x1F << IRUN_SHIFT;
static const uint8_t IHOLDDELAY_SHIFT = 16;
static const uint32_t IHOLDDELAY_MASK = 0x0F << IHOLDDELAY_SHIFT;
}  // namespace ihold_irun

// PWMCONF register bits and masks
namespace pwmconf {
static const uint8_t PWM_AMPL_SHIFT = 0;
static const uint32_t PWM_AMPL_MASK = 0xFF << PWM_AMPL_SHIFT;
static const uint8_t PWM_GRAD_SHIFT = 8;
static const uint32_t PWM_GRAD_MASK = 0xFF << PWM_GRAD_SHIFT;
static const uint8_t PWM_FREQ_SHIFT = 16;
static const uint32_t PWM_FREQ_MASK = 0x03 << PWM_FREQ_SHIFT;
static const uint32_t PWM_AUTOSCALE = 1 << 18;
static const uint32_t PWM_SYMMETRIC = 1 << 19;
static const uint8_t FREEWHEEL_SHIFT = 20;
static const uint32_t FREEWHEEL_MASK = 0x03 << FREEWHEEL_SHIFT;
}  // namespace pwmconf

// DRV_STATUS register bits
namespace drv_status {
static const uint32_t SG_RESULT_MASK = 0x3FF;
static const uint32_t FSACTIVE = 1 << 15;
static const uint8_t CS_ACTUAL_SHIFT = 16;
static const uint32_t CS_ACTUAL_MASK = 0x1F << CS_ACTUAL_SHIFT;
static const uint32_t STALLGUARD = 1 << 24;
static const uint32_t OT = 1 << 25;
static const uint32_t OTPW = 1 << 26;
static const uint32_t S2GA = 1 << 27;
static const uint32_t S2GB = 1 << 28;
static const uint32_t OLA = 1 << 29;
static const uint32_t OLB = 1 << 30;
static const uint32_t STST = 1 << 31;
}  // namespace drv_status

// RAMP_STAT register bits
namespace ramp_stat {
static const uint32_t STATUS_STOP_L = 1 << 0;
static const uint32_t STATUS_STOP_R = 1 << 1;
static const uint32_t STATUS_LATCH_L = 1 << 2;
static const uint32_t STATUS_LATCH_R = 1 << 3;
static const uint32_t EVENT_STOP_L = 1 << 4;
static const uint32_t EVENT_STOP_R = 1 << 5;
static const uint32_t EVENT_STOP_SG = 1 << 6;
static const uint32_t EVENT_POS_REACHED = 1 << 7;
static const uint32_t VELOCITY_REACHED = 1 << 8;
static const uint32_t POSITION_REACHED = 1 << 9;
static const uint32_t VZERO = 1 << 10;
static const uint32_t T_ZEROWAIT_ACTIVE = 1 << 11;
static const uint32_t SECOND_MOVE = 1 << 12;
static const uint32_t STATUS_SG = 1 << 13;
}  // namespace ramp_stat

// IOIN register
namespace ioin {
static const uint8_t VERSION_SHIFT = 24;
static const uint32_t VERSION_MASK = 0xFF << VERSION_SHIFT;
static const uint8_t TMC5130_VERSION = 0x11;
}  // namespace ioin

// Ramp modes
namespace rampmode {
static const uint8_t POSITION = 0;
static const uint8_t VELOCITY_POS = 1;
static const uint8_t VELOCITY_NEG = 2;
static const uint8_t HOLD = 3;
}  // namespace rampmode

// SPI protocol
namespace spi_const {
static const uint8_t WRITE_BIT = 0x80;
static const uint8_t READ_MASK = 0x7F;
}  // namespace spi_const

// Clock frequency
static const float FCLK = 12000000.0f;

// Velocity/acceleration conversion factors
static const float VELOCITY_FACTOR = 16777216.0f;    // 2^24
static const float ACCEL_FACTOR = 2199023255552.0f;  // 2^41

// Register limits
static const uint32_t VMAX_MAX = 0x7FFFFF;  // 23-bit
static const uint32_t AMAX_MAX = 0xFFFF;    // 16-bit
static const uint32_t AMAX_MIN = 1;

// Current calculation constants
static const float VSENSE_HIGH = 0.180f;  // vsense = 1
static const float VSENSE_LOW = 0.325f;   // vsense = 0
static const float SQRT2 = 1.41421f;
static const float RSENSE_INT = 0.02f;  // internal sense resistor offset
static const uint8_t CS_VSENSE_THRESHOLD = 16;
static const uint8_t CS_MAX = 31;

// Default chopper configuration (SpreadCycle)
namespace defaults {
static const uint8_t TOFF = 4;
static const uint8_t HSTRT = 5;
static const uint8_t HEND = 1;
static const uint8_t TBL = 2;          // 36 clocks
static const uint8_t PWM_AMPL = 0x80;  // 128
static const uint8_t PWM_GRAD = 0x04;
static const uint8_t PWM_FREQ = 1;  // 23.4kHz
static const uint8_t IHOLDDELAY = 6;
static const uint8_t TPOWERDOWN = 10;
static const uint32_t TPWMTHRS = 500;  // StealthChop velocity threshold
static const uint8_t VSTART = 1;
static const uint8_t VSTOP = 10;
}  // namespace defaults

class TMC5130 : public stepper::Stepper,
                public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING,
                                      spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_enable_pin(GPIOPin *pin) { this->enable_pin_ = pin; }
  void set_rsense(float rsense) { this->rsense_ = rsense; }
  void set_rms_current(uint16_t current) { this->rms_current_ = current; }
  void set_hold_current_percent(uint8_t percent) { this->hold_current_percent_ = percent; }
  void set_microsteps(uint8_t mres) { this->mres_ = mres; }
  void set_stealthchop(bool stealthchop) { this->stealthchop_ = stealthchop; }
  void set_tcool_threshold(uint32_t threshold) { this->tcool_threshold_ = threshold; }

  void on_update_speed() override;

 protected:
  void write_register_(uint8_t address, uint32_t data);
  uint32_t read_register_(uint8_t address);

  void configure_driver_();
  void configure_current_();
  void configure_ramp_();
  void update_target_();

  GPIOPin *enable_pin_{nullptr};
  float rsense_{0.11f};
  uint16_t rms_current_{800};
  uint8_t hold_current_percent_{50};
  uint8_t mres_{4};
  bool stealthchop_{true};
  uint32_t tcool_threshold_{0};

  uint32_t chopconf_{0};
  bool vsense_{false};
  int32_t last_target_{0};
  uint8_t status_response_{0};
};

}  // namespace tmc5130
}  // namespace esphome
