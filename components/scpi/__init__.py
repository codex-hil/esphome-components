"""Bounded SCPI subset for existing ESPHome entities."""
import itertools
import re
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import sensor, binary_sensor, switch, number, button, socket
from esphome.const import CONF_ID

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["network"]
ns = cg.esphome_ns.namespace("scpi")
SCPIComponent = ns.class_("SCPIComponent", cg.Component)
ENTITY_TYPES = {"sensor": sensor.Sensor, "binary_sensor": binary_sensor.BinarySensor,
                "switch": switch.Switch, "number": number.Number, "button": button.Button}

def pattern(value):
    value = cv.string_strict(value)
    if len(value) > 96 or not re.fullmatch(r"[A-Z]+[a-z]*(?::[A-Z]+[a-z]*){0,7}", value):
        raise cv.Invalid("Use colon-separated SCPI mnemonics, uppercase short form followed by lowercase suffix; no ?")
    return value

def aliases(value):
    return {":".join(parts) for parts in itertools.product(*[
        sorted({p.upper(), ''.join(c for c in p if c.isupper())}) for p in value.split(":")])}

def idn(value):
    value = cv.string_strict(value)
    if not 1 <= len(value) <= 192 or any(ord(c) < 32 or ord(c) > 126 for c in value):
        raise cv.Invalid("idn must contain 1..192 printable ASCII characters")
    if len(value.split(',')) != 4 or any(not x.strip() for x in value.split(',')) or '"' in value:
        raise cv.Invalid("idn must have four nonempty comma-separated fields, without quotes")
    return value

MAPPING = cv.typed_schema({kind: cv.Schema({
    cv.Required("command"): pattern,
    cv.Required("entity_id"): cv.use_id(cls),
    **({cv.Optional("writable", default=True): cv.boolean} if kind in ("switch", "number") else {}),
}) for kind, cls in ENTITY_TYPES.items()}, key="type")

def validate(config):
    seen = aliases("SYSTem:ERRor") | {"*IDN"}
    for mapping in config["commands"]:
        forms = aliases(mapping["command"])
        if forms & seen:
            raise cv.Invalid("Conflicting command aliases: " + mapping["command"])
        seen |= forms
    socket.consume_sockets(config["max_clients"] + 1, "scpi")(config)
    socket.consume_sockets(1, "scpi", socket.SocketType.TCP_LISTEN)(config)
    return config

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(SCPIComponent),
    cv.Required("idn"): idn,
    cv.Optional("port", default=5025): cv.int_range(min=1, max=65535),
    cv.Optional("max_clients", default=2): cv.int_range(min=1, max=8),
    cv.Optional("max_message_length", default=256): cv.int_range(min=16, max=1024),
    cv.Optional("tx_buffer_size", default=1024): cv.int_range(min=256, max=8192),
    cv.Optional("error_queue_size", default=8): cv.int_range(min=1, max=32),
    cv.Optional("timeout_ms", default=10000): cv.int_range(min=100, max=600000),
    cv.Required("commands"): cv.All(cv.ensure_list(MAPPING), cv.Length(min=1, max=64)),
}).extend(cv.COMPONENT_SCHEMA), cv.only_on(["host", "esp32"]), validate)

def final_validate(config):
    if fv.full_config.get()["socket"]["implementation"] != "bsd_sockets":
        raise cv.Invalid("scpi requires socket implementation: bsd_sockets")
    return config

FINAL_VALIDATE_SCHEMA = final_validate

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.configure(config["idn"], config["port"], config["max_clients"],
                         config["max_message_length"], config["tx_buffer_size"],
                         config["error_queue_size"], config["timeout_ms"]))
    for mapping in config["commands"]:
        entity = await cg.get_variable(mapping["entity_id"])
        args = [mapping["command"], entity]
        if mapping["type"] in ("switch", "number"):
            args.append(mapping["writable"])
        cg.add(getattr(var, "add_" + mapping["type"])(*args))
