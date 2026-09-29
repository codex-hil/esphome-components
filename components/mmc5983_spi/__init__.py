import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@wizath"]
DEPENDENCIES = ["spi"]
MULTI_CONF = True

CONF_MMC5983_SPI_ID = "mmc5983_spi_id"

mmc5983_spi_ns = cg.esphome_ns.namespace("mmc5983_spi")
MMC5983SPIComponent = mmc5983_spi_ns.class_(
    "MMC5983SPIComponent", cg.PollingComponent, spi.SPIDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MMC5983SPIComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
