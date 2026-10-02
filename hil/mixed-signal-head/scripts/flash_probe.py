"""Flash only the owner-designated ESP32 after a fresh ROM MAC check."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PORT = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
EXPECTED_MAC = '68:25:dd:4c:49:e4'
base = [sys.executable, '-m', 'esptool', '--chip', 'esp32', '--port', PORT]


def run(args, name):
    result = subprocess.run(args, capture_output=True, text=True, timeout=90)
    output = result.stdout + result.stderr
    (ROOT / 'results' / name).write_text(output)
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}; inspect saved log')
    return output


identity = run(base + ['--no-stub', '--after', 'no_reset', 'read_mac'], 'flash-identity.log')
if EXPECTED_MAC not in identity:
    raise RuntimeError('ESP32 MAC mismatch; no flash performed')
artifacts = ['bootloader/bootloader.bin', 'partition_table/partition-table.bin', 'moduliq_head_probe.bin']
manifest = {n: hashlib.sha256((ROOT / 'build' / n).read_bytes()).hexdigest() for n in artifacts}
args = base + ['--baud', '460800', '--after', 'hard_reset', 'write_flash',
               '--flash_mode', 'dio', '--flash_size', '4MB', '--flash_freq', '40m']
for offset, name in zip(['0x1000', '0x8000', '0x10000'], artifacts):
    args += [offset, str(ROOT / 'build' / name)]
output = run(args, 'flash.log')
report = {'timestamp_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
          'status': 'FLASH_PASS_RUNTIME_PENDING', 'expected_mac': EXPECTED_MAC,
          'port': PORT, 'artifacts_sha256': manifest, 'command': args,
          'cpld_modified': False, 'spi_transactions': 0}
(ROOT / 'results' / 'flash.json').write_text(json.dumps(report, indent=2) + '\n')
print(output[-1000:])
