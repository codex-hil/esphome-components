"""Simple SPI test component."""

import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@wizath"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

spi_test_ns = cg.esphome_ns.namespace("spi_test")
SPITestComponent = spi_test_ns.class_("SPITestComponent", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SPITestComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False))
)


async def to_code(config):
    """Code generation for SPI test component."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
