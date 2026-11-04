"""Run with the pinned ESPHome Python. No network or device access."""
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import PropertyMock, patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('esphome.components.zerotier', ROOT / 'components/zerotier/__init__.py', submodule_search_locations=[str(ROOT / 'components/zerotier')])
zt = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = zt
spec.loader.exec_module(zt)

class SchemaTests(unittest.TestCase):
    def test_network_id(self):
        self.assertEqual(zt.network_id('1234567890ABCDEF'), '1234567890abcdef')
        for value in ('0', '0000000000000000', 'ff12345678901234', 'x234567890abcdef', 1234567890123456):
            with self.subTest(value=value), self.assertRaises(zt.cv.Invalid):
                zt.network_id(value)

    def validate(self, framework='esp-idf', variant='ESP32P4', psram=None, sensors=None):
        with patch.object(type(zt.CORE), 'target_framework', new_callable=PropertyMock, return_value=framework), patch.object(zt.esp32, 'get_esp32_variant', return_value=variant):
            token = zt.fv.full_config.set({'psram': psram or {'ignore_not_found': False}, 'sensor': sensors or []})
            try:
                return zt.validate({})
            finally:
                zt.fv.full_config.reset(token)

    def test_supported_target(self):
        self.assertEqual(self.validate(), {})

    def test_unsupported_targets(self):
        for args in ({'sensors': [{'platform': 'adc'}]}, {'framework': 'arduino'}, {'variant': 'ESP32'}, {'psram': {'ignore_not_found': True}}, {'psram': {'disabled': True}}):
            with self.subTest(args=args), self.assertRaises(zt.cv.Invalid):
                self.validate(**args)

if __name__ == '__main__':
    unittest.main()
