from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RESET_PIN

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@wizath"]
MULTI_CONF = True

CONF_IO_UPDATE_PIN = "io_update_pin"
CONF_REF_CLOCK = "ref_clock"
CONF_PLL_MULTIPLIER = "pll_multiplier"

ad9959_ns = cg.esphome_ns.namespace("ad9959")
AD9959 = ad9959_ns.class_("AD9959", cg.Component, spi.SPIDevice)


def validate_core_clock(config):
    core_clock = config[CONF_REF_CLOCK] * config[CONF_PLL_MULTIPLIER]
    if not 100000000 <= core_clock <= 500000000:
        raise cv.Invalid(
            "AD9959 PLL output must be between 100 MHz and 500 MHz "
            f"(got {core_clock} Hz from ref_clock * pll_multiplier)"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AD9959),
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_IO_UPDATE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_REF_CLOCK, default=25000000): cv.uint32_t,
            cv.Optional(CONF_PLL_MULTIPLIER, default=20): cv.int_range(min=4, max=20),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE3")),
    validate_core_clock,
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "ad9959", require_miso=False, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))

    io_update_pin = await cg.gpio_pin_expression(config[CONF_IO_UPDATE_PIN])
    cg.add(var.set_io_update_pin(io_update_pin))

    cg.add(var.set_ref_clock(config[CONF_REF_CLOCK]))
    cg.add(var.set_pll_multiplier(config[CONF_PLL_MULTIPLIER]))
