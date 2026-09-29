import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, text_sensor
from esphome.components import sensor as sensor_component
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("ads124s08_base")
ADC = ns.class_("ADS124S08Base", cg.PollingComponent, spi.SPIDevice)
SCHEMA = {
    cv.GenerateID(): cv.declare_id(ADC),
    cv.Optional("conversion_mode", default="single_shot"): cv.one_of("single_shot", "continuous"),
    cv.Optional("miso_ready_pin"): pins.gpio_input_pin_schema,
    cv.Optional("samples_per_source", default=16): cv.int_range(min=2, max=64),
    cv.Optional("gain", default=1): cv.one_of(1, 2, 4, int=True),
    cv.Optional("sample_rate", default=20): cv.one_of(2.5, 20, 100, 1000, 4000, float=True),
    cv.Optional("filter", default="low_latency"): cv.one_of("low_latency", "sinc3", lower=True),
    cv.Optional("scan_offset", default=0): cv.int_range(min=0, max=3),
    cv.Required("raw"): sensor_component.sensor_schema(accuracy_decimals=0),
    cv.Required("status"): text_sensor.text_sensor_schema(),
    cv.Required("source"): text_sensor.text_sensor_schema(),
    cv.Optional("crc_errors"): sensor_component.sensor_schema(accuracy_decimals=0, state_class="total_increasing"),
    cv.Optional("short_raw"): sensor_component.sensor_schema(accuracy_decimals=0),
    cv.Optional("avdd_voltage"): sensor_component.sensor_schema(unit_of_measurement="V", accuracy_decimals=3),
    cv.Optional("dvdd_voltage"): sensor_component.sensor_schema(unit_of_measurement="V", accuracy_decimals=3),
    cv.Optional("die_temperature"): sensor_component.sensor_schema(unit_of_measurement="°C", accuracy_decimals=1),
}

def fixed_spi(config):
    if config["spi_mode"] not in ("MODE1", 1):
        raise cv.Invalid("ADS124S08 requires spi_mode: 1")
    if config["data_rate"] != 100000:
        raise cv.Invalid("This commissioned baseline requires data_rate: 100kHz")
    if config["conversion_mode"] == "continuous":
        if "miso_ready_pin" not in config:
            raise cv.Invalid("continuous requires miso_ready_pin shared with SPI MISO")
        if config["sample_rate"] not in (2.5, 20):
            raise cv.Invalid("continuous is validated only at 2.5 and 20 SPS")
    elif "miso_ready_pin" in config:
        raise cv.Invalid("miso_ready_pin currently requires continuous mode")
    return config

CONFIG_SCHEMA = cv.All(cv.Schema(SCHEMA).extend(cv.polling_component_schema("1s")).extend(
    spi.spi_device_schema(True, "100kHz", "MODE1")), fixed_spi)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_continuous(config["conversion_mode"] == "continuous"))
    cg.add(var.set_samples_per_source(config["samples_per_source"]))
    if "miso_ready_pin" in config:
        pin = await cg.gpio_pin_expression(config["miso_ready_pin"])
        cg.add(var.set_miso_ready_pin(pin))
    cg.add(var.set_gain(config["gain"]))
    cg.add(var.set_sample_rate(config["sample_rate"]))
    cg.add(var.set_sinc3(config["filter"] == "sinc3"))
    cg.add(var.set_scan_offset(config["scan_offset"]))
    for key in ("crc_errors", "raw", "short_raw", "avdd_voltage", "dvdd_voltage", "die_temperature"):
        if key in config:
            ent = await sensor_component.new_sensor(config[key])
            cg.add(getattr(var, "set_" + key)(ent))
    for key in ("status", "source"):
        ent = await text_sensor.new_text_sensor(config[key])
        cg.add(getattr(var, "set_" + key)(ent))
