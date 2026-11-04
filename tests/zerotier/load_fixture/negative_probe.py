#!/usr/bin/env python3
"""Small deterministic malformed-input HIL corpus; not a fuzzing qualification."""
import argparse
import asyncio
import hashlib
import json
import random
import socket
import time
from pathlib import Path
from probe import exchange

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--lan', required=True)
p.add_argument('--overlay', required=True)
p.add_argument('--output', type=Path, required=True)
args = p.parse_args()
results = []
for host in (args.lan, args.overlay):
    for protocol, port, payload, expected in (
        ('http', 8080, b'GET /missing HTTP/1.1\r\nHost: test\r\n\r\n', b'HTTP/1.1 404'),
        ('http', 8080, b'X' * 600, b''),
        ('scpi', 5025, b'UNDEFINED?\n', b'-113,'),
        ('scpi', 5025, b'X' * 600, b''),
    ):
        result = {'host': host, 'protocol': protocol, 'input_bytes': len(payload)}
        try:
            with socket.create_connection((host, port), timeout=5) as sock:
                sock.sendall(payload)
                try:
                    received = sock.recv(1024)
                except ConnectionResetError:
                    received = b''
                assert received.startswith(expected) if expected else received == b''
            result['rejection'] = 'PASS'
            asyncio.run(asyncio.wait_for(exchange(host, protocol), 5))
            result['recovery'] = 'PASS'
        except Exception as exc:
            result.update(status='FAIL', error=type(exc).__name__)
        results.append(result)

rng = random.Random(20261104)
corpus = [rng.randbytes(n) for n in (0, 1, 15, 16, 27, 28, 64, 512, 1400) for _ in range(4)]
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    for payload in corpus:
        sock.sendto(payload, (args.lan, 9993))
        time.sleep(.01)
# Valid application traffic is the postcondition, not the UDP send result.
recovery = []
for host in (args.lan, args.overlay):
    for protocol in ('http', 'scpi'):
        try:
            asyncio.run(asyncio.wait_for(exchange(host, protocol), 5))
            recovery.append({'host': host, 'protocol': protocol, 'status': 'PASS'})
        except Exception as exc:
            recovery.append({'host': host, 'protocol': protocol, 'status': 'FAIL', 'error': type(exc).__name__})
report = {'scope': 'bounded malformed application requests and 36 deterministic random/truncated outer UDP datagrams; not exhaustive protocol fuzzing',
          'application_cases': results, 'udp_packets_sent': len(corpus),
          'udp_corpus_sha256': hashlib.sha256(b''.join(corpus)).hexdigest(),
          'post_udp_recovery': recovery}
with args.output.open('x') as out:
    json.dump(report, out, indent=2)
    out.write('\n')
print(json.dumps(report))
raise SystemExit(0 if all(x.get('recovery') == 'PASS' for x in results) and all(x['status'] == 'PASS' for x in recovery) else 1)
