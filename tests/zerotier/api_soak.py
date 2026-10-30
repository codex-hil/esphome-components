#!/usr/bin/env python3
"""Bounded encrypted API soak; JSONL evidence, no OTA or device resets.

Run from an authorized peer while the station holds the DUT lease.
"""
import argparse
import asyncio
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import time

import aioesphomeapi as api
import yaml


async def probe(host, secret, args):
    client = api.APIClient(host, 6053, None, noise_psk=secret["api_encryption_key"])
    result = {"host": host, "utc": datetime.now(timezone.utc).isoformat()}
    observations = {}
    try:
        await asyncio.wait_for(client.connect(login=True), 20)
        started = time.monotonic()
        info = await asyncio.wait_for(client.device_info(), 10)
        result["device_info_ms"] = (time.monotonic() - started) * 1000
        assert info.mac_address.lower() == args.mac.lower(), "Unexpected device MAC"
        entities, _ = await asyncio.wait_for(client.list_entities_services(), 10)
        names = {entity.key: entity.name for entity in entities}

        def update(state):
            if state.key in names and hasattr(state, "state"):
                observations.setdefault(names[state.key], []).append(state.state)

        client.subscribe_states(update)
        await asyncio.sleep(12)
        assert observations.get("ZeroTier Node ID", [None])[-1] == args.node, "Wrong Node ID"
        assert observations.get("ZeroTier Address", [None])[-1] == args.overlay, "Wrong overlay IP"
        assert observations.get("ZeroTier Configured", [False])[-1], "Not configured"
        uptime = observations.get("Uptime", [])
        assert len(uptime) >= 2 and uptime[-1] > uptime[0], "No uptime progression"
        result.update(status="PASS", uptime_seconds=uptime[-1])
    except Exception as exc:
        result.update(status="FAIL", error=type(exc).__name__)
    finally:
        try:
            await asyncio.wait_for(client.disconnect(), 5)
        except Exception:
            result["disconnect_error"] = True
    return result


async def main(args):
    secret = yaml.safe_load(args.secrets.read_text())
    deadline = time.monotonic() + args.duration
    with args.output.open("x") as out:
        while time.monotonic() < deadline:
            started = time.monotonic()
            results = await asyncio.gather(*(probe(host, secret, args) for host in (args.lan, args.overlay)))
            for result in results:
                out.write(json.dumps(result) + "\n")
            out.flush()
            await asyncio.sleep(max(0, min(args.interval - (time.monotonic() - started), deadline - time.monotonic())))


if __name__ == "__main__":
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--secrets", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lan", required=True)
    parser.add_argument("--overlay", required=True)
    parser.add_argument("--mac", required=True)
    parser.add_argument("--node", required=True)
    parser.add_argument("--duration", type=float, default=86400)
    parser.add_argument("--interval", type=float, default=60)
    args = parser.parse_args()
    if not (0 < args.duration <= 86400 and args.interval >= 15):
        parser.error("duration must be in (0, 86400], interval at least 15 seconds")
    asyncio.run(main(args))
