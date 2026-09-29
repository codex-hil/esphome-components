"""Sensor support for MMC5983 magnetometer."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MAGNET,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from . import CONF_MMC5983_SPI_ID, MMC5983SPIComponent

CONF_FIELD_STRENGTH_X = "field_strength_x"
CONF_FIELD_STRENGTH_Y = "field_strength_y"
CONF_FIELD_STRENGTH_Z = "field_strength_z"
UNIT_GAUSS = "G"

field_strength_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_GAUSS,
    icon=ICON_MAGNET,
    accuracy_decimals=4,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MMC5983_SPI_ID): cv.use_id(MMC5983SPIComponent),
        cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
        cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
        cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config):
    """Generate code for sensor configuration."""
    parent = await cg.get_variable(config[CONF_MMC5983_SPI_ID])

    if x_config := config.get(CONF_FIELD_STRENGTH_X):
        sens = await sensor.new_sensor(x_config)
        cg.add(parent.set_x_sensor(sens))

    if y_config := config.get(CONF_FIELD_STRENGTH_Y):
        sens = await sensor.new_sensor(y_config)
        cg.add(parent.set_y_sensor(sens))

    if z_config := config.get(CONF_FIELD_STRENGTH_Z):
        sens = await sensor.new_sensor(z_config)
        cg.add(parent.set_z_sensor(sens))

    if temp_config := config.get(CONF_TEMPERATURE):
        sens = await sensor.new_sensor(temp_config)
        cg.add(parent.set_temperature_sensor(sens))
