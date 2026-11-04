#!/usr/bin/env python3
"""Bounded HTTP/SCPI load for services.h; no instrument control commands."""
import argparse
import asyncio
import json
import statistics
import time
from pathlib import Path


async def exchange(host, protocol):
    reader, writer = await asyncio.open_connection(host, 8080 if protocol == "http" else 5025)
    try:
        request = b"GET /status HTTP/1.1\r\nHost: p4\r\nConnection: close\r\n\r\n" if protocol == "http" else b"*IDN?\n"
        writer.write(request)
        await writer.drain()
        if protocol == "http":
            headers = await reader.readuntil(b"\r\n\r\n")
            assert headers.startswith(b"HTTP/1.1 200 OK\r\n")
            sizes = [int(line.split(b":", 1)[1]) for line in headers.split(b"\r\n") if line.lower().startswith(b"content-length:")]
            assert len(sizes) == 1 and 0 < sizes[0] < 1024
            body = await reader.readexactly(sizes[0])
            status = json.loads(body)
            assert status["fixture"] == "zt-load-v1"
            return len(body), status
        response = await reader.readline()
        assert response == b"LAB,ESP32-P4-ZT,LOAD,1\n"
        return len(response), None
    finally:
        writer.close()
        await writer.wait_closed()


async def worker(host, protocol, duration, interval):
    start = time.monotonic()
    deadline = start + duration
    times, errors, memory = [], [], []
    payload = 0
    while time.monotonic() < deadline:
        before = time.monotonic()
        try:
            size, status = await asyncio.wait_for(exchange(host, protocol), 5)
            times.append((time.monotonic() - before) * 1000)
            payload += size
            if status:
                memory.append(status)
        except Exception as exc:
            errors.append(type(exc).__name__)
        await asyncio.sleep(max(0, interval - (time.monotonic() - before)))
    elapsed = time.monotonic() - start
    ordered = sorted(times)
    result = {"host": host, "protocol": protocol, "elapsed_seconds": elapsed,
              "pass": len(times), "fail": len(errors), "errors": errors[:20],
              "response_payload_bytes": payload, "response_payload_bps": payload * 8 / elapsed,
              "request_ms_mean": statistics.mean(times) if times else None,
              "request_ms_p95": ordered[min(len(ordered) - 1, int(len(ordered) * .95))] if ordered else None,
              "request_ms_max": max(times) if times else None}
    if memory:
        result["memory_minimum"] = {key: min(row[key] for row in memory) for key in ("internal_free", "psram_free", "zt_stack_free")}
        result["first_status"], result["last_status"] = memory[0], memory[-1]
    return result


async def main(args):
    results = await asyncio.gather(*(worker(host, protocol, args.duration, args.interval)
                                   for host in args.host for protocol in ("http", "scpi")))
    report = {"status": "PASS" if all(r["pass"] and not r["fail"] for r in results) else "FAIL",
              "kind": "application request workload, not maximum TCP throughput or ICMP RTT",
              "interval_seconds_per_worker": args.interval, "results": results}
    with args.output.open("x") as out:
        json.dump(report, out, indent=2)
        out.write("\n")
    print(json.dumps(report))
    return report["status"] == "PASS"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", action="append", required=True)
    parser.add_argument("--duration", type=float, default=60)
    parser.add_argument("--interval", type=float, default=.1)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not (0 < args.duration <= 600 and .01 <= args.interval <= 10):
        parser.error("duration must be (0,600], interval [0.01,10]")
    raise SystemExit(0 if asyncio.run(main(args)) else 1)
