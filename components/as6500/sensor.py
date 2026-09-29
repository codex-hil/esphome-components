from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, STATE_CLASS_MEASUREMENT

from . import as6500_ns

DEPENDENCIES = ["spi"]

CONF_REFCLK_DIVISIONS = "refclk_divisions"
CONF_HIGH_RESOLUTION = "high_resolution"
CONF_CHANNEL_COMBINE = "channel_combine"
CONF_USE_CRYSTAL = "use_crystal"
CONF_CHANNEL_1 = "channel_1"
CONF_CHANNEL_2 = "channel_2"
CONF_CHANNEL_3 = "channel_3"
CONF_CHANNEL_4 = "channel_4"

AS6500 = as6500_ns.class_("AS6500", cg.PollingComponent, spi.SPIDevice)

HighResolutionMode = as6500_ns.enum("HighResolutionMode")
HIGH_RESOLUTION_OPTIONS = {
    "OFF": HighResolutionMode.HIGH_RES_OFF,
    "X2": HighResolutionMode.HIGH_RES_X2,
    "X4": HighResolutionMode.HIGH_RES_X4,
}

ChannelCombineMode = as6500_ns.enum("ChannelCombineMode")
CHANNEL_COMBINE_OPTIONS = {
    "NORMAL": ChannelCombineMode.COMBINE_NORMAL,
    "PULSE_DISTANCE": ChannelCombineMode.COMBINE_PULSE_DISTANCE,
    "PULSE_WIDTH": ChannelCombineMode.COMBINE_PULSE_WIDTH,
}

CHANNEL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement="ps",
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS6500),
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_REFCLK_DIVISIONS, default=195313): cv.uint32_t,
            cv.Optional(CONF_HIGH_RESOLUTION, default="OFF"): cv.enum(
                HIGH_RESOLUTION_OPTIONS, upper=True
            ),
            cv.Optional(CONF_CHANNEL_COMBINE, default="NORMAL"): cv.enum(
                CHANNEL_COMBINE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_USE_CRYSTAL, default=True): cv.boolean,
            cv.Optional(CONF_CHANNEL_1): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_2): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_3): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_4): CHANNEL_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(spi.spi_device_schema(cs_pin_required=True, default_mode="MODE1"))
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "as6500", require_miso=True, require_mosi=True
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        pin = await cg.gpio_pin_expression(interrupt_pin)
        cg.add(var.set_interrupt_pin(pin))

    cg.add(var.set_refclk_divisions(config[CONF_REFCLK_DIVISIONS]))
    cg.add(var.set_high_resolution_mode(config[CONF_HIGH_RESOLUTION]))
    cg.add(var.set_channel_combine_mode(config[CONF_CHANNEL_COMBINE]))
    cg.add(var.set_use_crystal(config[CONF_USE_CRYSTAL]))

    for i, channel_key in enumerate(
        [CONF_CHANNEL_1, CONF_CHANNEL_2, CONF_CHANNEL_3, CONF_CHANNEL_4]
    ):
        if channel_key in config:
            sens = await sensor.new_sensor(config[channel_key])
            cg.add(var.set_channel_sensor(i, sens))
            cg.add(var.set_stop_enabled(i, True))
        else:
            cg.add(var.set_stop_enabled(i, False))
