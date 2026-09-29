import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import AD9959, ad9959_ns

DEPENDENCIES = ["ad9959"]

CONF_AD9959_ID = "ad9959_id"

AD9959Channel = ad9959_ns.class_("AD9959Channel", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(AD9959Channel),
        cv.GenerateID(CONF_AD9959_ID): cv.use_id(AD9959),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_AD9959_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
