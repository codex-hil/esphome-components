"""Exercise the real ESPHome schema, including invalid wiring/configuration."""
import sys
import unittest
from pathlib import Path
from esphome import loader
from esphome.core import CORE
import esphome.config_validation as cv

ROOT = Path(__file__).resolve().parents[1]
loader.install_meta_finder(ROOT / "components")
from esphome.components.rtd.sensor import CONFIG_SCHEMA

class SchemaTests(unittest.TestCase):
    def setUp(self):
        CORE.reset()

    def valid(self, **changes):
        config = dict(id="temperature", sensor="resistance", nominal_resistance="100 Ohm")
        config.update(changes)
        return CONFIG_SCHEMA(config)

    def test_nominals(self):
        for raw, expected in [(100,100), (1000,1000), ("100 Ohm",100), ("1 kOhm",1000)]:
            with self.subTest(raw=raw):
                self.assertEqual(self.valid(nominal_resistance=raw)["nominal_resistance"],expected)

    def test_invalid_nominal(self):
        for raw in [0, -100, 500, float("nan"), float("inf"), "100 V"]:
            with self.subTest(raw=raw), self.assertRaises(cv.Invalid):
                self.valid(nominal_resistance=raw)

    def test_required(self):
        for missing in ["sensor", "nominal_resistance"]:
            config=dict(id="temperature",sensor="resistance",nominal_resistance=100)
            del config[missing]
            with self.subTest(missing=missing), self.assertRaises(cv.Invalid):
                CONFIG_SCHEMA(config)

    def test_self_reference(self):
        with self.assertRaises(cv.Invalid):
            self.valid(sensor="temperature")

    def test_event_driven(self):
        with self.assertRaises(cv.Invalid):
            self.valid(update_interval="1s")

if __name__ == "__main__":
    unittest.main()
