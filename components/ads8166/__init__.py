from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_REFERENCE_VOLTAGE, CONF_RESET_PIN

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@wizath"]
MULTI_CONF = True

ads8166_ns = cg.esphome_ns.namespace("ads8166")
ADS8166 = ads8166_ns.class_("ADS8166", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS8166),
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="4.096V"): cv.voltage,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE0"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))
