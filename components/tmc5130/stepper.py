from esphome import pins
import esphome.codegen as cg
from esphome.components import spi, stepper
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import tmc5130_ns

DEPENDENCIES = ["spi"]

CONF_RSENSE = "rsense"
CONF_RMS_CURRENT = "rms_current"
CONF_HOLD_CURRENT_PERCENT = "hold_current_percent"
CONF_MICROSTEPS = "microsteps"
CONF_STEALTHCHOP = "stealthchop"
CONF_TCOOL_THRESHOLD = "tcool_threshold"
CONF_TSTEP = "tstep"
CONF_ENABLE_PIN = "enable_pin"

TMC5130 = tmc5130_ns.class_("TMC5130", stepper.Stepper, cg.Component, spi.SPIDevice)

MICROSTEP_OPTIONS = {
    256: 0,
    128: 1,
    64: 2,
    32: 3,
    16: 4,
    8: 5,
    4: 6,
    2: 7,
    1: 8,
}

CONFIG_SCHEMA = (
    stepper.STEPPER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TMC5130),
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_RSENSE, default=0.11): cv.float_range(min=0.01, max=1.0),
            cv.Optional(CONF_RMS_CURRENT, default=800): cv.int_range(min=100, max=2000),
            cv.Optional(CONF_HOLD_CURRENT_PERCENT, default=50): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_MICROSTEPS, default=16): cv.enum(MICROSTEP_OPTIONS),
            cv.Optional(CONF_STEALTHCHOP, default=True): cv.boolean,
            cv.Optional(CONF_TCOOL_THRESHOLD, default=0): cv.uint32_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE3"))
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "tmc5130", require_miso=True, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await stepper.register_stepper(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_rsense(config[CONF_RSENSE]))
    cg.add(var.set_rms_current(config[CONF_RMS_CURRENT]))
    cg.add(var.set_hold_current_percent(config[CONF_HOLD_CURRENT_PERCENT]))
    cg.add(var.set_microsteps(config[CONF_MICROSTEPS]))
    cg.add(var.set_stealthchop(config[CONF_STEALTHCHOP]))
    cg.add(var.set_tcool_threshold(config[CONF_TCOOL_THRESHOLD]))

    if enable_pin := config.get(CONF_ENABLE_PIN):
        pin = await cg.gpio_pin_expression(enable_pin)
        cg.add(var.set_enable_pin(pin))
