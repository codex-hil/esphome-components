import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

from . import ADS8166, ads8166_ns

DEPENDENCIES = ["ads8166"]

CONF_ADS8166_ID = "ads8166_id"

ADS8166Sensor = ads8166_ns.class_("ADS8166Sensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ADS8166Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=5,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_ADS8166_ID): cv.use_id(ADS8166),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_ADS8166_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
