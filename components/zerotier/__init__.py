"""Native ZeroTier IPv4 endpoint on ESP32-P4 / system lwIP."""
import re

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import binary_sensor, ethernet, esp32, text_sensor
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority

from .source import prepare_component

DEPENDENCIES = ["esp32", "ethernet", "psram"]
CONFLICTS_WITH = ["adc", "esp32_touch"]
AUTO_LOAD = ["binary_sensor", "text_sensor"]
ns = cg.esphome_ns.namespace("zerotier")
ZeroTierComponent = ns.class_("ZeroTierComponent", cg.Component)


def network_id(value):
    value = cv.string_strict(value).lower()
    if not re.fullmatch(r"[0-9a-f]{16}", value) or int(value, 16) == 0 or value.startswith("ff"):
        raise cv.Invalid("Expected a nonzero, 16-digit private ZeroTier Network ID")
    return value


def validate(config):
    if CORE.target_framework != "esp-idf" or esp32.get_esp32_variant() != "ESP32P4":
        raise cv.Invalid("ZeroTier currently requires ESP32-P4 with ESP-IDF")
    full = fv.full_config.get()
    if any(sensor.get("platform") == "adc" for sensor in full.get("sensor", [])):
        raise cv.Invalid("ZeroTier SAR ADC entropy conflicts with the adc sensor platform")
    psram = full.get("psram", {})
    if psram.get("disabled") or psram.get("ignore_not_found", False):
        raise cv.Invalid("ZeroTier requires working PSRAM (ignore_not_found: false)")
    return config


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(ZeroTierComponent),
    cv.Required("network_id"): network_id,
    cv.GenerateID("ethernet_id"): cv.use_id(ethernet.EthernetComponent),
    cv.Optional("storage", default="nvs"): cv.one_of("nvs"),
    cv.Optional("interface", default="ethernet"): cv.one_of("ethernet"),
    cv.Optional("entropy", default="sar_adc"): cv.one_of("sar_adc"),
    cv.Optional("node_id"): text_sensor.text_sensor_schema(),
    cv.Optional("address"): text_sensor.text_sensor_schema(),
    cv.Optional("connected"): binary_sensor.binary_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA), cv.only_on_esp32, cv.require_esphome_version(2026, 9, 0))
FINAL_VALIDATE_SCHEMA = validate


@coroutine_with_priority(-100.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    eth = await cg.get_variable(config["ethernet_id"])
    cg.add(var.set_ethernet(eth))
    cg.add(var.set_network_id(cg.RawExpression(f"0x{config['network_id']}ULL")))
    for key, setter in (("node_id", var.set_node_id_sensor), ("address", var.set_address_sensor)):
        if key in config:
            entity = await text_sensor.new_text_sensor(config[key])
            cg.add(setter(entity))
    if "connected" in config:
        entity = await binary_sensor.new_binary_sensor(config["connected"])
        cg.add(var.set_connected_sensor(entity))
    esp32.add_idf_component(name="zerotier_native", path=str(prepare_component()))
    for key, value in {
        "CONFIG_COMPILER_CXX_EXCEPTIONS": True,
        "CONFIG_COMPILER_CXX_RTTI": True,
        "CONFIG_SPIRAM_USE_CAPS_ALLOC": False,
        "CONFIG_SPIRAM_USE_MALLOC": True,
        "CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY": True,
        "CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL": 16384,
        "CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL": 32768,
        "CONFIG_LWIP_CHECK_THREAD_SAFETY": True,
        # Upstream InetAddress requires sockaddr_in6 even for IPv4-only transport.
        "CONFIG_LWIP_IPV6": True,
        "CONFIG_ESP_TASK_WDT_EN": True,
        "CONFIG_ESP_TASK_WDT_INIT": True,
    }.items():
        esp32.add_idf_sdkconfig_option(key, value)
