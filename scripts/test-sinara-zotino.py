#!/usr/bin/env python3
"""Build/run portable synthetic Zotino examples. No device or lab network I/O."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,'-m','unittest','discover','-s','tests','-p','test_zotino.py','-v'],cwd=ROOT,check=True)
results={}
for controller in ('stm','kasli'):
    name=f'zotino-{controller}-simulation'
    config=ROOT/'examples'/f'{name}.yaml'
    subprocess.run([sys.executable,'-m','esphome','compile',str(config)],cwd=ROOT,check=True)
    binary=ROOT/'examples/.esphome/build'/name/'.pioenvs'/name/'program'
    with tempfile.TemporaryFile(mode='w+') as log:
        p=subprocess.Popen([str(binary)],stdout=log,stderr=subprocess.STDOUT)
        try:
            p.wait(timeout=3.5)
            raise AssertionError(f'Runtime exited: {p.returncode}')
        except subprocess.TimeoutExpired:
            p.terminate();p.wait(timeout=3)
        finally:
            if p.poll() is None:p.kill();p.wait()
        log.seek(0);text=log.read()
    assert 'SIMULATED_WRITE_OK' in text,text
    assert 'was marked as failed' not in text,text
    results[controller]='PASS'
print(json.dumps({'scope':'portable examples, synthetic GPIO/DAC models','hardware_tested':False,'results':results},indent=2))
