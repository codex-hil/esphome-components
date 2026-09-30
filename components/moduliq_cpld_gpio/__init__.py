"""CS3 GPIO and the single shared CFG/upper-bank owner for a CPLD module."""
from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import spi
from esphome.const import CONF_ID, CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN, CONF_MODE, CONF_NUMBER, CONF_INVERTED

DEPENDENCIES = ["spi", "addrspi"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("moduliq_cpld_gpio")
GPIO = ns.class_("CPLDGPIO", cg.Component, spi.SPIDevice)
Pin = ns.class_("CPLDGPIOPin", cg.GPIOPin)


def profile(config):
    if config["board_profile"] == "rtd16_rev10":
        config.setdefault("upper_input_mask", 0xFF)
        if config["upper_input_mask"] & 0x88 != 0x88:
            raise cv.Invalid("RTD16 REG_IN3/7 are DRDY and must remain inputs (mask 0x88)")
    elif "upper_input_mask" not in config:
        raise cv.Invalid("generic board_profile requires explicit upper_input_mask for external drivers/ID straps")
    if config["spi_mode"] != "MODE0":
        raise cv.Invalid("CPLD GPIO requires SPI mode0")
    if config["data_rate"] > 100000:
        raise cv.Invalid("CPLD GPIO software baseline is limited to 100kHz; faster timing is unvalidated")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(GPIO),
    cv.Required("module_address"): cv.int_range(min=0, max=15),
    cv.Required("board_profile"): cv.one_of("generic", "rtd16_rev10"),
    cv.Optional("upper_input_mask"): cv.hex_uint8_t,
    cv.Optional("lower_input_mask", default=0): cv.hex_uint8_t,
}).extend(cv.COMPONENT_SCHEMA).extend(spi.spi_device_schema(True, "100kHz", "MODE0")), profile)


def declared(config_id):
    full = fv.full_config.get()
    return full.get_config_for_path(full.get_path_for_id(config_id)[:-1])


def addressed_bus(config, channel):
    full = fv.full_config.get()
    for mux in full.get("addrspi", []):
        for entry in mux["channels"]:
            if entry["bus_id"] == config["spi_id"]:
                if entry["channel"] != channel:
                    raise cv.Invalid(f"CPLD device requires addrspi channel {channel}")
                bus_id = mux["spi_id"]
                seen = set()
                while True:
                    if str(bus_id) in seen:
                        raise cv.Invalid("Cyclic addressed SPI parent")
                    seen.add(str(bus_id))
                    parents = [parent for parent in full.get("addrspi", [])
                               if any(child["bus_id"] == bus_id for child in parent["channels"])]
                    if not parents:
                        bus = declared(bus_id)
                        break
                    bus_id = parents[0]["spi_id"]
                if "mosi_pin" not in bus or "miso_pin" not in bus:
                    raise cv.Invalid("CPLD SPI transport requires MOSI and MISO on the physical bus")
                return mux
    raise cv.Invalid("CPLD SPI device must use an addrspi channel")


def cs_number(config):
    return config["cs_pin"]["number"]


def walk(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def final_validate(config):
    mux = addressed_bus(config, 3)
    full = fv.full_config.get()
    for other in full.get("moduliq_cpld_gpio", []):
        if other["id"] != config["id"] and other["spi_id"] == config["spi_id"]:
            raise cv.Invalid("Only one GPIO/CFG owner is allowed per addressed CS3 bus")
    for reader in full.get("moduliq_cpld_i2c", []):
        if reader["module_address"] == config["module_address"]:
            if reader.get("gpio_id") != config["id"]:
                raise cv.Invalid("Readout and GPIO of this module must be linked with gpio_id")
            if reader["board_profile"] != config["board_profile"]:
                raise cv.Invalid("Readout/GPIO board profiles must match")
    if any(flash["gpio_id"] == config["id"] for flash in full.get("moduliq_cpld_flash", [])):
        for pin in walk(full):
            if pin.get("moduliq_cpld_gpio") == config["id"] and pin.get("number") in (1, 2, 3, 4):
                raise cv.Invalid("OUT1..4 are reserved by the Flash pinmux; use explicit target control API")
    # With nested addrspi, the outer channel is GA and the inner channel is CS3.
    for module_mux in full.get("addrspi", []):
        for entry in module_mux["channels"]:
            if entry["bus_id"] == mux["spi_id"]:
                if entry["channel"] != config["module_address"] or len(module_mux["address_pins"]) != 4:
                    raise cv.Invalid("Outer addrspi must select module_address (GA) using four address pins")
    for adc in full.get("ads124s08_base", []):
        for entry in mux["channels"]:
            if entry["bus_id"] == adc["spi_id"] and entry["channel"] not in (0, 1):
                raise cv.Invalid("ADS124S08 on this bridge must remain on CS0/CS1")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    cg.add_define("USE_MODULIQ_CPLD_GPIO")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    cg.add(var.set_input_masks(config["lower_input_mask"], config["upper_input_mask"]))


def validate_mode(mode):
    if mode[CONF_INPUT] == mode[CONF_OUTPUT]:
        raise cv.Invalid("Choose exactly one of input/output")
    if mode[CONF_OPEN_DRAIN] and not mode[CONF_OUTPUT]:
        raise cv.Invalid("open_drain requires output")
    return mode


PIN_SCHEMA = pins.gpio_base_schema(Pin, cv.int_range(min=0, max=15),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN], mode_validator=validate_mode).extend({
        cv.Required("moduliq_cpld_gpio"): cv.use_id(GPIO),
    })


def pin_final_validate(pin, parent):
    number = pin[CONF_NUMBER]
    mask = parent["upper_input_mask" if number >= 8 else "lower_input_mask"]
    if pin[CONF_MODE][CONF_OUTPUT] and mask & (1 << (number % 8)):
        raise cv.Invalid("This pin is an externally driven input in the board profile")



@pins.PIN_SCHEMA_REGISTRY.register("moduliq_cpld_gpio", PIN_SCHEMA, pin_final_validate)
async def pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_parent(await cg.get_variable(config["moduliq_cpld_gpio"])))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
