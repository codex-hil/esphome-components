"""DAC3/common -> owner-connected connector pin18, referenced to GND.

Requires detached power stage and existing DAC CONFIG=0500, GAIN=010F.
Preserves original DMM range configuration; restores DAC2/3 to midscale.
"""
import datetime
import json
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, '/home/codex-hil/workspaces/lab-instruments')
from labinstruments.transport import Transport
ROOT=Path(__file__).resolve().parents[1]

def send(commands, stem):
    args=[sys.executable,str(ROOT/'scripts/serial_probe.py')]
    for cmd in ['route 0 2',*commands]:args+=['--command',cmd]
    args+=['--seconds','.5','--output',stem+'-uart.log']
    subprocess.run(args,check=True,stdout=subprocess.DEVNULL,timeout=20)
    text=(ROOT/'results'/f'{stem}-uart.log').read_text()
    raw=re.findall(r'err=(\S+) tx=([0-9A-F]+) rx=([0-9A-F]+)',text)
    if len(raw)!=len(commands) or any(err!='ESP_OK' for err,_,_ in raw):
        raise RuntimeError('Missing frames or driver error')
    return [{'tx':tx,'rx':rx} for _,tx,rx in raw]

report={'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'user_DMM_connection':'GND to connector pin18; connector identity pending',
        'DAC_channel':3,'points':[],'CPLD_modified':False}
try:
    with Transport({'kind':'tcp','host':'192.168.2.27','port':5025,'timeout':5}) as meter:
        report['DMM_ID']=meter.query('*IDN?')
        if '04679690' not in report['DMM_ID'] or 'VOLT:DC' not in meter.query('SENS:FUNC?'):
            raise RuntimeError('Wrong instrument or function')
        report['DMM_terminals']=meter.query('ROUT:TERM?')
        old_range=meter.query('SENS:VOLT:DC:RANG?')
        old_auto=meter.query('SENS:VOLT:DC:RANG:AUTO?')
        report['DMM_original_range']=old_range
        report['DMM_original_autorange']=old_auto
        try:
            meter.write('SENS:VOLT:DC:RANG 10')
            for code in [0,16384,32768,49152,65535]:
                raw=send([f'burst 05 0B {code>>8:02X} {code&255:02X}',
                          'burst 05 8B 00 00','burst 05 00 00 00',
                          'burst 04 07 00 00'],f'dmm-pin18-dac3-{code:04x}')
                if bytes.fromhex(raw[2]['rx'])[1:]!=bytes([0x8B,code>>8,code&255]):
                    raise RuntimeError('DAC3 readback mismatch')
                b=bytes.fromhex(raw[3]['rx'])
                point={'code':code,'expected_V_nominal':3*code/65536,
                       'ADC3_CH4_code':((b[2]&15)<<8)|b[3],
                       'DMM_V':float(meter.query('READ?')),'frames':raw}
                report['points'].append(point);print(json.dumps(point),flush=True)
        finally:
            meter.write('SENS:VOLT:DC:RANG '+old_range.strip())
            meter.write('SENS:VOLT:DC:RANG:AUTO '+old_auto.strip())
finally:
    try:
        send(['burst 05 0A 80 00','burst 05 0B 80 00'],'dmm-pin18-restored-midscale')
        report['DAC2_DAC3_final']='8000 / 8000'
    finally:
        (ROOT/'results/dmm-pin18-sweep.json').write_text(json.dumps(report,indent=2)+'\n')
