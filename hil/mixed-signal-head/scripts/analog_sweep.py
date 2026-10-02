"""Explicit DAC2 sweep with raw ADC3 readouts and DMM. Run under sg dialout.

Requires disconnected power stage and already verified DAC CONFIG=0500,
GAIN=010F. No CPLD access; each burst is one hardware transfer in raw firmware.
Readback validates digital registers only, not analog output connectivity.
"""
import datetime
import json
from pathlib import Path
import re
import subprocess
import sys
import time

sys.path.insert(0, '/home/codex-hil/workspaces/lab-instruments')
from labinstruments.transport import Transport

ROOT = Path(__file__).resolve().parents[1]

def frames(commands, name):
    args = [sys.executable, str(ROOT/'scripts/serial_probe.py')]
    for command in ['route 0 2', *commands]:
        args += ['--command', command]
    args += ['--seconds', '.5', '--output', name+'-uart.log']
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, timeout=30)
    log = (ROOT/'results'/f'{name}-uart.log').read_text()
    if 'ROUTE err=ESP_OK' not in log:
        raise RuntimeError('Route not confirmed')
    parsed = re.findall(r'err=(\S+) tx=([0-9A-F]+) rx=([0-9A-F]+)', log)
    if len(parsed) != len(commands) or any(e != 'ESP_OK' for e, _, _ in parsed):
        raise RuntimeError('Missing frame or SPI driver error')
    return [{'tx': tx, 'rx': rx} for _, tx, rx in parsed]

report = {'timestamp_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'DAC_channel': 2, 'DAC3_common_code': '8000',
          'reference_V_nominal_not_measured': 3.0,
          'loopback_user': 'VOUTC_N to INC_N; VOUTC_P to INC_P',
          'loopback_ADC_connectivity': 'unconfirmed: schematic IN_P/N not ADC inputs',
          'DMM_connection': 'unconfirmed', 'CPLD_modified': False, 'points': []}
try:
    with Transport({'kind':'tcp', 'host':'192.168.2.27', 'port':5025,
                    'timeout':5}) as meter:
        report['DMM_ID'] = meter.query('*IDN?')
        if '04679690' not in report['DMM_ID'] or 'VOLT:DC' not in meter.query('SENS:FUNC?'):
            raise RuntimeError('Unexpected meter or measurement function')
        for code in [0, 16384, 32768, 49152, 65535]:
            name = f'analog-dac2-{code:04x}'
            commands = [f'burst 05 0A {code>>8:02X} {code&255:02X}',
                        'burst 05 8A 00 00', 'burst 05 00 00 00']
            for ch in range(8):
                commands.append(f'burst 04 {6+(ch>>2):02X} {(ch&3)<<6:02X} 00')
            raw = frames(commands, name)
            echo = bytes.fromhex(raw[2]['rx'])[1:]
            if echo != bytes([0x8A, code>>8, code&255]):
                raise RuntimeError(f'DAC code readback mismatch {echo.hex()}')
            adc = []
            for ch, f in enumerate(raw[3:]):
                b = bytes.fromhex(f['rx'])
                adc.append({'channel':ch, 'raw_nominal_12bit':((b[2]&15)<<8)|b[3],
                            'validated':False})
            point = {'code':code, 'DAC_register_readback':'PASS',
                     'DAC_P_expected_V_nominal':3*code/65536,
                     'DAC_P_minus_common_expected_V_nominal':3*code/65536-1.5,
                     'DMM_V':float(meter.query('READ?')),
                     'ADC3':adc, 'frames':raw}
            report['points'].append(point)
            print(json.dumps(point), flush=True)
finally:
    try:
        frames(['burst 05 0A 80 00','burst 05 0B 80 00'], 'analog-restored-midscale')
        report['final_DAC2_DAC3_codes'] = '8000 / 8000 (nominal differential 0 V)'
    except Exception as e:
        report['restore_error'] = str(e)
        raise
    finally:
        (ROOT/'results/analog-sweep.json').write_text(json.dumps(report, indent=2)+'\n')
