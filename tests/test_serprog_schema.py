"""Validation and code generation under the pinned real ESPHome runtime."""
import copy
import tempfile
import unittest
from pathlib import Path
from test_cpld_schema import FullYAMLTests, ROOT
import yaml

class SerprogSchemaTests(unittest.TestCase):
    check = FullYAMLTests.check
    tearDownClass = FullYAMLTests.__dict__["tearDownClass"]
    # Reuse only the YAML validation helper, not inherited CPLD cases.
    @classmethod
    def setUpClass(cls):
        cls.base=yaml.safe_load((ROOT/'examples/cpld-serprog.yaml').read_text())
        cls.tmp=tempfile.TemporaryDirectory()

    def test_example(self): self.check(self.base)

    def test_bounds(self):
        for port in [0,65536]:
            c=copy.deepcopy(self.base); c['moduliq_serprog']['port']=port
            self.check(c,'port')
        c=copy.deepcopy(self.base); c['moduliq_serprog']['session_timeout']='1ms'
        self.check(c,'at least')

    def test_duplicate_server(self):
        for mutation,expected in [({},'Only one serprog server'),({'flash_id':'missing'},"Couldn't find ID")]:
            c=copy.deepcopy(self.base)
            dup={'id':'duplicate','flash_id':'target_flash','port':6055,**mutation}
            c['moduliq_serprog']=[c['moduliq_serprog'],dup]
            self.check(c,expected)

    def test_multiple_memories_and_unique_ports(self):
        c=copy.deepcopy(self.base)
        c['addrspi'][0]['channels'].append({'bus_id':'module1','channel':1})
        for mux in [c['addrspi'][1]]:
            mux['address_pins']=[{'number':pin,'allow_other_uses':True} for pin in mux['address_pins']]
        inner=copy.deepcopy(c['addrspi'][1]); inner.update(id='mux1',spi_id='module1')
        inner['channels']=[{'bus_id':'other_ch2','channel':2},{'bus_id':'other_ch3','channel':3}]
        c['addrspi'].append(inner)
        gpio=copy.deepcopy(c['moduliq_cpld_gpio']); gpio.update(id='gpio1',spi_id='other_ch3',module_address=1)
        c['moduliq_cpld_gpio']=[c['moduliq_cpld_gpio'],gpio]
        flash=copy.deepcopy(c['moduliq_cpld_flash']); flash.pop('status')
        flash.update(id='another_flash',gpio_id='gpio1',spi_id='other_ch2')
        c['moduliq_cpld_flash']=[c['moduliq_cpld_flash'],flash]
        c['moduliq_serprog']=[c['moduliq_serprog'], {'id':'second_programmer','flash_id':'another_flash','port':6055}]
        self.check(c)
        c['moduliq_serprog'][1]['port']=6054
        self.check(c,'Serprog TCP ports must be unique')

    def test_missing_network(self):
        c=copy.deepcopy(self.base); c.pop('wifi')
        self.check(c,'network')

if __name__=='__main__':
    unittest.main(verbosity=2,defaultTest='SerprogSchemaTests')
