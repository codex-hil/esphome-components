"""Real ESPHome YAML validation, including cross-component ownership and board constraints."""
import copy
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
import yaml
from esphome import loader
from esphome.core import CORE
import esphome.config_validation as cv

ROOT = Path(__file__).resolve().parents[1]
loader.install_meta_finder(ROOT / "components")
from esphome.components.moduliq_cpld_i2c import CONFIG_SCHEMA as READOUT

class LocalSchemaTests(unittest.TestCase):
    def setUp(self):
        CORE.reset()

    def readout(self, **changes):
        value=dict(id="reader",module_address=0)
        value.update(changes)
        return READOUT(value)

    def test_address(self):
        for ga in range(16):
            self.assertEqual(self.readout(module_address=ga)["address"],0x50+ga)
        for changes in [dict(module_address=16),dict(address=0x51),dict(module_address=-1)]:
            with self.subTest(changes=changes), self.assertRaises(cv.Invalid):
                self.readout(**changes)

    def test_no_board_profile_or_adc_activation_inference(self):
        config=self.readout(adc={"bits":8})
        self.assertFalse(config["adc"]["enabled"])
        with self.assertRaises(cv.Invalid): self.readout(board_profile="rtd16_rev10")

    def test_adc_bounds_duplicates_and_timeout(self):
        for adc in [
            {"bits":10,"timeout":"100ms"},
            {"bits":12},
            {"bits":8,"channels":[{"channel":0,"raw":{"name":"a"}},{"channel":0,"raw":{"name":"b"}}]},
            {"bits":8,"channels":[{"channel":0,"id":{"name":"x"},"bands":[{"min":20,"max":10,"id":"x"}]}]},
            {"bits":8,"channels":[{"channel":0,"id":{"name":"x"},"bands":[{"min":100,"max":256,"id":"x"}]}]},
            {"bits":8,"channels":[{"channel":0,"id":{"name":"x"},"bands":[{"min":10,"max":20,"id":"a"},{"min":20,"max":30,"id":"b"}]}]},
        ]:
            with self.subTest(adc=adc), self.assertRaises(cv.Invalid): self.readout(adc=adc)
        self.readout(adc={"bits":10,"timeout":"500ms"})

    def test_ids_are_generic_yaml_fields(self):
        base=dict(bank="upper",mask=0x07,codes={0:"zero"},sensor={"name":"ID"})
        # The CPLD readout does not assign DRDY or temperature meanings to any bits.
        self.readout(digital_ids=[dict(base,mask=0x88,codes={0:"zero",0x88:"code"})])
        for changes in [dict(shift=4),dict(xor_mask=0x80),dict(codes={8:"bad"})]:
            with self.subTest(changes=changes), self.assertRaises(cv.Invalid):
                self.readout(digital_ids=[dict(base,**changes)])

    def test_counter_unique_consumer(self):
        with self.assertRaises(cv.Invalid):
            self.readout(error_counters=[{"channel":0,"sensor":{"name":"a"}},{"channel":0,"sensor":{"name":"b"}}])

class FullYAMLTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base=yaml.safe_load((ROOT/"examples/cpld-adc-flash.yaml").read_text())
        cls.gpio=yaml.safe_load((ROOT/"examples/cpld-gpio.yaml").read_text())
        cls.solo=yaml.safe_load((ROOT/"examples/cpld-readout.yaml").read_text())
        cls.tmp=tempfile.TemporaryDirectory()

    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()

    def check(self, config, expected=None):
        config=copy.deepcopy(config)
        config["external_components"][0]["source"]["path"]=str(ROOT/"components")
        path=Path(self.tmp.name)/"config.yaml"
        path.write_text(yaml.safe_dump(config,sort_keys=False))
        result=subprocess.run([sys.executable,"-m","esphome","config",str(path)],capture_output=True,text=True)
        output=result.stdout+result.stderr
        if expected is None: self.assertEqual(result.returncode,0,output)
        else:
            self.assertNotEqual(result.returncode,0,output)
            self.assertIn(expected,output)

    def test_three_valid_examples(self):
        for value in (self.base,self.gpio,self.solo): self.check(value)

    def test_yaml_input_mask(self):
        for number in (8,11,15):
            c=copy.deepcopy(self.gpio); c["output"][0]["pin"]["number"]=number
            self.check(c,"externally driven input")
        c=copy.deepcopy(self.gpio); c["moduliq_cpld_gpio"]["upper_input_mask"]=0
        c["output"][0]["pin"]["number"]=11
        self.check(c) # No implicit temperature/DRDY profile in the generic driver.

    def test_optional_link_only_explicit(self):
        c=copy.deepcopy(self.base); del c["moduliq_cpld_i2c"]["gpio_id"]
        self.check(c) # Presence and GA do not automatically link readout to GPIO.
        c=copy.deepcopy(self.base); c["moduliq_cpld_i2c"]["module_address"]=1
        self.check(c,"same module_address")
        c=copy.deepcopy(self.gpio); c["moduliq_cpld_i2c"]["module_address"]=1
        self.check(c) # Independent readout may address another module.

    def test_flash_channels_cs_modes_and_pins(self):
        cases=[
            (lambda c:c["moduliq_cpld_gpio"].update(spi_id="mux_ch2"),"channel 3"),
            (lambda c:c["moduliq_cpld_gpio"].update(spi_mode="MODE1"),"requires SPI mode0"),
            (lambda c:c["moduliq_cpld_flash"].update(spi_id="mux_ch3"),"channel 2"),
            (lambda c:c["moduliq_cpld_flash"]["cs_pin"].update(number="GPIO19"),"share addrspi mux and host CS"),
            (lambda c:c["moduliq_cpld_flash"].pop("spi_mode"),"explicit spi_mode and data_rate"),
            (lambda c:c["moduliq_cpld_flash"].pop("data_rate"),"explicit spi_mode and data_rate"),
            (lambda c:c["output"][0]["pin"].update(number=3),"reserved by the Flash pinmux"),
            (lambda c:c["output"][0]["pin"].update(number=2),"reserved by the Flash pinmux"),
        ]
        for mutation,expected in cases:
            with self.subTest(expected=expected):
                c=copy.deepcopy(self.base); mutation(c); self.check(c,expected)

    def test_missing_physical_miso(self):
        c=copy.deepcopy(self.base); del c["spi"]["miso_pin"]
        self.check(c,"requires MOSI and MISO")

    def test_outer_ga(self):
        c=copy.deepcopy(self.base)
        c["addrspi"][0]["channels"][0]["channel"]=1
        self.check(c,"must select module_address")

    def test_duplicate_owners(self):
        for key,message in [("moduliq_cpld_i2c","One readout/ADC/counter owner"),("moduliq_cpld_gpio","Only one GPIO/CFG owner"),("moduliq_cpld_flash","Only one Flash owner")]:
            c=copy.deepcopy(self.base); duplicate=copy.deepcopy(c[key]); duplicate["id"]="duplicate"
            if key=="moduliq_cpld_i2c":
                # Avoid duplicate entity IDs/names obscuring the ownership validation.
                duplicate={k:v for k,v in duplicate.items() if k in ("id","module_address","gpio_id","i2c_id")}
            if key=="moduliq_cpld_flash": duplicate.pop("status")
            c[key]=[c[key],duplicate]
            self.check(c,message)

    def test_unrelated_spi_client_has_own_configuration(self):
        c=copy.deepcopy(self.gpio)
        existing=yaml.safe_load((ROOT/"examples/bridge-adc.yaml").read_text())
        # This is a schema isolation test, not a board wiring recommendation.
        c["external_components"][0]["components"].append("ads124s08_base")
        c["moduliq_cpld_gpio"]["cs_pin"]["allow_other_uses"]=True
        c["ads124s08_base"]=[existing["ads124s08_base"][0]]
        c["addrspi"][1]["channels"].append({"bus_id":"other_client_bus","channel":2})
        c["ads124s08_base"][0]["spi_id"]="other_client_bus"
        self.check(c) # CPLD validators neither restrict nor inspect another driver's channel.

    def test_gpio_flash_without_i2c(self):
        c=copy.deepcopy(self.base)
        c.pop("moduliq_cpld_i2c"); c.pop("i2c")
        c["external_components"][0]["components"].remove("moduliq_cpld_i2c")
        self.check(c)


if __name__=="__main__": unittest.main(verbosity=2)
