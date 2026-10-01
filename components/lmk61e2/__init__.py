import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, number, select, switch, sensor, binary_sensor
from esphome.const import CONF_ID
DEPENDENCIES = ['i2c']
AUTO_LOAD = ['number', 'select', 'switch', 'sensor', 'binary_sensor']
MULTI_CONF = True
ns = cg.esphome_ns.namespace('lmk61e2')
LMK61E2 = ns.class_('LMK61E2Component', cg.PollingComponent, i2c.I2CDevice)
FrequencyNumber = ns.class_('FrequencyNumber', number.Number)
FormatSelect = ns.class_('FormatSelect', select.Select)
OutputSwitch = ns.class_('OutputSwitch', switch.Switch)
SetFrequencyAction = ns.class_('SetFrequencyAction', automation.Action)
FORMATS = {'LVPECL': 1, 'LVDS': 2, 'HCSL': 3}
MAXIMUM = {'LVPECL': 1e9, 'LVDS': 9e8, 'HCSL': 4e8}

def validate(config):
    f = config['frequency']
    if not 1e7 <= f <= MAXIMUM[config['output_format']]:
        raise cv.Invalid('Frequency must be >=10 MHz and within the output format limit')
    if not config['allow_fractional'] and not any(
        abs(100_000_000 * n / d - f) < 1e-5
        for n in range(46, 57) for d in range(5, 512)
    ):
        raise cv.Invalid('This frequency requires allow_fractional: true (experimental profile)')
    if config['address'] not in (0x58, 0x59, 0x5A):
        raise cv.Invalid('LMK61E2 address must be 0x58, 0x59 or 0x5A')
    return config

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(LMK61E2),
    cv.Optional('frequency', default='100MHz'): cv.frequency,
    cv.Optional('output_format', default='LVPECL'): cv.one_of(*FORMATS, upper=True),
    cv.Optional('output_enabled', default=True): cv.boolean,
    cv.Optional('allow_fractional', default=False): cv.boolean,
    cv.Optional('frequency_number'): number.number_schema(FrequencyNumber, unit_of_measurement='MHz', icon='mdi:sine-wave'),
    cv.Optional('format_select'): select.select_schema(FormatSelect),
    cv.Optional('output_switch'): switch.switch_schema(OutputSwitch),
    cv.Optional('actual_frequency'): sensor.sensor_schema(unit_of_measurement='MHz', accuracy_decimals=6),
    cv.Optional('pll_ok'): binary_sensor.binary_sensor_schema(),
}).extend(cv.polling_component_schema('5s')).extend(i2c.i2c_device_schema(0x58)), validate)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_initial_frequency(config['frequency']))
    cg.add(var.set_initial_format(FORMATS[config['output_format']]))
    cg.add(var.set_initial_enabled(config['output_enabled']))
    cg.add(var.set_allow_fractional(config['allow_fractional']))
    if 'frequency_number' in config:
        obj = await number.new_number(config['frequency_number'], min_value=10, max_value=1000, step=0.0001)
        cg.add(obj.set_parent(var)); cg.add(var.set_frequency_number(obj))
    if 'format_select' in config:
        obj = await select.new_select(config['format_select'], options=list(FORMATS))
        cg.add(obj.set_parent(var)); cg.add(var.set_format_select(obj))
    if 'output_switch' in config:
        obj = await switch.new_switch(config['output_switch'])
        cg.add(obj.set_parent(var)); cg.add(var.set_output_switch(obj))
    if 'actual_frequency' in config:
        obj = await sensor.new_sensor(config['actual_frequency']); cg.add(var.set_frequency_sensor(obj))
    if 'pll_ok' in config:
        obj = await binary_sensor.new_binary_sensor(config['pll_ok']); cg.add(var.set_lock_sensor(obj))

@automation.register_action('lmk61e2.set_frequency', SetFrequencyAction, cv.Schema({
    cv.Required(CONF_ID): cv.use_id(LMK61E2),
    cv.Required('frequency'): cv.templatable(cv.frequency),
}), synchronous=True)
async def set_frequency_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    value = await cg.templatable(config['frequency'], args, cg.double)
    cg.add(var.set_frequency(value))
    return var
