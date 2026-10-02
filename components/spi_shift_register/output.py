import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import output
from esphome.const import CONF_ID, CONF_CHANNEL
from . import SPIShiftRegister, ns, Model

DEPENDENCIES = ["spi_shift_register"]
SPIShiftOutput = ns.class_("SPIShiftOutput", output.BinaryOutput)
CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(SPIShiftOutput),
    cv.Required("spi_shift_register_id"): cv.use_id(SPIShiftRegister),
    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
})

def output_model(value):
    if str(value) != "HC595":
        raise cv.Invalid("Output needs HC595")
    return value

FINAL_VALIDATE_SCHEMA = cv.Schema({
    cv.Required("spi_shift_register_id"): fv.id_declaration_match_schema({
        cv.Required("model"): output_model,
    }),
}, extra=cv.ALLOW_EXTRA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    parent = await cg.get_variable(config["spi_shift_register_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
