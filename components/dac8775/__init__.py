from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@wizath"]
MULTI_CONF = True

CONF_RESET_PIN = "reset_pin"
CONF_LDAC_PIN = "ldac_pin"
CONF_CLR_PIN = "clr_pin"
CONF_USE_INTERNAL_REFERENCE = "use_internal_reference"

dac8775_ns = cg.esphome_ns.namespace("dac8775")
DAC8775 = dac8775_ns.class_("DAC8775", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DAC8775),
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_LDAC_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CLR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_USE_INTERNAL_REFERENCE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE1"))
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "dac8775", require_miso=True, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))

    if CONF_LDAC_PIN in config:
        ldac_pin = await cg.gpio_pin_expression(config[CONF_LDAC_PIN])
        cg.add(var.set_ldac_pin(ldac_pin))

    if CONF_CLR_PIN in config:
        clr_pin = await cg.gpio_pin_expression(config[CONF_CLR_PIN])
        cg.add(var.set_clr_pin(clr_pin))

    cg.add(var.set_use_internal_reference(config[CONF_USE_INTERNAL_REFERENCE]))
