# SPDX-License-Identifier: MIT
"""Optional raw serprog TCP transport. Board preparation/release policy belongs to YAML."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import socket
from esphome.components.moduliq_cpld_flash import Flash
from esphome.const import CONF_ID

DEPENDENCIES = ["moduliq_cpld_flash", "network"]
AUTO_LOAD = ["socket"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("moduliq_serprog")
Serprog = ns.class_("Serprog", cg.Component)
SessionTrigger = automation.Trigger.template(cg.uint32)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(Serprog),
        cv.Required("flash_id"): cv.use_id(Flash),
        cv.Optional("port", default=6054): cv.port,
        cv.Optional("enabled", default=False): cv.boolean,
        cv.Optional("session_timeout", default="30s"): cv.All(cv.positive_time_period_milliseconds, cv.Range(min=cv.TimePeriod(milliseconds=1000), max=cv.TimePeriod(milliseconds=3600000))),
        cv.Optional("on_prepare"): automation.validate_automation({cv.GenerateID("trigger_id"): cv.declare_id(SessionTrigger)}),
        cv.Optional("on_release_requested"): automation.validate_automation({cv.GenerateID("trigger_id"): cv.declare_id(SessionTrigger)}),
        cv.Optional("on_released"): automation.validate_automation({cv.GenerateID("trigger_id"): cv.declare_id(SessionTrigger)}),
    }).extend(cv.COMPONENT_SCHEMA),
    socket.consume_sockets(2, "moduliq_serprog"),  # active client plus a briefly accepted competitor
    socket.consume_sockets(1, "moduliq_serprog", socket.SocketType.TCP_LISTEN),
)


def final_validate(config):
    import esphome.final_validate as fv
    for other in fv.full_config.get().get("moduliq_serprog", []):
        if other["id"] == config["id"]:
            continue
        if other["flash_id"] == config["flash_id"]:
            raise cv.Invalid("Only one serprog server per Flash")
        if other["port"] == config["port"]:
            raise cv.Invalid("Serprog TCP ports must be unique")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_flash(await cg.get_variable(config["flash_id"])))
    cg.add(var.set_port(config["port"]))
    cg.add(var.set_session_timeout(config["session_timeout"].total_milliseconds))
    cg.add(var.set_enabled(config["enabled"]))
    for key, getter in [("on_prepare", "get_prepare_trigger"), ("on_release_requested", "get_release_requested_trigger"), ("on_released", "get_released_trigger")]:
        for conf in config.get(key, []):
            trigger = cg.Pvariable(conf["trigger_id"], getattr(var, getter)())
            await automation.build_automation(trigger, [(cg.uint32, "session")], conf)
