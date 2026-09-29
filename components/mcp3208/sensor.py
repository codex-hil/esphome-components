import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

from . import MCP3208, mcp3208_ns

DEPENDENCIES = ["mcp3208"]

CONF_MCP3208_ID = "mcp3208_id"
CONF_DIFFERENTIAL = "differential"

MCP3208Sensor = mcp3208_ns.class_("MCP3208Sensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MCP3208Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=4,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_MCP3208_ID): cv.use_id(MCP3208),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
            cv.Optional(CONF_DIFFERENTIAL, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MCP3208_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_differential(config[CONF_DIFFERENTIAL]))
