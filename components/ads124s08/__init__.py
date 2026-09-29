from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, spi, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNELS,
    CONF_ID,
    CONF_MODEL,
    CONF_RAW,
    CONF_REFERENCE_RESISTANCE,
    CONF_RTD_NOMINAL_RESISTANCE,
    CONF_RTD_WIRES,
    CONF_STATUS,
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_OHM,
)

CODEOWNERS = ["@wizath"]
DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True

CONF_AIN_N = "ain_n"
CONF_AIN_P = "ain_p"
CONF_ADC_DATA_RATE = "adc_data_rate"
CONF_BETA = "beta"
CONF_CONVERSION_TIME = "conversion_time"
CONF_DRDY_PIN = "drdy_pin"
CONF_IDAC_CURRENT = "idac_current"
CONF_IDAC_PIN = "idac_pin"
CONF_NOMINAL_RESISTANCE = "nominal_resistance"
CONF_PGA_GAIN = "pga_gain"
CONF_REFERENCE = "reference"
CONF_REFERENCE_TEMPERATURE = "reference_temperature"
CONF_RESISTANCE = "resistance"
CONF_START_SYNC_PIN = "start_sync_pin"

TYPE_NTC = "ntc"
TYPE_PT100 = "pt100"
TYPE_PT1000 = "pt1000"

MODEL_ADS124S06 = "ADS124S06"
MODEL_ADS124S08 = "ADS124S08"

ads124s08_ns = cg.esphome_ns.namespace("ads124s08")
ADS124S08Component = ads124s08_ns.class_(
    "ADS124S08Component", cg.PollingComponent, spi.SPIDevice
)

ADS124S08Reference = ads124s08_ns.enum("ADS124S08Reference")
REFERENCE = {
    "REF0": ADS124S08Reference.ADS124S08_REF0,
    "REF1": ADS124S08Reference.ADS124S08_REF1,
    "INTERNAL": ADS124S08Reference.ADS124S08_INTERNAL,
}

ADS124S08Model = ads124s08_ns.enum("ADS124S08Model")
MODEL = {
    MODEL_ADS124S06: ADS124S08Model.ADS124S06,
    MODEL_ADS124S08: ADS124S08Model.ADS124S08,
}

ADS124S08DataRate = ads124s08_ns.enum("ADS124S08DataRate")
DATA_RATE = {
    "2.5SPS": ADS124S08DataRate.ADS124S08_RATE_2_5SPS,
    "5SPS": ADS124S08DataRate.ADS124S08_RATE_5SPS,
    "10SPS": ADS124S08DataRate.ADS124S08_RATE_10SPS,
    "16.6SPS": ADS124S08DataRate.ADS124S08_RATE_16_6SPS,
    "20SPS": ADS124S08DataRate.ADS124S08_RATE_20SPS,
    "50SPS": ADS124S08DataRate.ADS124S08_RATE_50SPS,
    "60SPS": ADS124S08DataRate.ADS124S08_RATE_60SPS,
    "100SPS": ADS124S08DataRate.ADS124S08_RATE_100SPS,
    "200SPS": ADS124S08DataRate.ADS124S08_RATE_200SPS,
    "400SPS": ADS124S08DataRate.ADS124S08_RATE_400SPS,
    "800SPS": ADS124S08DataRate.ADS124S08_RATE_800SPS,
    "1000SPS": ADS124S08DataRate.ADS124S08_RATE_1000SPS,
    "2000SPS": ADS124S08DataRate.ADS124S08_RATE_2000SPS,
    "4000SPS": ADS124S08DataRate.ADS124S08_RATE_4000SPS,
}

GAIN = {
    1: 0,
    2: 1,
    4: 2,
    8: 3,
    16: 4,
    32: 5,
    64: 6,
    128: 7,
}


def validate_ain(value):
    if isinstance(value, str) and value.upper() == "AINCOM":
        return 12
    return cv.int_range(min=0, max=12)(value)


def validate_idac_current(value):
    current = cv.current(value)
    allowed = (10e-6, 50e-6, 100e-6, 250e-6, 500e-6, 750e-6, 1000e-6, 1500e-6, 2000e-6)
    if not any(abs(current - allowed_current) < 1e-9 for allowed_current in allowed):
        raise cv.Invalid(
            "idac_current must be one of 10uA, 50uA, 100uA, 250uA, 500uA, "
            "750uA, 1000uA, 1500uA, or 2000uA"
        )
    return current


def validate_channel(config):
    if config[CONF_TYPE] in (TYPE_PT100, TYPE_PT1000):
        if CONF_REFERENCE_RESISTANCE not in config:
            raise cv.Invalid("RTD channels require reference_resistance")
        if CONF_IDAC_PIN not in config:
            raise cv.Invalid("RTD channels require idac_pin")
        if CONF_IDAC_CURRENT not in config:
            raise cv.Invalid("RTD channels require idac_current")
    return config


