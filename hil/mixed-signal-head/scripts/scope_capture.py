"""Four simultaneous analog channels, MSO4104. Configure, arm, then read.

Run --action idle for a passive baseline; --action arm before a manual burst;
--action read afterwards. Scope is stopped during binary export. No SPI is sent.
"""
import argparse
import csv
import json
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, '/home/codex-hil/workspaces/lab-instruments')
from labinstruments.transport import Transport

ROOT = Path(__file__).resolve().parents[1]
MAP = {1: 'SCK', 2: 'CSn_HC165', 3: 'MOSI', 4: 'Header_done'}
p = argparse.ArgumentParser()
p.add_argument('--action', choices=['idle', 'arm', 'read'], required=True)
p.add_argument('--host', default='192.168.2.9')
p.add_argument('--scale', type=float, default=50e-6, help='seconds/div')
p.add_argument('--name', default='scope')
p.add_argument('--trigger-channel', type=int, choices=[1, 2, 3, 4], default=4)
p.add_argument('--ch4-label', default='Header_done')
p.add_argument('--trigger-position', type=float, default=10)
a = p.parse_args()
MAP[4] = a.ch4_label
if not 1e-9 <= a.scale <= 1 or not 0 <= a.trigger_position <= 100 or Path(a.name).name != a.name:
    p.error('invalid scale or filename')

with Transport({'kind': 'tcp', 'host': a.host, 'port': 4000, 'timeout': 4,
                'delay': .01, 'read_padding': '\r'}) as io:
    identity = io.query('*IDN?')
    if 'MSO4104' not in identity or 'C020817' not in identity:
        raise RuntimeError(f'Unexpected scope: {identity}')
    io.write('HEADER OFF')
    if a.action in ('idle', 'arm'):
        io.write('ACQ:STATE STOP')
        io.write('ACQ:MODE SAMPLE')
        io.write('ACQ:STOPAFTER RUNSTOP' if a.action == 'idle' else 'ACQ:STOPAFTER SEQUENCE')
        for ch in MAP:
            io.write(f'SEL:CH{ch} ON')
            io.write(f'CH{ch}:COUP DC')
            io.write(f'CH{ch}:TER MEG')
            io.write(f'CH{ch}:SCALE 1')
            io.write(f'CH{ch}:OFFSET 0')
            io.write(f'CH{ch}:POSITION 0')
        io.write('HOR:RECORDLENGTH 100000')
        io.write(f'HOR:SCALE {a.scale:.12g}')
        io.write(f'HOR:TRIGGER:POSITION {a.trigger_position}')
        io.write('TRIG:A:TYPE EDGE')
        io.write(f'TRIG:A:EDGE:SOURCE CH{a.trigger_channel}')
        io.write('TRIG:A:EDGE:SLOPE RISE')
        io.write(f'TRIG:A:LEVEL:CH{a.trigger_channel} 1.5')
        io.write('TRIG:A:MOD AUTO' if a.action == 'idle' else 'TRIG:A:MOD NORMAL')
        io.write('ACQ:STATE RUN')
        if a.action == 'arm':
            ready_deadline = time.monotonic() + 5
            while io.query('TRIG:STATE?').strip().upper() not in ('READY', 'ARMED', 'ARM'):
                # Repeating traffic can complete a single-shot before READY is polled.
                if io.query('ACQ:STATE?').strip().upper() in ('0', 'OFF', 'STOP') and int(io.query('ACQ:NUMACQ?')) >= 1:
                    print('Single-shot completed during arm status polling; export with --action read.')
                    break
                if time.monotonic() > ready_deadline:
                    io.write('ACQ:STATE STOP')
                    raise TimeoutError('Scope did not arm')
                time.sleep(.05)
            print(f'Armed on CH{a.trigger_channel} {MAP[a.trigger_channel]} rising. Send one frame; then run --action read.')
            sys.exit(0)
        idle_deadline = time.monotonic() + 5
        # NUMACQ can briefly retain the previous single-shot count after RUN.
        # Allow continuous AUTO acquisition to replace the old record first.
        time.sleep(max(.5, float(io.query('HOR:SCALE?'))*10+.2))
        while int(io.query('ACQ:NUMACQ?')) < 1:
            if time.monotonic() > idle_deadline:
                io.write('ACQ:STATE STOP')
                raise TimeoutError('No fresh idle acquisition')
            time.sleep(.05)
        io.write('ACQ:STATE STOP')
    deadline = time.monotonic() + 10
    while io.query('ACQ:STATE?').strip().upper() not in ('0', 'OFF', 'STOP'):
        if time.monotonic() > deadline:
            io.write('ACQ:STATE STOP')
            raise TimeoutError('No completed acquisition; no waveform result')
        time.sleep(.05)
    result = {'timestamp_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'idn': identity, 'action': a.action, 'probe_map': MAP, 'channels': {}}
    for ch in MAP:
        io.write(f'DATA:SOURCE CH{ch}')
        io.write('DATA:ENCDG RIBINARY')
        io.write('DATA:WIDTH 1')
        io.write('DATA:START 1')
        n = int(io.query('HOR:RECORDLENGTH?'))
        if not 64 <= n <= 100000:
            raise ValueError('Unexpected record length')
        io.write(f'DATA:STOP {n}')
        keys = {'dt_s': 'XINCR', 'xzero': 'XZERO', 'pt_off': 'PT_OFF',
                'ymult': 'YMULT', 'yoff': 'YOFF', 'yzero': 'YZERO'}
        meta = {k: float(io.query(f'WFMOUTPRE:{v}?')) for k, v in keys.items()}
        if not all(math.isfinite(v) for v in meta.values()) or meta['dt_s'] <= 0:
            raise ValueError('Invalid preamble')
        if io.query('WFMOUTPRE:XUNIT?').strip('"').lower() != 's' or \
                io.query('WFMOUTPRE:YUNIT?').strip('"').upper() != 'V':
            raise ValueError('Unexpected units')
        block = io.transaction(b'CURVE?\n')
        if block[:1] != b'#' or block[1:2] not in b'123456789':
            raise ValueError('Invalid binary block')
        digits = int(block[1:2])
        size = int(block[2:2+digits])
        data = block[2+digits:2+digits+size]
        if size != n or len(data) != n:
            raise ValueError('Incomplete waveform')
        stem = ROOT / 'results' / f'{a.name}-ch{ch}'
        stem.with_suffix('.bin').write_bytes(data)
        volts = [((b if b < 128 else b-256)-meta['yoff'])*meta['ymult']+meta['yzero'] for b in data]
        t0 = meta['xzero']-meta['pt_off']*meta['dt_s']
        with stem.with_suffix('.csv').open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['time_s', 'voltage_V'])
            writer.writerows((t0+i*meta['dt_s'], v) for i, v in enumerate(volts))
        result['channels'][ch] = {'preamble': meta, 'n': n, 'min_V': min(volts),
                                  'max_V': max(volts), 'probe_gain': io.query(f'CH{ch}:PROBE:GAIN?'),
                                  'clipped': any(b in (127, 128, 129, 126) for b in data)}
    (ROOT / 'results' / f'{a.name}.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))
