import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import DACX0504, dacx0504_ns

DEPENDENCIES = ["dacx0504"]

CONF_DACX0504_ID = "dacx0504_id"

DACX0504Channel = dacx0504_ns.class_(
    "DACX0504Channel", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(DACX0504Channel),
        cv.GenerateID(CONF_DACX0504_ID): cv.use_id(DACX0504),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_DACX0504_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
