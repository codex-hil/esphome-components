#include "mmc5983_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace mmc5983_spi {

static const char *const TAG = "mmc5983";

void MMC5983SPIComponent::setup() {
  this->spi_setup();

  uint8_t product_id = this->read_register(MMC5983_REG_PRODUCT_ID);
  if (product_id != MMC5983_PRODUCT_ID) {
    ESP_LOGW(TAG, "Product ID mismatch: 0x%02X (expected 0x%02X) - continuing anyway", product_id, MMC5983_PRODUCT_ID);
  }

  ESP_LOGCONFIG(TAG, "Product ID: 0x%02X", product_id);

  // Software reset
  this->write_register(MMC5983_REG_CTRL1, MMC5983_CTRL1_SW_RST);
  delay(10);

  // Initialize control registers
  this->write_register(MMC5983_REG_CTRL0, 0x00);
  this->write_register(MMC5983_REG_CTRL1, 0x00);
  this->write_register(MMC5983_REG_CTRL2, 0x00);
  this->write_register(MMC5983_REG_CTRL3, 0x00);

  ESP_LOGCONFIG(TAG, "MMC5983 initialized successfully");
}

void MMC5983SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MMC5983:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "X Axis", this->x_sensor_);
  LOG_SENSOR("  ", "Y Axis", this->y_sensor_);
  LOG_SENSOR("  ", "Z Axis", this->z_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

void MMC5983SPIComponent::update() {
  // Perform SET operation for calibration
  this->perform_set();
  delay(1);

  // Start magnetic field measurement
  this->write_register(MMC5983_REG_CTRL0, MMC5983_CTRL0_TM_M);

  // Wait for measurement to complete (8ms for BW=00)
  delay(10);

  // Check if measurement is done
  uint8_t status = this->read_register(MMC5983_REG_STATUS);
  if (!(status & MMC5983_STATUS_MEAS_M_DONE)) {
    ESP_LOGW(TAG, "Measurement not ready, status: 0x%02X", status);
    return;
  }

  // Read magnetic field data
  if (!this->read_magnetic_data()) {
    ESP_LOGW(TAG, "Failed to read magnetic field data");
    return;
  }

  // Read temperature if sensor is configured
  if (this->temperature_sensor_ != nullptr) {
    this->write_register(MMC5983_REG_CTRL0, MMC5983_CTRL0_TM_T);
    delay(2);

    status = this->read_register(MMC5983_REG_STATUS);
    if (status & MMC5983_STATUS_MEAS_T_DONE) {
      uint8_t temp_raw = this->read_register(MMC5983_REG_TOUT);
      // Temperature range: -75~125°C, 0.8°C/LSB, 0x00 = -75°C
      float temperature = -75.0f + (temp_raw * 0.8f);
      this->temperature_sensor_->publish_state(temperature);
    }
  }
}

uint8_t MMC5983SPIComponent::read_register(uint8_t reg) {
  // Format: 0x80 | register_address for read command
  this->enable();
  this->write_byte(0x80 | reg);
  uint8_t value = this->read_byte();
  this->disable();
  return value;
}

void MMC5983SPIComponent::write_register(uint8_t reg, uint8_t value) {
  // Format: register address directly (no bit shifting)
  this->enable();
  this->write_byte(reg);
  this->write_byte(value);
  this->disable();
}

uint32_t MMC5983SPIComponent::get_measurement_x() {
  uint8_t xout0 = this->read_register(MMC5983_REG_XOUT0);
  uint8_t xout1 = this->read_register(MMC5983_REG_XOUT1);
  uint8_t xyzout2 = this->read_register(MMC5983_REG_XYZOUT2);

  // Combine into 18-bit value: Xout0[17:10] | Xout1[9:2] | XYZout2[7:6]
  uint32_t x_raw = ((uint32_t) xout0 << 10) | ((uint32_t) xout1 << 2) | ((xyzout2 >> 6) & 0x03);
  return x_raw;
}

uint32_t MMC5983SPIComponent::get_measurement_y() {
  uint8_t yout0 = this->read_register(MMC5983_REG_YOUT0);
  uint8_t yout1 = this->read_register(MMC5983_REG_YOUT1);
  uint8_t xyzout2 = this->read_register(MMC5983_REG_XYZOUT2);

  // Combine into 18-bit value: Yout0[17:10] | Yout1[9:2] | XYZout2[5:4]
  uint32_t y_raw = ((uint32_t) yout0 << 10) | ((uint32_t) yout1 << 2) | ((xyzout2 >> 4) & 0x03);
  return y_raw;
}

uint32_t MMC5983SPIComponent::get_measurement_z() {
  uint8_t zout0 = this->read_register(MMC5983_REG_ZOUT0);
  uint8_t zout1 = this->read_register(MMC5983_REG_ZOUT1);
  uint8_t xyzout2 = this->read_register(MMC5983_REG_XYZOUT2);

  // Combine into 18-bit value: Zout0[17:10] | Zout1[9:2] | XYZout2[3:2]
  uint32_t z_raw = ((uint32_t) zout0 << 10) | ((uint32_t) zout1 << 2) | ((xyzout2 >> 2) & 0x03);
  return z_raw;
}

void MMC5983SPIComponent::perform_set() {
  this->write_register(MMC5983_REG_CTRL0, MMC5983_CTRL0_SET);
  // SET operation takes 500ns, but we add small delay to be safe
  delayMicroseconds(10);
}

void MMC5983SPIComponent::perform_reset() {
  this->write_register(MMC5983_REG_CTRL0, MMC5983_CTRL0_RESET);
  // RESET operation takes 500ns, but we add small delay to be safe
  delayMicroseconds(10);
}

bool MMC5983SPIComponent::read_magnetic_data() {
  // Read magnetic field data like SparkFun library - individual register reads
  uint32_t x_raw = this->get_measurement_x();
  uint32_t y_raw = this->get_measurement_y();
  uint32_t z_raw = this->get_measurement_z();

  // Convert to Gauss (subtract null field and divide by sensitivity)
  // Data is unsigned, null field = 131072 (2^17)
  int32_t x_counts = (int32_t) x_raw - MMC5983_NULL_FIELD;
  int32_t y_counts = (int32_t) y_raw - MMC5983_NULL_FIELD;
  int32_t z_counts = (int32_t) z_raw - MMC5983_NULL_FIELD;

  float x_gauss = x_counts / MMC5983_SENSITIVITY;
  float y_gauss = y_counts / MMC5983_SENSITIVITY;
  float z_gauss = z_counts / MMC5983_SENSITIVITY;

  ESP_LOGV(TAG, "Raw: X=0x%05X Y=0x%05X Z=0x%05X", x_raw, y_raw, z_raw);
  ESP_LOGV(TAG, "Gauss: X=%.4f Y=%.4f Z=%.4f", x_gauss, y_gauss, z_gauss);

  // Publish sensor values
  if (this->x_sensor_ != nullptr) {
    this->x_sensor_->publish_state(x_gauss);
  }
  if (this->y_sensor_ != nullptr) {
    this->y_sensor_->publish_state(y_gauss);
  }
  if (this->z_sensor_ != nullptr) {
    this->z_sensor_->publish_state(z_gauss);
  }

  return true;
}

}  // namespace mmc5983_spi
}  // namespace esphome
