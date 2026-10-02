"""Bounded UART capture; commands are explicit and recorded. No CPLD access."""
import argparse
from pathlib import Path
import time
import serial

p = argparse.ArgumentParser()
p.add_argument('--seconds', type=float, default=5)
p.add_argument('--command', action='append', default=[])
p.add_argument('--output', default='runtime.log')
a = p.parse_args()
if not 0 < a.seconds <= 60:
    p.error('seconds must be 0..60')
if Path(a.output).name != a.output:
    p.error('output must be a filename')
s = serial.Serial()
s.port = '/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0'
s.baudrate = 115200
s.timeout = .1
s.dtr = False
s.rts = False
s.open()
out = bytearray()
try:
    time.sleep(.2)
    for cmd in a.command:
        if '\r' in cmd or '\n' in cmd:
            raise ValueError('one line per command required')
        out.extend(f'HOST> {cmd}\n'.encode())
        s.write((cmd + '\n').encode('ascii'))
        s.flush()
        until = time.monotonic() + .25
        while time.monotonic() < until:
            out.extend(s.read(8192))
    until = time.monotonic() + a.seconds
    while time.monotonic() < until:
        out.extend(s.read(8192))
finally:
    s.close()
text = out.decode(errors='replace')
(Path(__file__).resolve().parents[1] / 'results' / a.output).write_text(text)
print(text[-5000:])
