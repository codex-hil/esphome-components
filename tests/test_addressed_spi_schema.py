"""Validate the real nested YAML and reject invalid routing/scaling combinations."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import yaml

ROOT=Path(__file__).resolve().parents[1]

class SchemaTests(unittest.TestCase):
    def check(self, config, expected=True):
        with tempfile.NamedTemporaryFile(mode="w",suffix=".yaml",dir=ROOT/'examples') as f:
            yaml.safe_dump(config,f);f.flush()
            p=subprocess.run([sys.executable,'-m','esphome','config',f.name],capture_output=True,text=True)
        self.assertEqual(p.returncode==0,expected,(p.stdout+p.stderr)[-2500:])

    def test_both_addressing_layers_and_parallel_only(self):
        for fixture in ['mixed-signal-head','mixed-signal-parallel-only']:
            self.check(yaml.safe_load((ROOT/'examples'/f'{fixture}.yaml').read_text()))

    def test_wrong_modes_rate_and_software_bus(self):
        base=yaml.safe_load((ROOT/'examples/mixed-signal-head.yaml').read_text())
        for mutate in [lambda c:c['dacx0504'].update(spi_mode='MODE1'),
                       lambda c:c['dacx0504'].update(data_rate='1MHz'),
                       lambda c:c['spi'].update(interface='software'),
                       lambda c:c['addrspi2']['devices'][1].update(address=0),
                       lambda c:c['addrspi2'].update(spi_id='head_adc_z')]:
            config=copy.deepcopy(base);mutate(config);self.check(config,False)

    def test_shared_handle_rate_validation(self):
        base=yaml.safe_load((ROOT/'examples/mixed-signal-head.yaml').read_text())
        base['addrspi2']['shared_device']=True
        self.check(base)
        base['dacx0504']['data_rate']='200kHz'
        self.check(base,False)

    def test_voltage_and_gpio_validation(self):
        base=yaml.safe_load((ROOT/'examples/mixed-signal-head.yaml').read_text())
        for mutate in [lambda c:c['dacx0504'].pop('reference_voltage'),
                       lambda c:c['output'][0].update(min_voltage='4V',max_voltage='3V'),
                       lambda c:c['binary_sensor'][0].update(spi_shift_register_id='gpio_out'),
                       lambda c:c['output'][-1].update(spi_shift_register_id='gpio_in'),
                       lambda c:c['mcp3208'][2].update(sample_rate='0Hz')]:
            config=copy.deepcopy(base);mutate(config);self.check(config,False)

if __name__=='__main__':unittest.main()
