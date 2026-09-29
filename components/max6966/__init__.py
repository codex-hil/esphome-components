import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@wizath"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

max6966_ns = cg.esphome_ns.namespace("max6966")
MAX6966Component = max6966_ns.class_("MAX6966Component", cg.Component, spi.SPIDevice)

CONF_MAX6966_ID = "max6966_id"
CONF_NUM_PORTS = "num_ports"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MAX6966Component),
            cv.Optional(CONF_NUM_PORTS, default=10): cv.int_range(min=1, max=10),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_num_ports(config[CONF_NUM_PORTS]))
