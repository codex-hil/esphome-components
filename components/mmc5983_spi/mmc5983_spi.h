#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mmc5983_spi {

// Register addresses
static const uint8_t MMC5983_REG_XOUT0 = 0x00;
static const uint8_t MMC5983_REG_XOUT1 = 0x01;
static const uint8_t MMC5983_REG_YOUT0 = 0x02;
static const uint8_t MMC5983_REG_YOUT1 = 0x03;
static const uint8_t MMC5983_REG_ZOUT0 = 0x04;
static const uint8_t MMC5983_REG_ZOUT1 = 0x05;
static const uint8_t MMC5983_REG_XYZOUT2 = 0x06;
static const uint8_t MMC5983_REG_TOUT = 0x07;
static const uint8_t MMC5983_REG_STATUS = 0x08;
static const uint8_t MMC5983_REG_CTRL0 = 0x09;
static const uint8_t MMC5983_REG_CTRL1 = 0x0A;
static const uint8_t MMC5983_REG_CTRL2 = 0x0B;
static const uint8_t MMC5983_REG_CTRL3 = 0x0C;
static const uint8_t MMC5983_REG_PRODUCT_ID = 0x2F;

// Control Register 0 bits
static const uint8_t MMC5983_CTRL0_TM_M = 0x01;           // Take measurement
static const uint8_t MMC5983_CTRL0_TM_T = 0x02;           // Take temperature measurement
static const uint8_t MMC5983_CTRL0_INT_MEAS_DONE = 0x04;  // Enable measurement done interrupt
static const uint8_t MMC5983_CTRL0_SET = 0x08;            // SET operation
static const uint8_t MMC5983_CTRL0_RESET = 0x10;          // RESET operation
static const uint8_t MMC5983_CTRL0_AUTO_SR_EN = 0x20;     // Auto SET/RESET enable
static const uint8_t MMC5983_CTRL0_OTP_READ = 0x40;       // OTP read

// Control Register 1 bits
static const uint8_t MMC5983_CTRL1_BW0 = 0x01;         // Bandwidth bit 0
static const uint8_t MMC5983_CTRL1_BW1 = 0x02;         // Bandwidth bit 1
static const uint8_t MMC5983_CTRL1_X_INHIBIT = 0x04;   // Inhibit X channel
static const uint8_t MMC5983_CTRL1_YZ_INHIBIT = 0x18;  // Inhibit Y/Z channels (2 bits)
static const uint8_t MMC5983_CTRL1_SW_RST = 0x80;      // Software reset

// Status Register bits
static const uint8_t MMC5983_STATUS_MEAS_M_DONE = 0x01;    // Magnetic measurement done
static const uint8_t MMC5983_STATUS_MEAS_T_DONE = 0x02;    // Temperature measurement done
static const uint8_t MMC5983_STATUS_OTP_READ_DONE = 0x10;  // OTP read done

// Product ID
static const uint8_t MMC5983_PRODUCT_ID = 0x30;

// Full scale range and sensitivity (18-bit mode)
static const float MMC5983_SENSITIVITY = 16384.0f;  // counts/Gauss
static const uint32_t MMC5983_NULL_FIELD = 131072;  // 18-bit zero field output

class MMC5983SPIComponent : public PollingComponent,
                            public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                  spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_x_sensor(sensor::Sensor *x_sensor) { x_sensor_ = x_sensor; }
  void set_y_sensor(sensor::Sensor *y_sensor) { y_sensor_ = y_sensor; }
  void set_z_sensor(sensor::Sensor *z_sensor) { z_sensor_ = z_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }

  /// Read a register
  uint8_t read_register(uint8_t reg);

  /// Write a register
  void write_register(uint8_t reg, uint8_t value);

  /// Get measurement X (like SparkFun library)
  uint32_t get_measurement_x();

  /// Get measurement Y (like SparkFun library)
  uint32_t get_measurement_y();

  /// Get measurement Z (like SparkFun library)
  uint32_t get_measurement_z();

  /// Perform SET operation
  void perform_set();

  /// Perform RESET operation
  void perform_reset();

  /// Read magnetic field data
  bool read_magnetic_data();

 protected:
  sensor::Sensor *x_sensor_{nullptr};
  sensor::Sensor *y_sensor_{nullptr};
  sensor::Sensor *z_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
};

}  // namespace mmc5983_spi
}  // namespace esphome
