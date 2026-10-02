"""Independent I2CBus instances on reserved CPLD open-drain pad pairs."""
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import i2c
from esphome.components.moduliq_cpld_gpio import GPIO, declared, walk
from esphome.const import CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["moduliq_cpld_gpio"]
AUTO_LOAD = ["i2c"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("moduliq_cpld_soft_i2c")
Bus = ns.class_("CPLDSoftI2C", cg.Component, i2c.I2CBus)


def validate(config):
    if config["sda"] == config["scl"] or config["sda"] // 8 != config["scl"] // 8:
        raise cv.Invalid("SDA and SCL must be distinct pins in the same bank")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(Bus),
    cv.Required("gpio_id"): cv.use_id(GPIO),
    cv.Required("sda"): cv.int_range(min=0, max=15),
    cv.Required("scl"): cv.int_range(min=0, max=15),
    cv.Optional("frequency", default="1kHz"): cv.All(cv.frequency, cv.Range(min=100, max=1000)),
    cv.Optional("stretch_timeout", default="10ms"): cv.All(cv.positive_time_period_microseconds,
        cv.Range(min=cv.TimePeriod(microseconds=100), max=cv.TimePeriod(milliseconds=100))),
    cv.Optional("scan", default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA), validate)


def final_validate(config):
    full = fv.full_config.get()
    parent = declared(config["gpio_id"])
    pair = {config["sda"], config["scl"]}
    for other in full.get("moduliq_cpld_soft_i2c", []):
        if other["id"] != config["id"] and other["gpio_id"] == config["gpio_id"] and pair.intersection({other["sda"], other["scl"]}):
            raise cv.Invalid("Soft I2C pin pairs must not overlap")
    for pin in pair:
        mask = parent["upper_input_mask" if pin >= 8 else "lower_input_mask"]
        if mask & (1 << (pin % 8)):
            raise cv.Invalid("Soft I2C pins cannot be protected inputs")
        if pin in (1, 2, 3, 4) and any(f["gpio_id"] == config["gpio_id"] for f in full.get("moduliq_cpld_flash", [])):
            raise cv.Invalid("Soft I2C pin is reserved by Flash pinmux")
        for item in walk(full):
            if item.get("moduliq_cpld_gpio") == config["gpio_id"] and item.get("number") == pin:
                raise cv.Invalid("Soft I2C pin is also configured as ordinary GPIO")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_I2C")
    parent = await cg.get_variable(config["gpio_id"])
    cg.add(var.set_parent(parent))
    # Address pins -> CPLD GPIO -> software bus -> ordinary IO expanders.
    cg.set_setup_priority(parent, 920.0)
    parent_config = next(c for c in CORE.config["moduliq_cpld_gpio"] if c["id"] == config["gpio_id"])
    bus_id = parent_config["spi_id"]
    for _ in range(len(CORE.config["addrspi"])):
        mux = next((m for m in CORE.config["addrspi"] if any(ch["bus_id"] == bus_id for ch in m["channels"])), None)
        if mux is None:
            break
        cg.set_setup_priority(await cg.get_variable(mux[CONF_ID]), 930.0)
        bus_id = mux["spi_id"]
    cg.add(var.set_pins(config["sda"], config["scl"]))
    cg.add(var.set_frequency(int(config["frequency"])))
    cg.add(var.set_stretch_timeout(int(config["stretch_timeout"].total_microseconds)))
    cg.add(var.set_scan(config["scan"]))
