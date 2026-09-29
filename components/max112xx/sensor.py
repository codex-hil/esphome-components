import esphome.codegen as cg
from esphome.components import sensor, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_LINE_FREQUENCY,
    CONF_REFERENCE_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@wizath"]
DEPENDENCIES = ["spi"]

CONF_RATE = "rate"
CONF_BIPOLAR = "bipolar"
CONF_SIGNAL_BUFFER = "signal_buffer"
CONF_REFERENCE_BUFFER = "reference_buffer"
CONF_SELF_CALIBRATE = "self_calibrate"

max112xx_ns = cg.esphome_ns.namespace("max112xx")
MAX112xxSensor = max112xx_ns.class_(
    "MAX112xxSensor", sensor.Sensor, cg.PollingComponent, spi.SPIDevice
)

Max112xxGain = max112xx_ns.enum("Max112xxGain")
GAIN_OPTIONS = {
    1: Max112xxGain.MAX112XX_GAIN_1,
    2: Max112xxGain.MAX112XX_GAIN_2,
    4: Max112xxGain.MAX112XX_GAIN_4,
    8: Max112xxGain.MAX112XX_GAIN_8,
    16: Max112xxGain.MAX112XX_GAIN_16,
    32: Max112xxGain.MAX112XX_GAIN_32,
    64: Max112xxGain.MAX112XX_GAIN_64,
    128: Max112xxGain.MAX112XX_GAIN_128,
}

Max112xxRate = max112xx_ns.enum("Max112xxRate")
RATE_OPTIONS = {
    "1SPS": Max112xxRate.MAX112XX_RATE_1SPS,
    "2.5SPS": Max112xxRate.MAX112XX_RATE_2_5SPS,
    "5SPS": Max112xxRate.MAX112XX_RATE_5SPS,
    "10SPS": Max112xxRate.MAX112XX_RATE_10SPS,
    "15SPS": Max112xxRate.MAX112XX_RATE_15SPS,
    "30SPS": Max112xxRate.MAX112XX_RATE_30SPS,
    "60SPS": Max112xxRate.MAX112XX_RATE_60SPS,
    "120SPS": Max112xxRate.MAX112XX_RATE_120SPS,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MAX112xxSensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(CONF_GAIN, default=1): cv.enum(GAIN_OPTIONS, int=True),
            cv.Optional(CONF_RATE, default="10SPS"): cv.enum(RATE_OPTIONS, upper=True),
            cv.Optional(CONF_LINE_FREQUENCY, default="50Hz"): cv.All(
                cv.frequency, cv.one_of(50, 60, int=True)
            ),
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="2.5V"): cv.voltage,
            cv.Optional(CONF_BIPOLAR, default=True): cv.boolean,
            cv.Optional(CONF_SIGNAL_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_REFERENCE_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_SELF_CALIBRATE, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_rate(config[CONF_RATE]))
    cg.add(var.set_line_frequency_50hz(config[CONF_LINE_FREQUENCY] == 50))
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
    cg.add(var.set_bipolar(config[CONF_BIPOLAR]))
    cg.add(var.set_enable_sig_buf(config[CONF_SIGNAL_BUFFER]))
    cg.add(var.set_enable_ref_buf(config[CONF_REFERENCE_BUFFER]))
    cg.add(var.set_self_calibrate(config[CONF_SELF_CALIBRATE]))
