# SPDX-License-Identifier: MIT
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_SENSOR,
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

rtd_ns = cg.esphome_ns.namespace("rtd")
RTDSensor = rtd_ns.class_("RTDSensor", cg.Component, sensor.Sensor)
CONF_NOMINAL_RESISTANCE = "nominal_resistance"

def validate_source(config):
    if config[CONF_ID].id is not None and config[CONF_ID].id == config[CONF_SENSOR].id:
        raise cv.Invalid("RTD cannot use itself as its resistance source")
    return config


CONFIG_SCHEMA = cv.All(sensor.sensor_schema(
    RTDSensor,
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend({
    cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_NOMINAL_RESISTANCE): cv.All(cv.resistance, cv.one_of(100.0, 1000.0, float=True)),
}).extend(cv.COMPONENT_SCHEMA), validate_source)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    source = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(source))
    cg.add(var.set_nominal_resistance(config[CONF_NOMINAL_RESISTANCE]))
