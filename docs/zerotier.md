# Native ZeroTier on ESP32-P4 (experimental)

External component `components/zerotier`; example `examples/zerotier-p4-ethernet.yaml`.
Requires ESPHome 2026.9.0, ESP-IDF 5.5.5 and working PSRAM. This is an endpoint,
not a router or bridge. The example uses the verified Waveshare P4 ETH PoE
IP101 wiring; Ethernet configuration stays in the board YAML.

```yaml
external_components:
  - source:
      type: local
      path: /path/to/esphome-components/components
    components: [zerotier]

zerotier:
  network_id: !secret zerotier_network_id
  ethernet_id: eth
  storage: nvs
  interface: ethernet
  entropy: sar_adc
  node_id:
    name: ZeroTier Node ID
  address:
    name: ZeroTier Address
  connected:
    name: ZeroTier Configured
```

Quote the 16-digit network ID in `secrets.yaml`. Authorize the logged public
Node ID in your private controller. No controller API token belongs on the
board. The `connected` entity means the virtual interface has accepted an
address and the physical Ethernet link is available; it is not an end-to-end
connectivity test. Configure Home Assistant with the virtual IP explicitly;
multicast discovery across ZeroTier is not implemented.

Keep API encryption and OTA authentication enabled, as in the example.
Backup and independently identify the board before flashing. Keep partition
layout unchanged during upgrades: the private identity lives in the default
NVS partition, namespace `zerotier`. Erasing NVS creates a different node and
requires new authorization. Backups contain that private identity.

## Architecture

Uses the genuine upstream ZeroTierOne Node C API at immutable revision
`899352e38405968516bb12a770f0ac02f6058fa8`, with `upstream.patch` applied during
code generation. ESPHome's Git cache downloads the revision once; generated
sources live under the build directory. No ESPHome fork or private socket
stack is required. Git and network access are needed for the initial source
fetch; offline builds must prefetch this pinned checkout.

One FreeRTOS task owns the node. UDP/9993 binds to the physical Ethernet IPv4
address. A separate lwIP netif receives virtual Ethernet frames; bounded TX
queues avoid calling the node from `tcpip_thread`. Netif updates and removal
run through lwIP synchronization. Standard BSD sockets, including existing
ESPHome API and OTA listeners, can receive connections on the virtual IP.
The physical Ethernet remains the default route.

Only one private network and one IPv4 address are supported. The controller
must provide a prefix /1 through /30, MTU >= 1280 and at most the canonical
connected route matching that address. Other managed routes, IPv6, VLAN,
bridging and subnets overlapping physical Ethernet are rejected. Virtual
MTU is 1280. Overlay destination addresses are rejected as transport paths.
No global DNS, default route or DHCP changes are made.

Ethernet link/IP changes cause automatic UDP rebinding and virtual interface
updates. Fatal node/storage errors stop only this component; they do not erase
identity or deliberately reboot the whole device. Inspect the error log before
restarting. Existing private identity cannot silently be replaced by a new one
following a failed NVS read or write.

## Resources and entropy

The worker reserves **128 KiB of internal task stack**. It requires at least
8 MiB free PSRAM before starting. It enables C++ exceptions, RTTI, PSRAM-backed
malloc and external BSS in the firmware configuration. These settings affect
allocation in the whole application and must be checked with other components.
The build also enables lwIP IPv6 definitions because upstream `InetAddress`
uses `sockaddr_in6` unconditionally. This does not implement IPv6 in the virtual
interface; its assigned addresses and transport remain restricted to IPv4.

The earlier standalone ESP-IDF PoC measured a 1,087,344-byte firmware,
264,020 bytes minimum free internal RAM, 27,325,008 bytes minimum free PSRAM,
and 22,440 bytes minimum unused worker stack. These are **not ESPHome results**;
API encryption, entities and other components consume additional memory.

The current port reserves the SAR ADC entropy source using ESP-IDF's bootloader
random enable/disable API. Internal ADC and touch components are rejected.
External SPI/I2C ADCs do not use this resource. Other custom code using SAR ADC
must also be excluded; the schema cannot detect arbitrary C++ code.

## Upstream changes and license

Adapter/wrapper code is MPL-2.0. Retain upstream `LICENSE.txt`,
`LICENSE-MPL.txt` and bundled third-party notices in generated sources and
review their distribution obligations before shipping a product. The source
preparation retains these files; it does not relicense upstream code.

The patch supplies ESP-IDF pthread synchronization and hardware RNG, removes
unsupported desktop services/multipath dependencies, places the large static
network configuration in external BSS and yields during identity proof-of-work.
Cryptographic algorithms, proof-of-work size, membership checks and network
rules remain upstream. The ESP/newlib byte-order macros are explicitly mapped
to compiler definitions: without this correction GMAC selected an incorrect
counter encoding. AES/GMAC/KDF known-answer tests run before starting the node.

## Security and validation limits

NVS persistence is not encryption at rest. The tested hardware has neither
flash encryption nor secure boot enabled. Use an appropriate provisioned
production security profile before claiming resistance to physical key theft.
No private identity or API/OTA credential is published by this component.
Any existing service bound to all interfaces may become reachable via ZeroTier;
authorize members and apply controller rules accordingly. The example adds no
unauthenticated HTTP, SCPI or flashing/debug service.

Standalone PoC data-plane tests passed over ZeroTier and LAN, including TCP,
UDP, HTTP, SCPI, ping and identity persistence. ESPHome build/HIL/API/OTA results
are recorded separately; do not infer them from the standalone PoC. Different-
NAT communication, direct-versus-relay classification, sustained throughput,
24-hour stability, corruption/fuzz testing and production resource headroom
remain acceptance work unless a later evidence file explicitly records them.
