from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@wizath"]
MULTI_CONF = True

CONF_GAIN = "gain"
CONF_LDAC_PIN = "ldac_pin"
CONF_REFERENCE = "reference"
CONF_REFERENCE_DIVIDER = "reference_divider"

MODEL_DAC80504 = "DAC80504"
MODEL_DAC70504 = "DAC70504"
MODEL_DAC60504 = "DAC60504"

dacx0504_ns = cg.esphome_ns.namespace("dacx0504")
DACX0504 = dacx0504_ns.class_("DACX0504", cg.Component, spi.SPIDevice)

DACX0504Model = dacx0504_ns.enum("DACX0504Model")
MODEL = {
    MODEL_DAC80504: DACX0504Model.DAC80504_MODEL,
    MODEL_DAC70504: DACX0504Model.DAC70504_MODEL,
    MODEL_DAC60504: DACX0504Model.DAC60504_MODEL,
}

DACX0504Reference = dacx0504_ns.enum("DACX0504Reference")
REFERENCE = {
    "INTERNAL": DACX0504Reference.DACX0504_INTERNAL_REFERENCE,
    "EXTERNAL": DACX0504Reference.DACX0504_EXTERNAL_REFERENCE,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DACX0504),
            cv.Optional(CONF_MODEL, default=MODEL_DAC80504): cv.one_of(
                MODEL_DAC80504, MODEL_DAC70504, MODEL_DAC60504, upper=True, space=""
            ),
            cv.Optional(CONF_REFERENCE, default="INTERNAL"): cv.enum(
                REFERENCE, upper=True, space=""
            ),
            cv.Optional(CONF_REFERENCE_DIVIDER, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_GAIN, default=1): cv.one_of(1, 2, int=True),
            cv.Optional(CONF_LDAC_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE1"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_model(MODEL[config[CONF_MODEL]]))
    cg.add(var.set_reference(config[CONF_REFERENCE]))
    cg.add(var.set_reference_divider(config[CONF_REFERENCE_DIVIDER]))
    cg.add(var.set_gain(config[CONF_GAIN]))

    if CONF_LDAC_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_LDAC_PIN])
        cg.add(var.set_ldac_pin(pin))
