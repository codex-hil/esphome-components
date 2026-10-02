#!/usr/bin/env python3
"""Flash known ESP32, collect real-driver coexistence regression and SMU sweep.

LAB ONLY: requires detached power stage and prior confirmed DAC2/3 -> ADC Z3/4
loopback. SMU Force HI on DAC2 positive measurement point, LO on GND.
No CPLD programming. HC165 is not tested while its /PL circuit is faulty.
"""
import argparse
import concurrent.futures
import datetime
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time
import serial

sys.path.insert(0, '/home/codex-hil/workspaces/lab-instruments')
from labinstruments.transport import Transport
ROOT=Path(__file__).resolve().parents[1]
BRINGUP=ROOT.parent/'moduliq-mixed-signal-bringup'
if not BRINGUP.is_dir(): BRINGUP=ROOT/'hil/mixed-signal-head'
PORT='/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
MAC='68:25:dd:4c:49:e4'
p=argparse.ArgumentParser()
p.add_argument('--label',required=True)
p.add_argument('--capture-gpio',action='store_true')
p.add_argument('--voltage-sweep',action='store_true')
p.add_argument('--no-smu',action='store_true',help='Leave SMU OFF/HIZ during strict ADC loopback regression')
p.add_argument('--diagnostic',action='store_true')
p.add_argument('--no-flash',action='store_true',help='Repeat by resetting the already flashed matching build')
a=p.parse_args()
if Path(a.label).name!=a.label: p.error('label must be a filename')
folder=BRINGUP/'results'/a.label
folder.mkdir(exist_ok=False)
device='moduliq-coexist-voltage' if a.voltage_sweep else 'moduliq-coexist-diagnostic' if a.diagnostic else 'moduliq-coexistence-hil'
config='mixed-signal-coexistence-voltage.yaml' if a.voltage_sweep else 'mixed-signal-coexistence-diagnostic.yaml' if a.diagnostic else 'mixed-signal-coexistence-hil.yaml'
header='mixed_signal_voltage_sweep.h' if a.voltage_sweep else 'mixed_signal_diagnostic.h' if a.diagnostic else 'mixed_signal_coexistence.h'
build=ROOT/'examples/.esphome/build'/device/'build'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
report={'timestamp_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'RUNNING','port':PORT,'expected_mac':MAC,'cpld_modified':False,'HC165':'NOT_TESTED_HARDWARE_PL_FIX_PENDING','configuration_sha256':sha(ROOT/'examples'/config),'test_header_sha256':sha(ROOT/'tests/hil'/header),'source_sha256':{},'SMU_points':[],'SMU_during_regression':'OFF/HIZ' if a.no_smu else 'CONNECTED_MEASUREMENT'}
for name in ['spi','addrspi','addrspi2','mcp3208','dacx0504','spi_shift_register']:
 for f in sorted((ROOT/'components'/name).rglob('*')):
  if f.is_file() and '__pycache__' not in f.parts: report['source_sha256'][str(f.relative_to(ROOT))]=sha(f)
for f in sorted((build.parent/'src/esphome/components/mmc5983_spi').rglob('*')):
 if f.is_file() and '__pycache__' not in f.parts: report['source_sha256'][str(f)]=sha(f)
base=[sys.executable,'-m','esptool','--chip','esp32','--port',PORT]
scope_pool=concurrent.futures.ThreadPoolExecutor(max_workers=1)
scope_future=None
def capture_gpio():
 name=a.label+'-gpio'
 args=[sys.executable,str(BRINGUP/'scripts/scope_capture.py'),'--host','192.168.2.4','--ch4-label','DIS_B','--name',name]
 run(args+['--action','arm','--trigger-channel','1' if a.voltage_sweep else '4','--trigger-position','50','--scale','100e-6'],'scope-arm.log')
 run(args+['--action','read'],'scope-read.log')
 return name
def run(args,name):
 r=subprocess.run(args,capture_output=True,text=True,timeout=100)
 log=r.stdout+r.stderr;(folder/name).write_text(log)
 if r.returncode: raise RuntimeError(f'{name} exit={r.returncode}')
 return log
try:
 with Transport({'kind':'tcp','host':'192.168.2.35','port':5025,'timeout':5}) as io:
  report['SMU_IDN']=io.query('*IDN?')
  if 'B2910BL,MY63320305' not in report['SMU_IDN']: raise RuntimeError('Wrong SMU')
  if not io.query(':SYST:ERR?').startswith('+0,'): raise RuntimeError('SMU pre-existing error')
  try:
   for cmd in [':ABOR',':OUTP:ON:AUTO OFF',':OUTP:OFF:MODE HIZ',':OUTP OFF',':SENS:REM OFF',':OUTP:LOW FLO',':SOUR:FUNC:MODE CURR',':SOUR:CURR:MODE FIX',':SOUR:CURR:RANG 1E-8',':SOUR:CURR 0',':SENS:FUNC "VOLT"',':SENS:VOLT:PROT 3.3',':SENS:VOLT:RANG 20',':SENS:VOLT:NPLC 1']:
    io.write(cmd)
   report['SMU_setup']={q:io.query(q) for q in [':OUTP?',':OUTP:ON:AUTO?',':OUTP:OFF:MODE?',':SOUR:FUNC:MODE?',':SOUR:CURR?',':SOUR:CURR:RANG?',':SENS:REM?',':SENS:VOLT:PROT?',':SYST:ERR?']}
   rb=report['SMU_setup']
   if rb[':OUTP?']!='0' or rb[':OUTP:ON:AUTO?']!='0' or rb[':OUTP:OFF:MODE?']!='HIZ' or rb[':SOUR:FUNC:MODE?']!='CURR' or float(rb[':SOUR:CURR?'])!=0 or rb[':SENS:REM?']!='0' or abs(float(rb[':SENS:VOLT:PROT?'])-3.3)>1e-6 or not rb[':SYST:ERR?'].startswith('+0,'): raise RuntimeError('SMU unsafe/unverified config')
   ident=run(base+['--no-stub','--after','no_reset','read_mac'],'identity.log')
   if MAC not in ident: raise RuntimeError('MAC mismatch, no flash')
   if not a.no_flash:
    args=shlex.split((build/'flash_args').read_text())
    # First six arguments are flash settings; remaining args alternate offset/file.
    for index in range(7,len(args),2): args[index]=str(build/args[index])
    report['firmware_sha256']={args[i]:sha(Path(args[i])) for i in range(7,len(args),2)}
    run(base+['--baud','460800','--after','hard_reset','write_flash']+args,'flash.log')
   else:
    run(base+['--no-stub','--after','hard_reset','read_mac'],'repeat-reset.log')
   s=serial.Serial();s.port=PORT;s.baudrate=115200;s.timeout=.2;s.dtr=False;s.rts=False;s.open()
   if not a.diagnostic and not a.no_smu:
    io.write(':OUTP ON')
    if io.query(':OUTP?')!='1': raise RuntimeError('SMU relay not on')
   raw=bytearray();pending=bytearray();deadline=time.monotonic()+85;final=None
   try:
    with (folder/'uart.log').open('wb') as log:
     while time.monotonic()<deadline:
      chunk=s.read(8192)
      if not chunk: continue
      raw.extend(chunk); log.write(chunk);log.flush();pending.extend(chunk)
      while b'\n' in pending:
       line,_,pending=pending.partition(b'\n');text=line.decode(errors='replace')
       if 'coexist_hil' in text or 'coexist_diag' in text: print(text,flush=True)
       if a.capture_gpio and scope_future is None and ('ARM_GPIO pattern=AA' in text or (not a.voltage_sweep and 'STRESS cycle=0 ' in text)):
        scope_future=scope_pool.submit(capture_gpio)
       dm=re.search(r'PHASE phase=(\d+)',text) if a.diagnostic else None
       if dm:
        phase=int(dm[1]);io.write(':OUTP ON' if phase in (1,2) else ':OUTP OFF');print('SMU_PHASE '+str(phase)+' OUTP='+io.query(':OUTP?'),flush=True)
       rowmatch=re.search(r'ROW phase=2 sample=(\d+)',text) if a.diagnostic else None
       if rowmatch and int(rowmatch[1])%10==0:
        report['SMU_points'].append({'diagnostic_sample':int(rowmatch[1]),'SMU_V':float(io.query(':MEAS:VOLT?'))})
       m=re.search(r'SWEEP point=(\d+) code2=(\d+) code3=(\d+) adc_p=([\d.]+) adc_n=([\d.]+)',text)
       if m and not a.no_smu:
        point,c2,c3,vp,vn=m.groups(); v=float(io.query(':MEAS:VOLT?'));expected=3*int(c2)/65536
        row={'point':int(point),'dac2_code':int(c2),'dac3_code':int(c3),'adc_z3_V':float(vp),'adc_z4_V':float(vn),'SMU_V':v,'nominal_V':expected,'error_V':v-expected,'status':'PASS' if abs(v-expected)<=.01 else 'FAIL'}
        report['SMU_points'].append(row);print('SMU '+json.dumps(row),flush=True)
       if 'FINAL ' in text: final=text;break
      if final: break
   finally: s.close()
   if scope_future is not None:
    try: report['gpio_scope_capture']=scope_future.result(timeout=25)
    except Exception as e: report['gpio_scope_error']=str(e)
   report['final_line']=final
   report['uart_sha256']=sha(folder/'uart.log')
   decoded=raw.decode(errors='replace')
   report['error_lines']=[line for line in decoded.splitlines() if '[E]' in line or 'Brownout' in line or 'Guru Meditation' in line]
   report['warning_lines']=[line for line in decoded.splitlines() if '[W]' in line]
   report['status']='DIAGNOSTIC_CAPTURED' if a.diagnostic and final and 'DIAG_DONE' in final else 'PASS' if final and 'FINAL PASS' in final and (a.no_smu or len(report['SMU_points'])==5) and all(row['status']=='PASS' for row in report['SMU_points']) and not report['error_lines'] else 'FAIL'
  finally:
   io.write(':OUTP:ON:AUTO OFF');io.write(':OUTP:OFF:MODE HIZ');io.write(':OUTP OFF')
   report['SMU_final']={q:io.query(q) for q in [':OUTP?',':OUTP:OFF:MODE?',':SYST:ERR?']}
except Exception as e:
 report['status']='FAIL';report['exception']=str(e);raise
finally:
 scope_pool.shutdown(wait=True)
 (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
 print('RESULT '+report['status'],flush=True)
if report['status'] not in ('PASS','DIAGNOSTIC_CAPTURED'): sys.exit(1)
