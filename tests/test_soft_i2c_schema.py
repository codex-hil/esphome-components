import copy
import subprocess
import sys
from pathlib import Path
import tempfile
import unittest
import yaml
import test_cpld_schema as cpld_tests
ROOT = cpld_tests.ROOT

class SoftI2CSchemaTests(unittest.TestCase):
    check = cpld_tests.FullYAMLTests.check
    tearDownClass = cpld_tests.FullYAMLTests.__dict__["tearDownClass"]
    @classmethod
    def setUpClass(cls):
        cls.base = yaml.safe_load((ROOT/'examples/cpld-soft-i2c.yaml').read_text())
        cls.tmp = tempfile.TemporaryDirectory()
    def test_four_buses_normal_devices(self): self.check(self.base)
    def test_codegen_and_soft_only(self):
        for native in (True, False):
            c=copy.deepcopy(self.base)
            if not native:
                c.pop('i2c'); c.pop('moduliq_cpld_i2c')
                c['external_components'][0]['components'].remove('moduliq_cpld_i2c')
            c['external_components'][0]['source']['path']=str(ROOT/'components')
            c['esphome']['build_path']=str(Path(self.tmp.name)/('native' if native else 'soft'))
            path=Path(self.tmp.name)/'generate.yaml'; path.write_text(yaml.safe_dump(c))
            result=subprocess.run([sys.executable,'-m','esphome','compile','--only-generate',str(path)],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            build=Path(c['esphome']['build_path'])
            self.assertIn('USE_SETUP_PRIORITY_OVERRIDE',(build/'src/esphome/core/defines.h').read_text())
            source=(build/'src/main.cpp').read_text()
            self.assertIn('bridge_gpio->set_setup_priority(920.0f)',source)
            for i in range(4): self.assertIn(f'head_io_{i}->set_i2c_bus(head_bus_{i})',source)
    def test_invalid_pairs(self):
        for sda,scl in [(8,8),(7,8),(8,16)]:
            c=copy.deepcopy(self.base); c['moduliq_cpld_soft_i2c'][0].update(sda=sda,scl=scl)
            self.check(c,'moduliq_cpld_soft_i2c')
    def test_lower_bank(self):
        c=copy.deepcopy(self.base)
        for bus in c['moduliq_cpld_soft_i2c']:
            bus['sda']-=8; bus['scl']-=8
        self.check(c)
    def test_flash_pin_conflict(self):
        c=yaml.safe_load((ROOT/'examples/cpld-adc-flash.yaml').read_text())
        c['external_components'][0]['components'].append('moduliq_cpld_soft_i2c')
        c['moduliq_cpld_soft_i2c']=[dict(id='bad_bus',gpio_id='bridge_gpio',sda=1,scl=2)]
        self.check(c,'Flash pinmux')
    def test_overlap(self):
        c=copy.deepcopy(self.base); c['moduliq_cpld_soft_i2c'][1]['sda']=8
        self.check(c,'must not overlap')
    def test_protected(self):
        c=copy.deepcopy(self.base); c['moduliq_cpld_gpio']['upper_input_mask']=1
        self.check(c,'protected inputs')
    def test_ordinary_gpio_conflict(self):
        c=copy.deepcopy(self.base); c['output']=[{'platform':'gpio','id':'bad','pin':{'moduliq_cpld_gpio':'bridge_gpio','number':8,'mode':'OUTPUT'}}]
        self.check(c,'ordinary GPIO')
    def test_limits(self):
        for key,value in [('frequency','100kHz'),('stretch_timeout','1s')]:
            c=copy.deepcopy(self.base); c['moduliq_cpld_soft_i2c'][0][key]=value
            self.check(c,key)
if __name__=='__main__': unittest.main(verbosity=2)
