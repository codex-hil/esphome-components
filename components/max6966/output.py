import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import CONF_MAX6966_ID, MAX6966Component, max6966_ns

DEPENDENCIES = ["max6966"]

MAX6966Output = max6966_ns.class_("MAX6966Output", output.FloatOutput, cg.Component)

CONF_FULL_CURRENT = "full_current"
CONF_OPEN_DRAIN = "open_drain"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MAX6966Output),
        cv.GenerateID(CONF_MAX6966_ID): cv.use_id(MAX6966Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=9),
        cv.Optional(CONF_FULL_CURRENT, default=True): cv.boolean,
        cv.Optional(CONF_OPEN_DRAIN, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAX6966_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_full_current(config[CONF_FULL_CURRENT]))
    cg.add(var.set_open_drain(config[CONF_OPEN_DRAIN]))
    cg.add(parent.register_output(var))
