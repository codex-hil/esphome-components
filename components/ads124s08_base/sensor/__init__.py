"""Independent voltage entities sharing one ADS124S08 acquisition engine."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from .. import ADC, ns

DEPENDENCIES = ["ads124s08_base"]
Channel = ns.class_("ADS124S08Channel", sensor.Sensor, cg.PollingComponent)
INPUTS = {f"AIN{i}": i for i in range(12)} | {"AINCOM": 12}
MUX = {f"{p}_{n}": (pv << 4) | nv for p, pv in INPUTS.items() for n, nv in INPUTS.items()}
ROUTES = INPUTS | {"DISABLED": 15}
CURRENTS = {v: i for i, v in enumerate((0, 10, 50, 100, 250, 500, 750, 1000, 1500, 2000))}
REFERENCES = {"internal": 0x3A, "ref0": 0x32, "ref1": 0x36}


def check_channel(config):
    if config["pga"] == "bypass" and config["gain"] != 1:
        raise cv.Invalid("PGA bypass requires gain: 1")
    if config["reference"] != "internal" and "reference_voltage" not in config:
        raise cv.Invalid("External reference requires reference_voltage")
    config.setdefault("reference_voltage", 2.5)
    enabled = config["idac1"] != "DISABLED" or config["idac2"] != "DISABLED"
    if enabled != (config["idac_current_ua"] != 0):
        raise cv.Invalid("Nonzero idac_current_ua requires a routed IDAC; disabled current requires both routes DISABLED")
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(Channel, unit_of_measurement="V", accuracy_decimals=6,
                         device_class="voltage", state_class="measurement").extend({
        cv.GenerateID("ads124s08_id"): cv.use_id(ADC),
        cv.Required("multiplexer"): cv.one_of(*MUX, upper=True),
        cv.Optional("gain", default=1): cv.one_of(1, 2, 4, 8, 16, 32, 64, 128, int=True),
        cv.Optional("pga", default="bypass"): cv.one_of("bypass", "enabled", lower=True),
        cv.Optional("reference", default="internal"): cv.one_of(*REFERENCES, lower=True),
        cv.Optional("reference_voltage"): cv.All(cv.voltage, cv.Range(min=0, min_included=False, max=5.25)),
        cv.Optional("sample_rate", default=20): cv.one_of(2.5, 20, 100, 1000, 4000, float=True),
        cv.Optional("filter", default="low_latency"): cv.one_of("low_latency", "sinc3", lower=True),
        cv.Optional("settling_time", default="100ms"): cv.All(cv.positive_time_period_milliseconds,
                                                          cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(seconds=10))),
        # Both hardware current sources have one shared magnitude setting.
        cv.Optional("idac_current_ua", default=0): cv.one_of(*CURRENTS, int=True),
        cv.Optional("idac1", default="DISABLED"): cv.one_of(*ROUTES, upper=True),
        cv.Optional("idac2", default="DISABLED"): cv.one_of(*ROUTES, upper=True),
    }).extend(cv.polling_component_schema("60s")), check_channel)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await sensor.register_sensor(var, config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config["ads124s08_id"])
    cg.add(var.configure(MUX[config["multiplexer"]], config["gain"], config["pga"] == "enabled",
                         REFERENCES[config["reference"]], config["reference_voltage"],
                         CURRENTS[config["idac_current_ua"]],
                         (ROUTES[config["idac2"]] << 4) | ROUTES[config["idac1"]],
                         config["settling_time"].total_milliseconds, config["sample_rate"],
                         config["filter"] == "sinc3"))
    cg.add(parent.add_channel(var))
