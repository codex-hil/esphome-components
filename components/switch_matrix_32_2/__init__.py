from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["select"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("switch_matrix_32_2")
Matrix = ns.class_("Matrix", cg.Component, spi.SPIDevice)
SetMasksAction = ns.class_("SetMasksAction", automation.Action)
BypassAllAction = ns.class_("BypassAllAction", automation.Action)


def physical_pin(value):
    value = pins.internal_gpio_output_pin_schema(value)
    if value.get("inverted", False):
        raise cv.Invalid("Use physical pin polarity (inverted: false)")
    return value


def validate_config(config):
    if "cs_pin" in config:
        raise cv.Invalid("Use cs_a_pin and cs_b_pin instead of cs_pin")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(Matrix),
    cv.Required("cs_a_pin"): physical_pin,
    cv.Required("cs_b_pin"): physical_pin,
    cv.Required("oe_pin"): physical_pin,
    cv.Optional("startup_state", default="BYPASS"): cv.one_of("BYPASS"),
    cv.Optional("break_before_make", default="10ms"): cv.All(
        cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(milliseconds=1))),
}).extend(cv.COMPONENT_SCHEMA).extend(
    spi.spi_device_schema(cs_pin_required=False, default_mode="MODE0", default_data_rate="200kHz")
).extend({cv.Optional("spi_mode", default="MODE0"): cv.enum({"MODE0": spi.SPI_MODE_OPTIONS["MODE0"]}, upper=True)}), validate_config)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config, write_only=True)
    for key in ("cs_a_pin", "cs_b_pin", "oe_pin"):
        cg.add(getattr(var, "set_" + key)(await cg.gpio_pin_expression(config[key])))
    cg.add(var.set_break_before_make(config["break_before_make"].total_milliseconds))


@automation.register_action("switch_matrix_32_2.set_masks", SetMasksAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(Matrix),
    cv.Required("mask_a"): cv.templatable(cv.string_strict),
    cv.Required("mask_b"): cv.templatable(cv.string_strict),
}), synchronous=True)
async def masks_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
    for key in ("mask_a", "mask_b"):
        cg.add(getattr(var, "set_" + key)(await cg.templatable(config[key], args, cg.std_string)))
    return var


@automation.register_action("switch_matrix_32_2.bypass_all", BypassAllAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(Matrix)}), synchronous=True)
async def bypass_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
