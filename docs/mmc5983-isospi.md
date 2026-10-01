# MMC5983MA addressed SPI correction

`mmc5983_spi` now defaults to mode3/100kHz. The manufacturer specifies idle-high
SCK, falling-edge data changes, and rising-edge capture. Commands use a read bit
plus six-bit address; command/data stay under one CS. The address is not shifted.
[MEMSIC Rev A, pp.4–7,13–16](https://media.digikey.com/pdf/Data%20Sheets/MEMSIC%20PDFs/MMC5983MA_RevA_4-3-19.pdf).

The driver retries identification at most five times without writes, fails on
persistent mismatch, waits 20ms around startup/reset, checks reset ID/OTP, and
checks ID before taking another measurement. Reserved status bits are rejected:
0xFF must never count as measurement-done. XYZ is fetched in a single seven-byte
burst and decoded as unsigned18-bit values, offset131072, sensitivity16384/G.
Command/data use a single transfer with 10us CS setup/hold and 100us gap. These
are conservative diagnostic margins, not a characterized IsoSPI requirement.
Temperature uses a 10ms wait and rejects invalid/not-ready status. Explicit YAML
SPI settings remain possible; endpoints and isolation-bridge POL/PHA must agree.

Hardware: owner-designated classic ESP32 MAC68:25:dd:4c:49:e4, new PCB, CPLD rev1.0
pinout and fae8341 NVM image. No CPLD programming or payload GPIO changes occurred.
Tests used the external Wizath `spi` and `addrspi` at
16cb61e697b712d1ac20be9fb2b0b0fdcc37930c and the MMC source at
5961cc9c933759ec3f0425a9652a8d47009191c4 before this patch.

Mode1 failed identification on this fixture. Mode3 produced ID0x30 on ch1/ch2.
Ch0 did not identify a sensor; rerouting to ch2 worked, but its electrical cause
has not been localized. A first identification failed on ch1 during two firmware
starts while later read-only checks succeeded. The final patched firmware
produced119 XYZ+temperature samples per channel over120s at100kHz, with238 valid
magnetic and238 valid temperature status reads and no communication errors.
Maximum consecutive axis step was0.003174G. This does not prove calibrated accuracy
or fix the historical occasional-large-error report. MMC SPI has no CRC; valid
ID/status/reserved bits cannot detect every corruption.

[HIL recipe and full structured evidence](https://github.com/codex-hil/esphome-hil/blob/fix/mmc5983-isospi/docs/mmc5983-isospi.md).
Hub I2C ID is distinct from MMC Product ID. Clear-on-read counters, RF clients,
GPIO payload, independent waveforms and power-cycle tests were outside these tests.
Verbose diagnostic UART logging produced operation-duration warnings. The driver
still uses blocking conversion waits; it is not an asynchronous polling redesign.

Software regression (pinned Nix): `bash tests/run-mmc-software.sh`. The fake bus
checks absent/transient ID, OTP/reset failure, coherent decode, identity loss and
invalid reserved output bits. These are software tests, separate from hardware.

## Diagnostic error counter

Each MMC instance has an optional `error_count` diagnostic sensor with `state_class: total_increasing` and zero decimal places. Its independent `update_interval` defaults to10min; measurement polling is unchanged. Publishing reads only the in-memory counter, never SPI, and does not clear it. The saturating32-bit counter resets on ESP32 restart (no flash persistence). It increments once per detected rejected stage: each invalid startup-ID attempt, failed reset/OTP verification, lost runtime ID, invalid/not-ready magnetic or temperature status, and invalid reserved XYZ bits. A rejected XYZ stage is counted once, not again by its caller. Recovered startup errors are included; an initial value of1 followed by no growth is distinct from runtime errors. This counts detected failures, not arbitrary plausible data corruption or physical field changes.

```yaml
sensor:
  - platform: mmc5983_spi
    mmc5983_spi_id: mag2
    error_count:
      name: "Mag2 Read Errors"
      update_interval: 10min
    # Existing XYZ/temperature entries can remain here.
```

Simulated-bus regression verifies accumulation across startup retries, lost ID, bad status and malformed XYZ, recovery without clearing the counter, and publication without SPI traffic. The physical diagnostic configuration publishes every10s to verify behavior promptly; production can use10min. HA transport has not been exercised by the offline diagnostic firmware.
