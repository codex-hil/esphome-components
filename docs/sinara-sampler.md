# Sinara Sampler over STM/Kasli GPIO endpoints

The Sampler component uses slow SPI mode 0 implemented through GPIO registers.
It requires neither RTIO nor a dedicated ADC FPGA engine. GPIO writes over a
serial/TCP link are substantially slower than local SPI; the polling interval
is a request, not a guaranteed sample rate. MHz acquisition is outside this
implementation's scope.

## YAML and pin mapping

Use one exclusive `sinara_gpio_endpoint` and one `sinara_sampler` hub. The
endpoint selects `controller: stm_sys_board` or `kasli`, explicit GPIO register
addresses, electrical input/output capabilities and `mode: simulation` or
`hardware`. Public `slot` numbers are 1..8. STM word addresses are
`20*(slot-1)+{0,1,2}` for output/input/OE. Kasli addresses are 32-bit-aligned
byte addresses from the actual build's CSR map; its ABI address is separate.

The hub provides `hardware_revision: v2.1|v2.2`, eight `gains` in channel order,
`update_interval` and eight mandatory pin references. The primary EEM routes are:

| Function | Logical EEM line | Controller direction |
|---|---:|---|
| ADC clock | 0 | Output |
| ADC data | 1 | Input |
| SDR, held high | 2 | Output |
| CNV | 3 | Output |
| PGIA clock | 4 | Output |
| PGIA data | 5 | Output |
| PGIA readback | 6 | Input |
| PGIA latch | 7 | Output |

For example, `cnv: {slot: 1, line: 3}`. Every pin must belong to the endpoint's
slot; cross-slot routing, duplicate functions and incompatible electrical
capabilities are rejected. Each channel sensor uses `platform: sinara_sampler`,
`sampler_id` and a channel index 0..7.

`pinmux` assigns the driver's functions to logical GPIO bits and configures OE.
It does not program a dynamic FPGA crossbar or change I/O standards. The
loaded FPGA design must already expose these pins as GPIO. The driver will
not switch an unrelated SPI master's offline register in incompatible gateware.

## Protocol and physical integration

Hardware mode uses existing `sinara-gpio-v1`/SCG1 register identity (`0x53434731`),
not an invented wire protocol. STM needs `serial_path` at 115200 baud. Kasli
needs numeric IPv4 `host_address`, `tcp_port` (default1234) and `abi_address`.
Both require `lock_path` shared with other tools for the board and
`electrical_profile_reviewed: true` with actual pad/adapter capabilities.
Examples' addresses and capabilities are synthetic, not hardware profiles.

The STM service core is in [firmware/sinara-stm-gateway](../firmware/sinara-stm-gateway).
It needs RTIC/HAL, bounded FPGA accesses, framing/UART ownership and build-specific
ABI attestation; it is not deployable firmware. Its `stdio_simulator` is a
host-only memory fixture. Unmodified ELHEP upstream STM gateware and the other
project's integrated ABI4 do not automatically implement SCG1. STM LVDS output
directions may be physically fixed; software OE alone cannot make them inputs.
A reviewed build/adapter is required for the Sampler directions.

Kasli target generation is in [gateware/kasli_uart_gpio.py](../gateware/kasli_uart_gpio.py).
`--pin-profile sampler_eem0` exposes eight pairs with six OBUFDS and two IBUFDS,
sharing GPIO CSRs. The default legacy profile still exposes six Urukul signals.
[The generator](../scripts/generate-sinara-sampler-kasli.py) uses synthetic EEM0
mapping for HDL elaboration only; it does not run synthesis/P&R or load a board.
For a physical build, the CLI requires a reviewed JSON pad map containing
`reviewed: true` and `pins` mapping each function to `[positive, negative]`
connector names. This does not replace electrical or timing validation.

## Acquisition and error behavior

The driver writes a16-bit PGIA register, two bits per channel, MSB first, then
latches it on a rising edge and waits10ms. This delay is a conservative policy,
not a measured settling specification. PGIA readback is not implemented; gain
state becomes known after successful writes, not after physical verification.

CNV is held high for at least1us, followed by at least1us wait. The reference
ARTIQ sequence uses30ns/450ns. Full acquisition clocks128 bits in channel order
7..0 and returns signed16 samples in channel order0..7. Conversion is
`raw * span / 65536 / gain`, where span is20.0V (v2.1) or20.48V (v2.2).
No individual channel offset/gain calibration is applied.

The endpoint is locked throughout the frame and uses a bounded transaction
deadline. Writes with uncertain outcomes are never replayed automatically.
LiteX writes have no reply, so the Kasli adapter checks ordered GPIO readback
before advancing. A failed complete acquisition leaves the C++ output vector
unchanged; ESPHome publishes NaN and stops the hub until restart. A busy
endpoint skips that polling attempt. Unrelated GPIO bits are preserved.

The shared endpoint currently includes a Zotino helper model as an internal
dependency. This publication exposes the Sampler component and tests it; it
makes no Zotino component or physical validation claim.

## Software validation and provenance

`bash scripts/test-sinara.sh` checks C++ signed extrema, channel/bit order,
asymmetric gain words, references, pin remapping, both address layouts,
busy handling and every injected I/O failure point in init/acquisition.
It tests the actual ESPHome runtime in both simulation profiles, the STM
protocol over a PTY, and a real LiteX RemoteServer with a synthetic ADC model.
Negative YAML cases cover pin overlap/range/slot, direction capabilities,
gains, duplicate sensors and shared endpoints. All network tests use localhost.

`python scripts/generate-sinara-sampler-kasli.py` elaborates HDL using pinned
Migen/LiteX dependencies. None of these checks establishes hardware operation,
FPGA timing closure, ADC accuracy or a native STM32/Kasli ESPHome runtime.

Protocol references: [ARTIQ Sampler](https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/artiq/coredevice/sampler.py),
[ARTIQ EEM mapping](https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/artiq/gateware/eem.py),
[ELHEP STM endpoint](https://github.com/elhep/stm_system_board_gateware/tree/46f14f81d6a29c664f72d8c7373f4d346b43970d).
Original local file hashes are in [provenance](../reports/sinara-sampler-provenance.json).
