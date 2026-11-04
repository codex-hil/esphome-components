# Concurrent application load — 2026-11-04

PASS for a bounded, read-only laboratory fixture. The ESPHome external component
ran HTTP `/status` (8080), SCPI `*IDN?` (5025) and encrypted native API together
through Ethernet and ZeroTier. No GPIO or instrument control was implemented.
This is not acceptance of the complete resbox16 application.

The pinned Nix build produced a diagnostic image with the unchanged ZeroTier
adapter and upstream patch, plus fixture tasks and `-fstack-usage` reports.
The same partition table and NVS identity were retained. LAN OTA installed the
fixture; the known-good ESPHome ZeroTier image was restored afterward.

## Results

| Workload | Successful requests | Failures |
| --- | ---: | ---: |
| 30 seconds, four workers, 0.2 s pacing | 590 | 0 |
| 60 seconds, four workers, 0.02 s pacing | 10954 | 0 |
| Encrypted API observations, concurrent 150 s window | 20 | 0 |

High-load p95 request latency: LAN HTTP 4.34 ms / SCPI 4.23 ms;
ZeroTier HTTP 22.01 ms / SCPI 23.04 ms. Outliers reached 1.30 seconds over
ZeroTier; do not claim deterministic latency. Each request uses a new TCP
connection. These are application transaction times, not ICMP RTT or maximum
link throughput. CPU utilization was not measured.

Minimum **sampled** free internal heap: 269491 bytes; PSRAM: 28240416 bytes.
Final adapter RX/TX queue-drop counts remained zero. API uptime progressed
without decreases throughout its observation window. The separate reboot for
restoring the known-good image is intentional.

Eight malformed/unsupported HTTP/SCPI cases were rejected and followed by
successful valid requests. A deterministic corpus of 36 random/truncated UDP
packets sent to the physical ZeroTier port was followed by successful services
on both interfaces. This is a smoke test, not protocol fuzzing qualification
or proof of membership/ACL enforcement.

## Stack findings

GCC individual stack frames: Multicaster::send 57040 bytes, Packet::compress
36480 bytes, several network/configuration functions about 32 KiB, ECC::verify
11408 bytes. Multicaster::send calls compression on one path. Compiler frame
reports do not establish maximum simultaneous stack usage across all paths.

This short trial left at least 37576 bytes unused in the 128 KiB node stack.
The earlier day-long run reached only **11032 bytes** unused; its lower margin
remains the applicable observation. Do not shrink the stack based on this
short test. A larger reserved stack or reducing temporary upstream buffers
requires a separate reviewed change and repeat HIL; neither was silently
included here. Full production load and longer protocol-path coverage remain.

[Structured evidence](../benchmarks/zerotier/load-20261104/summary.json),
[reproducer](../tests/zerotier/load_fixture/README.md).
