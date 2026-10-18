"""Validate complete YAML with the installed ESPHome, including pin ownership."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import yaml

ROOT = Path(__file__).resolve().parents[1]

class MatrixYAMLTests(unittest.TestCase):
    def setUp(self):
        self.config = yaml.safe_load((ROOT / 'examples/switch-matrix-32-2.yaml').read_text())
        self.config['external_components'][0]['source']['path'] = str(ROOT / 'components')

    def check(self, error=None):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'matrix.yaml'
            path.write_text(yaml.safe_dump(self.config, sort_keys=False))
            result = subprocess.run([sys.executable, '-m', 'esphome', 'config', str(path)],
                                    capture_output=True, text=True)
        output = result.stdout + result.stderr
        if error:
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn(error, output)
        else:
            self.assertEqual(result.returncode, 0, output)

    def test_all_32_channels(self): self.check()

    def test_channel_range(self):
        for ch in (0, 33):
            with self.subTest(channel=ch):
                self.config['select'][0]['channel'] = ch
                self.check('channel')

    def test_duplicate_channel(self):
        self.config['select'][1]['channel'] = 1
        self.check('Only one select')

    def test_pin_collision(self):
        self.config['switch_matrix_32_2']['cs_b_pin'] = 'GPIO16'
        self.check('multiple places')

    def test_polarity(self):
        self.config['switch_matrix_32_2']['oe_pin'] = {'number': 21, 'inverted': True}
        self.check('physical pin polarity')

    def test_spi_mode(self):
        self.config['switch_matrix_32_2']['spi_mode'] = 'MODE1'
        self.check('MODE0')

    def test_generic_cs_forbidden(self):
        self.config['switch_matrix_32_2']['cs_pin'] = 'GPIO22'
        self.check('Use cs_a_pin')

    def test_zero_delay_forbidden(self):
        self.config['switch_matrix_32_2']['break_before_make'] = '0ms'
        self.check('break_before_make')

    def test_api_schema(self):
        # !lambda is exercised by the separate compile fixture; the action schema
        # also accepts literal string masks without a lambda.
        self.config['wifi'] = {'ssid': 'COMPILE_ONLY', 'password': 'compile-only-password'}
        self.config['api'] = {'actions': [{'action': 'set_matrix_masks',
            'variables': {'mask_a': 'string', 'mask_b': 'string'},
            'then': [{'switch_matrix_32_2.set_masks': {'id': 'matrix', 'mask_a': '0', 'mask_b': '0'}}]}]}
        self.check()

if __name__ == '__main__': unittest.main()
