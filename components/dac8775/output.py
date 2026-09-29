import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_RANGE

from . import DAC8775, dac8775_ns

DEPENDENCIES = ["dac8775"]

CONF_DAC8775_ID = "dac8775_id"

DAC8775Channel = dac8775_ns.class_("DAC8775Channel", output.FloatOutput, cg.Component)
OutputRange = dac8775_ns.enum("OutputRange")

RANGE_OPTIONS = {
    "0-5V": OutputRange.VOLTAGE_0_5V,
    "0-10V": OutputRange.VOLTAGE_0_10V,
    "+-5V": OutputRange.VOLTAGE_PM5V,
    "+-10V": OutputRange.VOLTAGE_PM10V,
    "3.5-23.5mA": OutputRange.CURRENT_3_5_23_5MA,
    "0-20mA": OutputRange.CURRENT_0_20MA,
    "0-24mA": OutputRange.CURRENT_0_24MA,
    "+-24mA": OutputRange.CURRENT_PM24MA,
    "0-6V": OutputRange.VOLTAGE_0_6V,
    "0-12V": OutputRange.VOLTAGE_0_12V,
    "+-6V": OutputRange.VOLTAGE_PM6V,
    "+-12V": OutputRange.VOLTAGE_PM12V,
    "4-20mA": OutputRange.CURRENT_4_20MA,
}

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(DAC8775Channel),
        cv.GenerateID(CONF_DAC8775_ID): cv.use_id(DAC8775),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=3),
        cv.Required(CONF_RANGE): cv.enum(RANGE_OPTIONS),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_DAC8775_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_range(config[CONF_RANGE]))
