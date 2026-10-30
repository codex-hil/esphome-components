# ZeroTier acceptance update — 2026-10-30

Encrypted ESPHome API over LAN and ZeroTier, actual OTA through ZeroTier and
post-OTA API recovery passed. The same Node ID and virtual address survived.
Upload took 9.01 seconds; upload-to-completed-recovery-check took 34.64 seconds,
including the 15-second API observation period (not a pure reconnect latency).
The final previously built image was reused without source changes.

- [Current HIL evidence](../benchmarks/zerotier/hil-20261030.json)
- [Independent API/OTA observations](../benchmarks/zerotier/api-ota-20261030.json)

Memory samples across the API/OTA trial show minimum free internal RAM 313,152
bytes, PSRAM 27,322,904 bytes, unused worker stack 37,580 bytes. This does not
represent the combined production HTTP/SCPI application.

A 24-hour encrypted API and UART/memory soak started on 2026-10-30. It is
**RUNNING, not passed**. A POWERON boot was observed when opening the soak UART
capture, and the first overlay API probe failed during that startup. Keep the
failure in the results; do not classify the capture as uninterrupted uptime.
The serial port stays open for the remaining capture. No automatic firmware
restoration is configured: the owner requested retaining ZeroTier firmware.

The prior 30-minute PoC test had 56/56 overlay failures while LAN passed.
On the following morning four paired rounds passed without re-flashing.
Immediately before the ESPHome trial, overlay SCPI passed but TCP/UDP/HTTP
failed in a paired round. Exact discovery/reachability cause remains unknown;
waiting alone has not been established as the fix.

Remaining checks: completed soak, Ethernet link loss/recovery, different NATs,
direct versus relay classification, CPU and sustained throughput, production
HTTP/SCPI coexistence, dedicated secure identity storage and fuzz testing.

The following report is historical; its restoration and blocked-status claims
apply only to the 2026-10-28 trials.

---

# ZeroTier ESPHome acceptance — 2026-10-28

The external component is implemented and builds in the board repository's
pinned Nix environment (ESPHome 2026.9.0 / ESP-IDF 5.5.5). End-to-end overlay
acceptance remains **blocked**, not passed.

## Observed results

- ESP32-P4 rev3.2, 32 MiB flash and 32 MiB PSRAM, IP101 Ethernet: boot passed.
- AES/GMAC-SIV/KDF known-answer tests passed on hardware.
- Existing private NVS identity survived all three boots and two LAN OTA updates.
- Controller ONLINE and assigned virtual IPv4 address were observed.
- Encrypted ESPHome API worked on LAN and returned the expected device MAC.
- Two official ESPHome OTA uploads over LAN succeeded with Noise authentication/
  encryption enabled in the client and no plaintext fallback (5.24 s / 6.23 s).
- From Optiplex, overlay ARP resolution failed; ping and API were unreachable.
  Overlay OTA was not attempted without a working API/data-plane precheck.
- Changing core optimization from ESPHome `-Os` to the PoC's `-Og` did not fix it.
  The final component retains the standard optimization settings.
- An unchanged copy of the previously passing PoC image reproduced the failure:
  five rounds of LAN TCP/UDP/HTTP/SCPI passed; all five overlay rounds failed.
  This rules out an ESPHome-only regression as the sole explanation, but does
  **not** establish the precise network/controller/peer cause.

The Optiplex interface is UP, has the expected address and MAC for its public
Node ID, and its route selects the ZeroTier interface. Its daemon is running.
A recent cached network configuration exists. These facts do not prove current
membership authorization or usable peer paths. Administrative CLI access from
this account is unavailable; no credentials or host policy were changed.

## Next required checks

1. On Optiplex, inspect `sudo zerotier-cli info`, `sudo zerotier-cli listnetworks`
   and, if needed, `sudo zerotier-cli listpeers`. Share status only, never the
   API token or private identity.
2. Confirm both existing members remain authorized in the controller; inspect
   managed addresses and flow rules. Check whether another authorized peer can
   reach the P4 during a bounded HIL window. Do not infer success from ONLINE.
3. Repeat the unchanged PoC test to re-establish the external baseline.
4. Repeat `tests/zerotier/api_ota_client.py` against ESPHome: simultaneous LAN
   and overlay encrypted API, OTA through the overlay address, then recovery
   and persistent Node ID. Keep a verified backup and the board lease through
   restoration.
5. Only after this gate: integrate into the real application YAML, measure its
   resource margin with all services, test link loss, different NATs, direct
   versus relay operation and a supervised 24-hour soak.

## Resource observations and limits

Final application image: **1,043,808 bytes**. Minimum free internal RAM:
**315,064 bytes**; minimum free PSRAM: **27,322,904 bytes**. Worker stack reserves
128 KiB; lowest unused stack observed in these diagnostic runs: 37,448 bytes.
These are not measurements of working overlay API/OTA, nor a capacity guarantee
for the existing application. The earlier standalone data-plane test reached
only 22,440 bytes unused worker stack, so the reservation was not reduced.
CPU utilization and sustained overlay throughput remain unmeasured.

The component reserves internal SAR ADC for hardware entropy. Internal ADC
sensors and touch are incompatible. Shared default NVS can be erased by
ESPHome's recovery/factory-reset path outside this adapter; dedicated identity
storage and a provisioned encryption/security profile remain production work.

## Restoration and evidence

Original `resbox16` firmware was restored and flash contents verified after
all tests, including both OTA slots. Its LAN ping and TCP ports 80/5025 passed
again. Board leases were released. Full backups, credentials, firmware and
serial logs remain private on the owning debug station.

- [Build provenance](../benchmarks/zerotier/build.json)
- [Software tests](../benchmarks/zerotier/software-tests.json)
- [HIL summary](../benchmarks/zerotier/hil.json)
- [Memory samples](../benchmarks/zerotier/memory.csv)
- [Unchanged-PoC comparison](../benchmarks/zerotier/poc-regression.jsonl)
