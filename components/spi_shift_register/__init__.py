import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID, CONF_MODEL

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["binary_sensor", "output"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("spi_shift_register")
Model = ns.enum("Model")
MODELS = {"HC165": Model.HC165, "HC595": Model.HC595}
SPIShiftRegister = ns.class_("SPIShiftRegister", cg.PollingComponent, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SPIShiftRegister),
    cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True),
    cv.Optional("load_pulse_verified", default=False): cv.boolean,
    cv.Optional("initial_value", default=0xFF): cv.hex_uint8_t,
}).extend(cv.polling_component_schema("1s")).extend(
    spi.spi_device_schema(cs_pin_required=True, default_mode="MODE0", default_data_rate="100kHz")
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_load_pulse_verified(config["load_pulse_verified"]))
    cg.add(var.set_initial_value(config["initial_value"]))
