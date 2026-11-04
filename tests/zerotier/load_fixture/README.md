# Read-only application load fixture

This is an opt-in HIL profile, not part of the installed ZeroTier component.
It serves `GET /status` on TCP 8080 and `*IDN?` / `SYST:ERR?` on TCP 5025.
No GPIO or instrument commands are implemented. It is not the production
resbox16 application or a general SCPI implementation. Each connection handles
one bounded request and closes. Each service has one 8 KiB worker task, a
512-byte request limit and socket timeouts. It binds all local IPv4 interfaces;
use only the authorized test LAN/private ZeroTier network for the bounded run.

Add the following to the existing verified private P4 YAML (retain its NVS and
OTA layout, Ethernet settings, Noise API credentials and identity):

```yaml
esphome:
  includes:
    - load-services.h  # copy services.h alongside the private YAML
  on_boot:
    priority: -100
    then:
      - lambda: zt_load_fixture::start();
```

For compiler stack reports add `target_compile_options(${COMPONENT_LIB}
PRIVATE -fstack-usage)` to the **isolated diagnostic copy** of the IDF
component CMakeLists.txt before building. GCC `.su` files report individual
frames, not whole call-chain bounds or runtime high-water marks.

Build using the board's pinned Nix ESPHome/IDF environment. Acquire both board
leases on station2 and verify MAC through the encrypted API before OTA.
Keep the known-good ESPHome ZeroTier image available for OTA restoration.
This restores the existing ZeroTier firmware, not the original resbox16 image.

Run `probe.py --host LAN_IP --host OVERLAY_IP --duration 60 --interval 0.1
--output NEW_RESULT.json` with the pinned Nix Python. The four concurrent
workers verify response content, request latency, failures and memory samples.
Run the encrypted `../api_soak.py` in parallel to check API coexistence.
Response-payload bit rate is workload goodput, not maximum TCP/UDP throughput.
CPU utilization is not measured by this fixture. Preserve failures and before/
after uptime. Remove test services by restoring known-good ZeroTier firmware.
