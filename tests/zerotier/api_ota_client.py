#!/usr/bin/env python3
"""Independent authorized-node probe. Secrets and images stay outside Git.
Run only while hil_station2.py owns the board lease and restoration window.
"""
import argparse
import asyncio
import json
from pathlib import Path
import time
import aioesphomeapi as api
from esphome.espota2 import run_ota
import yaml

p = argparse.ArgumentParser()
p.add_argument('--secrets', type=Path, required=True)
p.add_argument('--firmware', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--lan', required=True)
p.add_argument('--overlay', required=True)
p.add_argument('--mac', required=True)
p.add_argument('--node', required=True)
args = p.parse_args()
secret = yaml.safe_load(args.secrets.read_text())
result = {'status': 'RUNNING', 'lan': args.lan, 'overlay': args.overlay, 'started_unix': time.time()}

async def probe(host, seconds=15):
    client = api.APIClient(host, 6053, None, noise_psk=secret['api_encryption_key'])
    observations = {}
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
        info = await asyncio.wait_for(client.device_info(), 10)
        assert info.mac_address.lower() == args.mac.lower(), 'Wrong MAC'
        entities, _ = await asyncio.wait_for(client.list_entities_services(), 10)
        names = {e.key: e.name for e in entities}
        def update(state):
            if state.key in names and hasattr(state, 'state'):
                observations.setdefault(names[state.key], []).append(state.state)
        client.subscribe_states(update)
        await asyncio.sleep(seconds)
        assert args.node in observations.get('ZeroTier Node ID', []), 'Node ID missing'
        assert args.overlay in observations.get('ZeroTier Address', []), 'Overlay address missing'
        assert True in observations.get('ZeroTier Configured', []), 'Virtual interface not configured'
        uptime = observations.get('Uptime', [])
        assert len(uptime) >= 2 and uptime[-1] > uptime[0], 'No live uptime progression'
        return {'status': 'PASS', 'mac': info.mac_address, 'esphome': info.esphome_version,
                'observations': observations}
    finally:
        await client.disconnect()

async def dual():
    lan, overlay = await asyncio.gather(probe(args.lan), probe(args.overlay), return_exceptions=True)
    for value in (lan, overlay):
        if isinstance(value, BaseException):
            raise value
    return {'lan': lan, 'overlay': overlay}

async def wait_dual():
    deadline = time.monotonic()+120
    last = None
    while time.monotonic() < deadline:
        try:
            return await dual()
        except Exception as exc:
            last = type(exc).__name__
            await asyncio.sleep(3)
    raise RuntimeError('API recovery timeout: '+str(last))

try:
    result['before'] = asyncio.run(wait_dual())
    start = time.monotonic()
    # The API PSK also permits the official uploader's encrypted path when
    # supported; password authenticates the configured ESPHome OTA endpoint.
    code, address = run_ota(args.overlay, 3232, secret['ota_password'], args.firmware,
                            noise_psk=secret['api_encryption_key'])
    result['ota'] = {'exit_code': code, 'address': address, 'seconds': time.monotonic()-start}
    assert code == 0, 'OTA failed'
    time.sleep(5)
    result['after'] = asyncio.run(wait_dual())
    result['recovery_seconds_including_probe'] = time.monotonic()-start
    before = result['before']['overlay']['observations']['Uptime'][-1]
    after = result['after']['overlay']['observations']['Uptime'][0]
    assert after < before, 'No reboot proven by uptime'
    result['status'] = 'PASS'
except Exception as exc:
    result.update(status='FAIL', error=type(exc).__name__+': '+str(exc))
finally:
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result), flush=True)
raise SystemExit(0 if result['status'] == 'PASS' else 1)
