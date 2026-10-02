"""1080 measured SPI frames: DAC2/3 patterns, readbacks and all 24 ADC inputs.

Requires detached power stage and verified DAC2/3 -> ADC3 CH3/4 loopback.
GPIO IN regression remains blocked by the independent /PL fault.
No firmware upload, CPLD writes or heap requirements implied by host Python.
"""
import datetime
import json
from pathlib import Path
import re
import time
import serial

ROOT = Path(__file__).resolve().parents[1]
PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
report = {'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'status':'RUNNING', 'SPI_frames':0, 'driver_errors':0,
          'failures':[], 'ADC_tolerance_codes':8,
          'absolute_voltage_accuracy':'NOT_VALIDATED_BY_DMM',
          'GPIO_IN':'NOT_RUN_INDEPENDENT_FAULT', 'CPLD_modified':False,
          'cycles':[]}
uart = (ROOT/'results/analog-regression-uart.log').open('w')
s = serial.Serial(); s.port=PORT; s.baudrate=115200; s.timeout=.1
s.dtr=False; s.rts=False

def command(cmd, prefix):
    uart.write('HOST> '+cmd+'\n'); uart.flush()
    s.write((cmd+'\n').encode()); s.flush()
    until = time.monotonic()+3
    while time.monotonic()<until:
        line=s.readline().decode(errors='replace').strip()
        if line:
            uart.write(line+'\n'); uart.flush()
            if line.startswith(prefix): return line
    raise RuntimeError('UART response timeout: '+cmd)

def frame(data):
    line=command('burst '+' '.join(f'{x:02X}' for x in data),'FRAME ')
    m=re.search(r'err=(\S+) tx=([0-9A-F]+) rx=([0-9A-F]+)',line)
    report['SPI_frames']+=1
    if not m or m[1]!='ESP_OK':
        report['driver_errors']+=1
        raise RuntimeError('SPI driver response: '+line)
    if bytes.fromhex(m[2])!=bytes(data): raise RuntimeError('TX mismatch')
    return bytes.fromhex(m[3])[1:]

def read_dac(reg):
    frame([5,0x80|reg,0,0])
    b=frame([5,0,0,0])
    if b[0] != 0x80|reg: raise RuntimeError('DAC echoed address mismatch')
    return int.from_bytes(b[1:],'big')

try:
    s.open(); time.sleep(.5)
    if 'err=ESP_OK' not in command('route 0 2','ROUTE '):
        raise RuntimeError('Route failed')
    # Explicit preflight reads, before altering outputs.
    for reg,expected in [(1,0x0497),(3,0x0500),(4,0x010F),(7,0)]:
        if read_dac(reg)!=expected: raise RuntimeError(f'DAC preflight register {reg} mismatch')
    codes=[0,16384,32768,49152,65535]
    for i in range(36):
        positive=codes[i%5]; common=codes[(i+2)%5]
        frame([5,10,positive>>8,positive&255])
        frame([5,11,common>>8,common&255])
        for reg,expected in [(10,positive),(11,common)]:
            if read_dac(reg)!=expected: raise RuntimeError('DAC code readback mismatch')
        time.sleep(.002)
        values={}
        for addr in [2,3,4]:
            readings=[]
            for ch in range(8):
                b=frame([addr,6+(ch>>2),(ch&3)<<6,0])
                readings.append(((b[1]&15)<<8)|b[2])
            values[str(addr)]=readings
        for ch,code in [(3,positive),(4,common)]:
            expected=code/16
            if abs(values['4'][ch]-expected)>8:
                raise RuntimeError(f'ADC3 CH{ch}: {values["4"][ch]} expected ~{expected}')
        report['cycles'].append({'cycle':i,'DAC2_code':positive,'DAC3_code':common,
                                 'ADC':values,'loopback_status':'PASS'})
        if i%6==0: print(f'cycle {i}: PASS, frames={report["SPI_frames"]}',flush=True)
    report['status']='PASS_ANALOG_LOOPBACK_AND_DIGITAL_READBACK'
except Exception as e:
    report['status']='FAIL'; report['failures'].append(str(e))
    raise
finally:
    try:
        if s.is_open:
            frame([5,10,128,0]); frame([5,11,128,0])
            report['restored_DAC2_DAC3']='8000 / 8000'
    except Exception as e:
        report['restore_error']=str(e); report['status']='FAIL'
    s.close(); uart.close()
    (ROOT/'results/analog-regression.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='cycles'}),flush=True)
