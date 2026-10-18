# Sinara Urukul support

Public source snapshot for Unified Sinara Slow Control, targeting STM System
Board and Kasli. **This is Python host support and Migen integration code, not
an ESPHome `external_components` driver.** A native ESPHome/C++ Urukul component
and autonomous controller runtime remain future work.

Implemented: CPLD protocol8 identification, RF OFF, four attenuators0..31.5dB
in0.5dB steps, readback restoring the attenuator chain, explicit simulated
STM/Kasli transports and a shared module-side `urukul_eem0` pinmux profile.
No CPLD9, DDS initialization, frequency, PLL, RF enable or RTIO support.

EEM0: SCK0, MOSI1, MISO2, address CS bits0/1/2 on lines3/4/5.
CS is an active-high binary CPLD address, not independent active-low selects.
IO_UPDATE6 and RESET/SYNC7 remain GPIO. Controller package pins, DIOT adapters,
transceiver directions and physical startup still require validation.

## Run from a clean checkout

```sh
cd support/sinara-urukul
python3 -m venv .venv
.venv/bin/python -m pip install -e .
.venv/bin/python -m unittest discover -s tests -v
.venv/bin/python -m sinara_slow.cli simulate examples/urukul.yaml --connection examples/stm.simulation.yaml
.venv/bin/python -m sinara_slow.cli simulate examples/urukul.yaml --connection examples/kasli.simulation.yaml
```

Examples have **synthetic controller CSR addresses and connections**. They are
not hardware profiles. Real transports require separately installed pyserial
or LiteX, a matching deployed ABI, an exclusively owned module and reviewed
physical wiring. Status reads write CFG; attenuation readback is intrusive.
The host GPIO ABI and integrated master ABI4 are distinct.

[Pinmux details and source references](docs/URUKUL.md).
`SlotPinMux(spi_interface, mapping="urukul_eem0")` selects the full six-line
profile. The included integration generator accepts per-slot profiles for the
archived STM baseline and logical Kasli fabric; it is not a deployable image.

## Optional integrated CSR/master test

This downloads pinned upstream source dependencies, without programming hardware:

```sh
.venv/bin/python tools/prepare_references.py
PYTHONPATH=src:work/upstream-stm/deps/migen:work/upstream-stm/deps/misoc:work/upstream-stm/deps/litex .venv/bin/python tools/test_urukul_pinmux.py
```

Use Python3.12 for this archived Migen/MiSoC integration. The test covers
segmented24/32bit transfers, three decoded CS bits, MOSI/MISO/OE and full STM
elaboration. It performs no synthesis, P&R, board access or electrical checks.
`evidence/urukul-pinmux-tests.json` records the original offline result, not HIL.
The clean-checkout unit suite skips one optional ARTIQ comparison until the
pinned references are downloaded.

## License and provenance

This source bundle is distributed under GPL-3.0-only; see [LICENSE](LICENSE).
Protocol behavior and pin assignments follow M-Labs ARTIQ (LGPL-3.0) and Sinara
HW Urukul, pinned in `evidence/source-commits.json`. Attribution and source
links are in `docs/URUKUL.md`; no ARTIQ driver or hardware design is vendored.
`provenance.json` records hashes of the published source snapshot. Downloaded
Migen/MiSoC/LiteX/STM/ARTIQ dependencies retain their upstream licenses.
