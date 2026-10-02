"""Measure owner-connected OUT bit B on CH4, without GPIO IN reads.

Uses the explicit raw probe already flashed on the designated ESP32.
Run under the pre-existing dialout membership. Leaves OUT with a 00 command.
"""
import json
from pathlib import Path
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
rows = []
try:
    for code in [0x00, 0x55, 0xAA, 0xFF]:
        name = f'gpio-pattern-{code:02x}'
        serial_args = [sys.executable, str(ROOT/'scripts/serial_probe.py'),
                       '--command', 'route 0 2', '--command', f'burst 01 {code:02X}',
                       '--seconds', '.5', '--output', name+'-uart.log']
        subprocess.run(serial_args, check=True, stdout=subprocess.DEVNULL, timeout=15)
        scope_args = [sys.executable, str(ROOT/'scripts/scope_capture.py'),
                      '--host', '192.168.2.4', '--action', 'idle',
                      '--ch4-label', 'DIS_B_to_OK_A', '--name', name]
        subprocess.run(scope_args, check=True, stdout=subprocess.DEVNULL, timeout=30)
        meta = json.loads((ROOT/'results'/f'{name}.json').read_text())
        q = meta['channels']['4']['preamble']
        b = (ROOT/'results'/f'{name}-ch4.bin').read_bytes()
        volts = [((x if x < 128 else x-256)-q['yoff'])*q['ymult']+q['yzero'] for x in b]
        median = statistics.median(volts)
        expected = (code >> 1) & 1
        good = (2.7 <= median <= 3.6) if expected else (-.3 <= median <= .3)
        row = {'OUT_code': f'{code:02X}', 'observed_pin': 'DIS B = IC9 QB pin1',
               'expected_bit': expected, 'median_V': median,
               'min_V': min(volts), 'max_V': max(volts),
               'status': 'PASS' if good else 'FAIL', 'capture': name+'.json'}
        rows.append(row)
        print(json.dumps(row), flush=True)
finally:
    subprocess.run([sys.executable, str(ROOT/'scripts/serial_probe.py'),
                    '--command', 'route 0 2', '--command', 'burst 01 00',
                    '--seconds', '.5', '--output', 'gpio-restored-zero-uart.log'],
                   check=True, stdout=subprocess.DEVNULL, timeout=15)
    (ROOT/'results/digital-physical-patterns.json').write_text(json.dumps({
        'observed_output_only': 'B (QB)', 'other_outputs': 'NOT_OBSERVED',
        'GPIO_IN_reads': 'none during this test', 'patterns': rows,
        'final_OUT_command': '00', 'CPLD_modified': False}, indent=2)+'\n')
