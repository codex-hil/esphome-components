#!/usr/bin/env python3
"""Station-specific bounded test lease. Restores all regions affected by OTA.
Private run directory must contain flash-manifest.json and its hashed binaries.
The independent client writes client-done.json after completing its tests.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import time
import serial

os.umask(0o077)
assert socket.gethostname() == 'codex-hil-debug-station2'
root = Path(sys.argv[1]).resolve()
assert not (root/'result.json').exists(), 'Use a fresh run directory'
port = '/dev/serial/by-id/usb-1a86_USB_Single_Serial_5C93016778-if00'
locks = []
for name in ('esp32p4-5c93016778', 'serial-usb-11-1'):
    lock = open(Path.home()/'.cache'/f'{name}.lock', 'a')
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    locks.append(lock)
backup = Path.home()/'.local/share/esp32p4-backups/5C93016778/20261028T134346Z/flash-1.bin'
assert hashlib.sha256(backup.read_bytes()).hexdigest() == 'e161b75dc4e1741876376db2bbc4b7c71fc9ae8ad5c5c73f3b34d77f1c1c05d1'
# This test uses a 16 MiB partition layout. Restore both app slots, NVS and OTA
# selection, including app1 written by the independent OTA client.
size = 0x1000000
restore = root/'restore-prefix.bin'
restore.write_bytes(backup.read_bytes()[:size])
files = json.loads((root/'flash-manifest.json').read_text())
for item in files:
    data = (root/item['file']).read_bytes()
    assert hashlib.sha256(data).hexdigest() == item['sha256']
    assert 0 <= item['offset'] < item['offset']+len(data) <= size
base = [sys.executable, '-m', 'esptool', '--chip', 'esp32p4', '--port', port, '--baud', '921600']
def run(args, name):
    with open(root/name, 'w') as out:
        subprocess.run(base+args, stdout=out, stderr=subprocess.STDOUT, check=True, timeout=300)
def capture(name, seconds, client=False):
    uart = serial.Serial(port=None, baudrate=115200, timeout=.2)
    uart.dtr = False
    uart.rts = False
    uart.port = port
    uart.open()
    try:
        uart.rts = True
        time.sleep(.1)
        uart.rts = False
        data = bytearray()
        deadline = time.monotonic()+seconds
        with open(root/name, 'wb') as log:
            while time.monotonic() < deadline:
                part = uart.read(4096)
                data.extend(part)
                log.write(part)
                log.flush()
                if client and (root/'client-done.json').exists():
                    break
                if client and re.search(rb'Guru Meditation|abort\(\)|Stack smashing|Task watchdog got triggered', data):
                    break
        return data.decode(errors='replace')
    finally:
        uart.close()

run(['flash-id'], 'identity.log')
identity = (root/'identity.log').read_text()
assert 'e8:f6:0a:e7:fe:3f' in identity and '32MB' in identity
run(['get-security-info'], 'security.log')
security = (root/'security.log').read_text()
assert 'Secure Boot: Disabled' in security and 'Flash Encryption: Disabled' in security
run(['verify-flash', '0', str(restore)], 'current-flash-verify.log')
result = {'status': 'RUNNING', 'original_restored': False, 'restore_bytes': size}
print('Identity and 16 MiB current flash verified; lease acquired', flush=True)
try:
    trial = Path.home()/'.local/share/zerotier-p4-hil/hil-20261028-03/poc-identity-nvs.bin'
    assert trial.stat().st_size == 0x4000
    run(['--after', 'no-reset', 'write-flash', '0x9000', str(trial)], 'trial-identity.log')
    args = ['--after', 'no-reset', 'write-flash']
    for item in files:
        args += [hex(item['offset']), str(root/item['file'])]
    run(args, 'flash.log')
    (root/'flashed.json').write_text(json.dumps({'status': 'FLASHED', 'test_window_seconds': 720}))
    print('ESPHome flashed; independent API/OTA client window open', flush=True)
    text = capture('esphome-serial.log', 720, client=True)
    result['panic'] = bool(re.search(r'Guru Meditation|abort\(\)|Stack smashing|Task watchdog got triggered', text))
    result['node_ids'] = re.findall(r'(?:node=|Node ID: )([0-9a-f]{10})', text)
    result['overlay_ips'] = re.findall(r'(?:overlay=|Overlay address: )([0-9.]+)', text)
    result['memory_lines'] = [re.sub(r'\x1b\[[0-9;]*m', '', line) for line in text.splitlines() if 'heap_internal=' in line]
    result['client'] = json.loads((root/'client-done.json').read_text()) if (root/'client-done.json').exists() else {'status': 'NOT_COMPLETED'}
    result['status'] = 'PASS' if not result['panic'] and result['node_ids'] and set(result['node_ids']) == {'6e31ab38dc'} and result['client']['status'] == 'PASS' else 'FAIL'
except Exception as exc:
    result.update(status='ERROR', error=type(exc).__name__+': '+str(exc))
finally:
    print('Restoring original firmware and both OTA slots', flush=True)
    try:
        run(['--after', 'no-reset', 'write-flash', '0', str(restore)], 'restore.log')
        run(['--after', 'no-reset', 'verify-flash', '0', str(restore)], 'restore-verify.log')
        text = capture('restored-boot.log', 25)
        result['original_restored'] = 'Project name:     resbox16' in text
        result['restore_digest_match'] = 'Verification successful' in (root/'restore-verify.log').read_text()
    except Exception as exc:
        result['restore_error'] = type(exc).__name__+': '+str(exc)
    (root/'result.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result), flush=True)
sys.exit(0 if result['status'] == 'PASS' and result['original_restored'] and result['restore_digest_match'] else 1)
