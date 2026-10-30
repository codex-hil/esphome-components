# esphome-components

Public ESPHome external components for MODULIQ and Sinara.
The earlier component collection remains on
[`feature/mixed-signal-head`](https://github.com/codex-hil/esphome-components/tree/feature/mixed-signal-head)
and [`collection/initial-import`](https://github.com/codex-hil/esphome-components/tree/collection/initial-import).
This `main` branch provides the Sinara Sampler driver and shared GPIO endpoint.

## Sinara Sampler

- Eight LTC2320-16 ADC channels, PGIA gains ×1/10/100/1000, v2.1/v2.2 voltage conversion.
- STM Sys Board gateway UART or Kasli LiteX TCP register transport, with endpoint/pin mapping in YAML.
- One acquisition for all eight ESPHome voltage sensors; partial-frame errors publish NaN and stop acquisition.
- Portable C++17 core, STM Rust gateway core and eight-pair Kasli GPIO gateware profile.

**Current ESPHome hardware transport runs on a Linux host.** This is not a
native ESPHome runtime for STM32 or Kasli. Physical board and ADC validation
remain pending; examples use explicit synthetic simulation profiles.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/codex-hil/esphome-components
      ref: main  # Pin a commit for reproducible deployments.
    components: [sinara_gpio_endpoint, sinara_sampler]
```

Complete examples: [STM](examples/sinara-sampler-stm-simulation.yaml),
[Kasli](examples/sinara-sampler-kasli-simulation.yaml).
All logical pins use `{slot: 1, line: 3}` coordinates; they are not FPGA
package pin numbers. Details: [Sampler usage and limitations](docs/sinara-sampler.md).

## Reproduce software checks

Requires Linux, Python 3.12+, Git, a C++17 compiler and Cargo.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-sinara-test.txt
PATH="$PWD/.venv/bin:$PATH" bash scripts/test-sinara.sh
.venv/bin/python scripts/generate-sinara-sampler-kasli.py
```

The tests compile and run ESPHome 2026.9.0, use a PTY for STM and the actual
LiteX server on localhost for Kasli, and inject transfer failures. ADC values
are synthetic. No physical ports, FPGA programming or laboratory network
connections are used. Reports: [software](reports/sinara-sampler-software.json)
and [HDL generation](reports/sinara-sampler-kasli-elaboration.json).

Licenses: [GPL-3.0-only for C++ runtime; MIT for Python/Rust/docs](LICENSE.md).

## Sinara Urukul

[Urukul CPLD8 support and STM/Kasli pinmux](support/sinara-urukul): Python host
code, portable Migen pinmux, simulation examples and offline tests. This is
controller integration support, not a native ESPHome C++ component. No hardware
validation or DDS/frequency/RF-enable support is claimed.

## Native ZeroTier on ESP32-P4 (experimental)

[External component and configuration](docs/zerotier.md),
[example YAML](examples/zerotier-p4-ethernet.yaml),
[HIL results and remaining checks](docs/zerotier-blockers.md).
The ESP-IDF node uses system lwIP and persistent NVS identity. Build, Ethernet,
controller join, encrypted API over LAN and ZeroTier, and OTA through ZeroTier
passed. A 24-hour stability test is running; intermittent reachability and
production acceptance checks remain open. No production-readiness claim.
