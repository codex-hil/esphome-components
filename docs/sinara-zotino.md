# Sinara Zotino slow control for STM and Kasli

This package controls the 32-channel AD5372 DAC and eight Zotino LEDs through
the shared `sinara_gpio_endpoint` register adapter. It includes the portable
C++17 core, ESPHome hub and standard float outputs. It uses slow GPIO SPI;
it does not use ARTIQ RTIO or promise MHz acquisition or deterministic timing.

The GPIO endpoint and its Sampler sibling are included as source dependencies
and to preserve exclusive endpoint ownership checks. The original project is
Unified Sinara Slow Control. Its recorded Sampler/Urukul decisions are preserved:
Sampler uses portable C++/ESPHome and GPIO register endpoints; Urukul is limited
to CPLD8 attenuators/RF OFF, with DDS control still unimplemented. This package
adds Zotino and does not import a Urukul driver.

## Install and run the examples

Use Python 3.12+, a C++ compiler and ESPHome 2026.9.0 on Linux:

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install esphome==2026.9.0
python scripts/test-sinara-zotino.py
```

The script compiles/runs both examples with synthetic register/DAC models.
Neither example connects to a UART or a network endpoint. Build paths are
relative to the checkout; no laboratory filesystem is needed.

For a Git external component source, select the published branch or pin its
full commit SHA:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/codex-hil/esphome-components.git
      ref: publish/sinara-zotino-20261018
    components: [sinara_gpio_endpoint, sinara_zotino]
```

Start with [STM](../examples/zotino-stm-simulation.yaml) or
[Kasli](../examples/zotino-kasli-simulation.yaml). Each pinmux function uses
an explicit `{slot: 1, line: 0}` pair. Public YAML slots are 1..8, lines 0..15;
all signals must belong to the selected endpoint slot. Cross-slot routing is
not implemented. The portable STM helper uses internal indices 0..7.

## Pinout and protocol

| EEM pair | Signal | Controller direction |
|---|---|---|
| 0 | CLK | output |
| 1 | MOSI | output |
| 2 | MISO | input |
| 3 | SYNC_N DAC | output |
| 4 | LED CS_N | output |
| 5 | LDAC_N | output |
| 6 | BUSY | input |
| 7 | CLR_N | output |

DAC frames are 24-bit MSB-first, SPI mode 1. LED frames are 8-bit mode 0.
Both selects are independent active-low lines, unlike Urukul's binary address.
The map is a logical module interface, not a reviewed FPGA package-pin map.

## API and output semantics

`sinara::Zotino` in `components/sinara_gpio_endpoint/zotino_core.h` uses the
existing `SamplerRegisters` read/write/lock/delay interface; it has no ADC or
ESPHome runtime dependency.

- `initialize()` stages idle levels/OE, writes OFS0/OFS1=8192, checks CONTROL,
  rejects missing/overtemperature devices, enables thermal protection and
  verifies CONTROL. It leaves LDAC and CLR high.
- `set_dac_mu(channels, values, count)` stages 1..32 distinct channels under
  one lock, waits 2us and pulses LDAC for 1us.
- `set_voltage(channel, volts)` uses nominal 5V reference and default trims:
  -10V through 10V-20V/65536. NaN/Inf/out-of-range requests are rejected.
- `read_dac_mu()` reads the input register using READ and NOP frames; it is
  not a measurement of analog output voltage.
- `set_leds()` writes an 8-bit LED mask using its separate chip select.

ESPHome `output: platform: sinara_zotino` exposes channels 0..31. Its normalized
0..1 range maps to the bipolar DAC span: **0 means -10V**, and 1 means positive
full scale. For volts use `id(zotino0).set_voltage(0, 1.25)`;
`id(zotino0).set_leds(0xa5)` sets LEDs. Setup does not automatically zero channels.

An I/O failure stops the driver without replay. A partial read preserves the
caller's previous result. Cleanup only attempts idle levels: a write/LDAC error
can leave outputs uncertain. Initialization does not clear channel input data or
reset gain/offset trims. Recovery must restore the intended full register state
before pulsing LDAC, which also commits previously staged channel data.

## Controller integration and verification limits

Hardware transport currently runs in ESPHome Linux/POSIX host mode: STM UART
115200 using the SCG1 gateway protocol, or LiteX/Kasli TCP. It requires a reviewed
CSR/electrical profile, matching ABI and an exclusive board lock. STM gateway
RTIC/HAL and physical LVDS/translator directions still require integration.
This is not a native ESPHome port to STM32 or the Kasli CPU. Integrated ABI4 and
upstream STM firmware do not become SCG1-compatible simply by selecting YAML.
No physical boards were programmed or analog voltages measured.

The native test checks frames, channels 0/31, modes, OE, LDAC, conversions,
STM slots 0/7, Kasli addresses, missing-device detection and injected failures
at every init/write/read operation. The source-project reports preserve the
previous ESPHome/PTY and LiteX localhost model tests and HDL-only elaboration;
these are synthetic tests, not HIL. The portable example script reruns builds
and synthetic runtimes from this public checkout. Reports are in
[reports](../reports); exact imported source hashes are recorded in
[sinara-zotino-source.json](../reports/sinara-zotino-source.json).

Protocol references are pinned to M-Labs ARTIQ revision
`7d7b85f377e0c172ae3496f0c03cfb6cb85ba022`:
[zotino.py](https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/artiq/coredevice/zotino.py),
[ad53xx.py](https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/artiq/coredevice/ad53xx.py),
[eem.py](https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/artiq/gateware/eem.py).
See [license and attribution](../LICENSE.md).
