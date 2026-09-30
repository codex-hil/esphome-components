"""Shared target Flash transport; no implicit erase/program/target boot sequence."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, text_sensor
from esphome.components.moduliq_cpld_gpio import GPIO, addressed_bus, cs_number, declared
from esphome.const import CONF_ID

DEPENDENCIES = ["spi", "addrspi", "moduliq_cpld_gpio"]
AUTO_LOAD = ["text_sensor"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("moduliq_cpld_flash")
Flash = ns.class_("CPLDFlash", cg.Component, spi.SPIDevice)

def explicit_transport(config):
    if "spi_mode" not in config or "data_rate" not in config:
        raise cv.Invalid("Flash requires explicit spi_mode and data_rate from the exact memory profile")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(Flash),
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Required("gpio_id"): cv.use_id(GPIO),
    cv.Required("target_profile"): cv.All(cv.string_strict, cv.Length(min=1)),
    cv.Optional("status"): text_sensor.text_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema(True)), explicit_transport)


def final_validate(config):
    import esphome.final_validate as fv
    full = fv.full_config.get()
    gpio = declared(config["gpio_id"])
    parent = addressed_bus(gpio, 3)
    flash_bus = addressed_bus(config, 2)
    if parent["id"] != flash_bus["id"] or cs_number(config) != cs_number(gpio):
        raise cv.Invalid("Flash CS2 and GPIO CS3 must share addrspi mux and host CS")
    for other in full.get("moduliq_cpld_flash", []):
        if other["id"] != config["id"] and other["gpio_id"] == config["gpio_id"]:
            raise cv.Invalid("Only one Flash owner per CPLD module")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_enabled(config["enabled"]))
    cg.add(var.set_gpio(await cg.get_variable(config["gpio_id"])))
    cg.add(var.set_target_profile(config["target_profile"]))
    if "status" in config:
        cg.add(var.set_status(await text_sensor.new_text_sensor(config["status"])))