def validate_config(config):
    if config[CONF_MODEL] == MODEL_ADS124S06:
        for channel in config[CONF_CHANNELS]:
            for key in (CONF_AIN_P, CONF_AIN_N):
                if channel[key] not in (*range(6), 12):
                    raise cv.Invalid(
                        f"{config[CONF_MODEL]} only supports AIN0-AIN5 and AINCOM for {key}"
                    )
            if CONF_IDAC_PIN in channel and channel[CONF_IDAC_PIN] not in (
                *range(8),
                12,
            ):
                raise cv.Invalid(
                    f"{config[CONF_MODEL]} IDAC routing only supports AIN0-AIN5, "
                    "REFP1, REFN1, and AINCOM"
                )
    return config


SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CHANNEL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_TYPE): cv.one_of(
                TYPE_PT100, TYPE_PT1000, TYPE_NTC, lower=True
            ),
            cv.Required(CONF_TEMPERATURE): SENSOR_SCHEMA,
            cv.Required(CONF_AIN_P): validate_ain,
            cv.Required(CONF_AIN_N): validate_ain,
            cv.Optional(CONF_REFERENCE_RESISTANCE): cv.resistance,
            cv.Optional(CONF_NOMINAL_RESISTANCE): cv.resistance,
            cv.Optional(CONF_RTD_NOMINAL_RESISTANCE): cv.resistance,
            cv.Optional(CONF_RTD_WIRES, default=4): cv.one_of(2, 4, int=True),
            cv.Optional(CONF_IDAC_PIN): validate_ain,
            cv.Optional(CONF_IDAC_CURRENT): validate_idac_current,
            cv.Optional(CONF_BETA, default=3950.0): cv.positive_float,
            cv.Optional(CONF_REFERENCE_TEMPERATURE, default=25.0): cv.float_,
            cv.Optional(CONF_RESISTANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_OHM,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_RAW): sensor.sensor_schema(
                unit_of_measurement=UNIT_EMPTY,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(),
        }
    ),
    validate_channel,
)

BASE_CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ADS124S08Component),
            cv.Optional(CONF_MODEL, default=MODEL_ADS124S08): cv.one_of(
                MODEL_ADS124S06, MODEL_ADS124S08, upper=True, space=""
            ),
            cv.Optional(CONF_REFERENCE, default="REF0"): cv.enum(
                REFERENCE, upper=True, space=""
            ),
            cv.Optional(CONF_PGA_GAIN, default=1): cv.enum(GAIN, int=True),
            cv.Optional(CONF_ADC_DATA_RATE, default="20SPS"): cv.enum(
                DATA_RATE, upper=True, space=""
            ),
            cv.Optional(
                CONF_CONVERSION_TIME, default="120ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DRDY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_START_SYNC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CHANNELS): cv.ensure_list(CHANNEL_SCHEMA),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)

CONFIG_SCHEMA = cv.All(BASE_CONFIG_SCHEMA, validate_config)


async def build_optional_sensor(config, key):
    if key not in config:
        return cg.nullptr
    return await sensor.new_sensor(config[key])


async def build_optional_text_sensor(config, key):
    if key not in config:
        return cg.nullptr
    return await text_sensor.new_text_sensor(config[key])


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    cg.add(var.set_model(MODEL[config[CONF_MODEL]]))
    cg.add(var.set_reference(config[CONF_REFERENCE]))
    cg.add(var.set_pga_gain(config[CONF_PGA_GAIN]))
    cg.add(var.set_data_rate(config[CONF_ADC_DATA_RATE]))
    cg.add(var.set_conversion_time_ms(config[CONF_CONVERSION_TIME].total_milliseconds))

    if CONF_DRDY_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_DRDY_PIN])
        cg.add(var.set_drdy_pin(pin))

    if CONF_START_SYNC_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_START_SYNC_PIN])
        cg.add(var.set_start_sync_pin(pin))

    for conf in config[CONF_CHANNELS]:
        temp = await sensor.new_sensor(conf[CONF_TEMPERATURE])
        resistance = await build_optional_sensor(conf, CONF_RESISTANCE)
        raw = await build_optional_sensor(conf, CONF_RAW)
        status = await build_optional_text_sensor(conf, CONF_STATUS)

        if conf[CONF_TYPE] in (TYPE_PT100, TYPE_PT1000):
            nominal = conf.get(
                CONF_RTD_NOMINAL_RESISTANCE,
                100.0 if conf[CONF_TYPE] == TYPE_PT100 else 1000.0,
            )
            cg.add(
                var.add_rtd_channel(
                    conf[CONF_AIN_P],
                    conf[CONF_AIN_N],
                    conf[CONF_IDAC_PIN],
                    conf[CONF_IDAC_CURRENT],
                    conf[CONF_REFERENCE_RESISTANCE],
                    nominal,
                    conf[CONF_RTD_WIRES],
                    temp,
                    resistance,
                    raw,
                    status,
                )
            )
        else:
            nominal = conf.get(CONF_NOMINAL_RESISTANCE, 10000.0)
            reference = conf.get(CONF_REFERENCE_RESISTANCE, 10000.0)
            cg.add(
                var.add_ntc_channel(
                    conf[CONF_AIN_P],
                    conf[CONF_AIN_N],
                    reference,
                    nominal,
                    conf[CONF_BETA],
                    conf[CONF_REFERENCE_TEMPERATURE],
                    temp,
                    resistance,
                    raw,
                    status,
                )
            )
