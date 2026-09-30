"""Independent I2C readout; ADC capabilities and ID decoding are explicit."""
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import i2c, sensor, text_sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor"]
MULTI_CONF = True
ns = cg.esphome_ns.namespace("moduliq_cpld_i2c")
Readout = ns.class_("CPLDReadout", cg.PollingComponent, i2c.I2CDevice)
# A symbolic reference avoids importing/loading SPI/GPIO for standalone I2C readout.
GPIO = cg.esphome_ns.namespace("moduliq_cpld_gpio").class_("CPLDGPIO", cg.Component)


def declared(config_id):
    full = fv.full_config.get()
    return full.get_config_for_path(full.get_path_for_id(config_id)[:-1])
NUMERIC = ("project_id", "revision_id", "power_request", "errin", "upper_gpio", "lower_gpio", "fault_mask", "fault_status")
DIGITAL = cv.Schema({
    cv.Required("bank"): cv.one_of("upper", "lower"),
    cv.Required("mask"): cv.All(cv.hex_uint8_t, cv.int_range(min=1, max=255)),
    cv.Optional("shift", default=0): cv.int_range(min=0, max=7),
    cv.Optional("xor_mask", default=0): cv.hex_uint8_t,
    cv.Required("codes"): cv.Schema({cv.int_range(min=0, max=255): cv.string_strict}),
    cv.Required("sensor"): text_sensor.text_sensor_schema(),
})
BAND = cv.Schema({
    cv.Required("min"): cv.int_range(min=0, max=1023),
    cv.Required("max"): cv.int_range(min=0, max=1023),
    cv.Required("id"): cv.string_strict,
})
ADC_CHANNEL = cv.Schema({
    cv.Required("channel"): cv.int_range(min=0, max=7),
    cv.Optional("raw"): sensor.sensor_schema(accuracy_decimals=0),
    cv.Optional("id"): text_sensor.text_sensor_schema(),
    cv.Optional("bands", default=[]): cv.ensure_list(BAND),
})
ADC = cv.Schema({
    cv.Optional("enabled", default=False): cv.boolean,
    cv.Required("bits"): cv.one_of(8, 9, 10, int=True),
    cv.Optional("timeout", default="500ms"): cv.positive_time_period_milliseconds,
    cv.Optional("channels", default=[]): cv.ensure_list(ADC_CHANNEL),
})
SCHEMA = {
    cv.GenerateID(): cv.declare_id(Readout),
    cv.Required("module_address"): cv.int_range(min=0, max=15),
    cv.Optional("gpio_id"): cv.use_id(GPIO),
    cv.Optional("status"): text_sensor.text_sensor_schema(),
    cv.Optional("digital_ids", default=[]): cv.ensure_list(DIGITAL),
    cv.Optional("error_counters", default=[]): cv.ensure_list(cv.Schema({
        cv.Required("channel"): cv.int_range(min=0, max=3),
        cv.Required("sensor"): sensor.sensor_schema(accuracy_decimals=0),
    })),
    cv.Optional("adc"): ADC,
}
SCHEMA.update({cv.Optional(key): sensor.sensor_schema(accuracy_decimals=0) for key in NUMERIC})


def validate(config):
    expected = 0x50 + config["module_address"]
    if "address" in config and config["address"] != expected:
        raise cv.Invalid("I2C address must equal 0x50 + module_address (GA)")
    config["address"] = expected
    for field in config["digital_ids"]:
        mask, shift = field["mask"], field["shift"]
        if mask & ((1 << shift) - 1) or (mask >> shift) == 0:
            raise cv.Invalid("Digital ID mask/shift discard selected bits")
        if field["xor_mask"] & ~mask:
            raise cv.Invalid("xor_mask must be inside the digital ID mask")
        if any(code & ~(mask >> shift) for code in field["codes"]):
            raise cv.Invalid("Digital ID code is outside mask/shift")
    counters = [item["channel"] for item in config["error_counters"]]
    if len(counters) != len(set(counters)):
        raise cv.Invalid("Each destructive error counter has one consumer")
    if "adc" in config:
        adc = config["adc"]
        if adc["timeout"].total_milliseconds < {8: 40, 9: 90, 10: 220}[adc["bits"]]:
            raise cv.Invalid("ADC timeout is shorter than the default HDL sweep plus margin")
        channels = [item["channel"] for item in adc["channels"]]
        if len(channels) != len(set(channels)):
            raise cv.Invalid("Duplicate ADC channel")
        for channel in adc["channels"]:
            if not ("raw" in channel or "id" in channel):
                raise cv.Invalid("ADC channel requires raw and/or id sensor")
            if channel["bands"] and "id" not in channel:
                raise cv.Invalid("ADC bands require an id sensor")
            ordered = sorted(channel["bands"], key=lambda band: band["min"])
            for index, band in enumerate(ordered):
                if band["min"] > band["max"] or band["max"] >= (1 << adc["bits"]):
                    raise cv.Invalid("ADC ID band outside configured resolution")
                if index and ordered[index - 1]["max"] >= band["min"]:
                    raise cv.Invalid("ADC ID bands overlap")
    return config


def add_address(config):
    config = dict(config)
    if "module_address" in config:
        ga = cv.int_range(min=0, max=15)(config["module_address"])
        config.setdefault("address", 0x50 + ga)
    return config


CONFIG_SCHEMA = cv.All(add_address, cv.Schema(SCHEMA).extend(cv.polling_component_schema("1s")).extend(
    i2c.i2c_device_schema(None)), validate)


def final_validate(config):
    full = fv.full_config.get()
    for other in full.get("moduliq_cpld_i2c", []):
        if other["id"] != config["id"] and other["i2c_id"] == config["i2c_id"] and other["address"] == config["address"]:
            raise cv.Invalid("One readout/ADC/counter owner per I2C module")
    if "gpio_id" in config:
        parent = declared(config["gpio_id"])
        if parent["module_address"] != config["module_address"]:
            raise cv.Invalid("gpio_id must refer to the same module_address (GA)")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if "gpio_id" in config:
        cg.add(var.set_gpio(await cg.get_variable(config["gpio_id"])))
    for index, key in enumerate(NUMERIC):
        if key in config:
            cg.add(var.set_numeric(index, await sensor.new_sensor(config[key])))
    if "status" in config:
        cg.add(var.set_status(await text_sensor.new_text_sensor(config["status"])))
    for item in config["error_counters"]:
        cg.add(var.set_counter(item["channel"], await sensor.new_sensor(item["sensor"])))
    for index, item in enumerate(config["digital_ids"]):
        cg.add(var.add_digital_id(item["bank"] == "upper", item["mask"], item["shift"], item["xor_mask"],
                                await text_sensor.new_text_sensor(item["sensor"])))
        for code, label in item["codes"].items():
            cg.add(var.add_digital_code(index, code, label))
    if "adc" in config:
        adc = config["adc"]
        cg.add(var.configure_adc(adc["bits"], adc["timeout"].total_milliseconds))
        cg.add(var.set_adc_enabled(adc["enabled"]))
        for item in adc["channels"]:
            channel = item["channel"]
            if "raw" in item:
                cg.add(var.set_adc_raw(channel, await sensor.new_sensor(item["raw"])))
            if "id" in item:
                cg.add(var.set_adc_id(channel, await text_sensor.new_text_sensor(item["id"])))
            for band in item["bands"]:
                cg.add(var.add_adc_band(channel, band["min"], band["max"], band["id"]))
