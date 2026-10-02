"""Keysight B2910BL voltmeter (source 0 A), DAC3 sweep, ADC3 CH4.

Requires detached power stage and owner connection GND -> channel3 pin18.
Verifies SMU source/readback before connecting its output relay. Leaves SMU
OFF/HIZ and DAC2/3 at midscale. Does not perform self-test or program CPLD.
"""
import argparse
import datetime
import json
from pathlib import Path
import re
import subprocess
import sys
import time

sys.path.insert(0,'/home/codex-hil/workspaces/lab-instruments')
from labinstruments.transport import Transport
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--dac-channel',type=int,choices=[2,3],default=3)
a=p.parse_args()
reg=8+a.dac_channel
adc_tx='burst 04 06 C0 00' if a.dac_channel==2 else 'burst 04 07 00 00'
adc_channel=3 if a.dac_channel==2 else 4

def frames(commands, name):
    args=[sys.executable,str(ROOT/'scripts/serial_probe.py')]
    for cmd in ['route 0 2',*commands]: args+=['--command',cmd]
    args+=['--seconds','.5','--output',name+'-uart.log']
    subprocess.run(args,check=True,stdout=subprocess.DEVNULL,timeout=20)
    raw=re.findall(r'err=(\S+) tx=([0-9A-F]+) rx=([0-9A-F]+)',
                   (ROOT/'results'/f'{name}-uart.log').read_text())
    if len(raw)!=len(commands) or any(e!='ESP_OK' for e,_,_ in raw):
        raise RuntimeError('SPI frames missing or driver error')
    return [{'tx':tx,'rx':rx} for _,tx,rx in raw]

report={'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'SMU_host':'192.168.2.35','user_connection':'GND to channel3 connector pin18',
        'DAC_channel':a.dac_channel,'ADC3_channel':adc_channel,
        'mode':'current source 0 A / voltage measurement, local sense',
        'VREF_nominal_V':3.0,'VREF_directly_measured':False,
        'voltage_tolerance_V':.01,'points':[],'CPLD_modified':False,'status':'RUNNING'}
try:
    with Transport({'kind':'tcp','host':report['SMU_host'],'port':5025,'timeout':5}) as io:
        report['IDN']=io.query('*IDN?')
        if 'B2910BL,MY63320305' not in report['IDN']: raise RuntimeError('Wrong SMU')
        report['initial_error']=io.query(':SYST:ERR?')
        if not report['initial_error'].startswith('+0,'): raise RuntimeError('SMU error at preflight')
        try:
            for cmd in [':ABOR', ':OUTP:ON:AUTO OFF', ':OUTP:OFF:MODE HIZ',
                        ':OUTP OFF', ':SENS:REM OFF', ':OUTP:LOW FLO',
                        ':SOUR:FUNC:MODE CURR', ':SOUR:CURR:MODE FIX',
                        ':SOUR:CURR:RANG 1E-8', ':SOUR:CURR 0',
                        ':SENS:FUNC "VOLT"', ':SENS:VOLT:PROT 3.3',
                        ':SENS:VOLT:RANG 20', ':SENS:VOLT:NPLC 1']:
                io.write(cmd)
            readback={q:io.query(q) for q in [':OUTP?',':OUTP:ON:AUTO?',
                ':OUTP:OFF:MODE?',':SOUR:FUNC:MODE?',':SOUR:CURR?',
                ':SOUR:CURR:RANG?',':SENS:REM?',':SENS:VOLT:PROT?',':SYST:ERR?']}
            report['setup_readback']=readback
            if (readback[':OUTP?']!='0' or readback[':OUTP:ON:AUTO?']!='0' or
                readback[':OUTP:OFF:MODE?']!='HIZ' or
                readback[':SOUR:FUNC:MODE?']!='CURR' or
                float(readback[':SOUR:CURR?'])!=0 or readback[':SENS:REM?']!='0' or
                abs(float(readback[':SENS:VOLT:PROT?'])-3.3)>1e-6 or
                not readback[':SYST:ERR?'].startswith('+0,')):
                raise RuntimeError('Voltmeter configuration not verified')
            io.write(':OUTP ON')
            if io.query(':OUTP?')!='1': raise RuntimeError('Measurement relay not enabled')
            for code in [0,16384,32768,49152,65535]:
                raw=frames([f'burst 05 {reg:02X} {code>>8:02X} {code&255:02X}',
                            f'burst 05 {0x80|reg:02X} 00 00','burst 05 00 00 00',
                            adc_tx],f'smu-pin18-dac{a.dac_channel}-{code:04x}')
                if bytes.fromhex(raw[2]['rx'])[1:]!=bytes([0x80|reg,code>>8,code&255]):
                    raise RuntimeError('DAC readback mismatch')
                b=bytes.fromhex(raw[3]['rx']);adc=((b[2]&15)<<8)|b[3]
                time.sleep(.1)
                v=float(io.query(':MEAS:VOLT?'))
                if abs(v)>10: raise RuntimeError('Invalid or out-of-range SMU reading')
                expected=3*code/65536
                row={'DAC_code':code,'ADC_code':adc,'ADC3_channel':adc_channel,'expected_V_nominal':expected,
                     'SMU_V':v,'error_from_nominal_V':v-expected,
                     'status':'PASS' if abs(v-expected)<=.01 else 'FAIL','frames':raw}
                report['points'].append(row);print(json.dumps(row),flush=True)
                if row['status']=='FAIL':
                    break  # Diagnose connection before additional voltage steps.
            report['status']='PASS' if all(p['status']=='PASS' for p in report['points']) else 'FAIL_VOLTAGE_TRACKING'
        finally:
            io.write(':OUTP:ON:AUTO OFF');io.write(':OUTP:OFF:MODE HIZ');io.write(':OUTP OFF')
            report['SMU_final_output']=io.query(':OUTP?')
            report['SMU_final_off_mode']=io.query(':OUTP:OFF:MODE?')
            report['SMU_final_error']=io.query(':SYST:ERR?')
except Exception as e:
    report['status']='FAIL';report['exception']=str(e)
    raise
finally:
    try:
        frames(['burst 05 0A 80 00','burst 05 0B 80 00'],'smu-pin18-restored-midscale')
        report['DAC2_DAC3_final']='8000 / 8000'
    finally:
        (ROOT/'results'/f'smu-pin18-dac{a.dac_channel}-sweep.json').write_text(json.dumps(report,indent=2)+'\n')
        print('status:',report['status'],flush=True)
