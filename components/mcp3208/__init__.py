import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_REFERENCE_VOLTAGE

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@wizath"]
MULTI_CONF = True

mcp3208_ns = cg.esphome_ns.namespace("mcp3208")
MCP3208 = mcp3208_ns.class_("MCP3208", cg.Component, spi.SPIDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP3208),
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.All(cv.voltage, cv.Range(min=0.1, max=5.5)),
            cv.Optional("sample_rate"): cv.All(cv.frequency, cv.Range(min=1, max=2000)),
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

    if "sample_rate" in config:
        cg.add(var.set_sample_rate(config["sample_rate"]))
