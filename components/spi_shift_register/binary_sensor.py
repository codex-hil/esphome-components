import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import binary_sensor
from esphome.const import CONF_CHANNEL
from . import SPIShiftRegister, Model

DEPENDENCIES = ["spi_shift_register"]
CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend({
    cv.Required("spi_shift_register_id"): cv.use_id(SPIShiftRegister),
    cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
})

def input_model(value):
    if str(value) != "HC165":
        raise cv.Invalid("Input needs HC165")
    return value

FINAL_VALIDATE_SCHEMA = cv.Schema({
    cv.Required("spi_shift_register_id"): fv.id_declaration_match_schema({
        cv.Required("model"): input_model,
    }),
}, extra=cv.ALLOW_EXTRA)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    parent = await cg.get_variable(config["spi_shift_register_id"])
    cg.add(parent.register_input(config[CONF_CHANNEL], var))
