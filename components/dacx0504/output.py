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

def validate_voltage_range(config):
    if "min_voltage" in config and "max_voltage" not in config:
        raise cv.Invalid("min_voltage requires max_voltage")
    if "max_voltage" in config:
        config.setdefault("min_voltage", 0.0)
        if config["min_voltage"] >= config["max_voltage"]:
            raise cv.Invalid("min_voltage must be below max_voltage")
    return config

CONFIG_SCHEMA = cv.All(output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(DACX0504Channel),
        cv.GenerateID(CONF_DACX0504_ID): cv.use_id(DACX0504),
        cv.Optional("min_voltage"): cv.All(cv.voltage, cv.Range(min=0, max=5.5)),
        cv.Optional("max_voltage"): cv.All(cv.voltage, cv.Range(min=0, max=5.5)),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
    }
).extend(cv.COMPONENT_SCHEMA), validate_voltage_range)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_DACX0504_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    if "max_voltage" in config:
        cg.add(var.set_voltage_range(config["min_voltage"], config["max_voltage"]))
