# ADS124S08 implementations — source review, 2026-09-29

Compared snapshots, not a claim about all past/future work by either author:

- `ads124s08`: [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/ads124s08).
- `ads124s08_base`: [codex-hil/esphome-hil@631bf39](https://github.com/codex-hil/esphome-hil/tree/631bf39899a496578a0cafe1fc34cdad07509158/components/ads124s08_base).

| Area | Wizath `ads124s08` | HIL `ads124s08_base` |
|---|---|---|
| Abstraction | Temperature hub: PT100/PT1000 and NTC channels | ADC hub + independent voltage sensors; temperature processing external |
| Models | ADS124S08 and ADS124S06 ID/input checks | ADS124S08 only |
| Temperature math | RTD Callendar–Van Dusen, numeric inverse below0°C; NTC beta model | Native ESPHome resistance/NTC composition example; independent RTD converter still to implement |
| Configuration | Per-channel MUX/IDAC1; gain/reference/rate shared by hub | Per-channel MUX, PGA bypass/gain, reference voltage, filter/rate, settling and IDAC1/2 routes |
| SPI | Header defaults to CPOL0/leading edge (MODE0),2MHz; schema permits override | Requires MODE1,100kHz baseline |
| Integrity | RDATA24bits; CRC and appended STATUS explicitly disabled; STATUS read separately | STATUS+24bits+CRC; reject before publishing; verify configuration registers |
| Conversion | Single-shot, common user-supplied delay(default120ms); optional DRDY checked once afterwards | External channels single-shot with rate/filter timing; internal diagnostics can poll DOUT readiness in continuous batches |
| Excitation | One routed source; IDAC2 disconnected; RTD excitation persists after last STOP | Shared magnitude, independently routed IDAC1/2; off/disconnect before switching and after result/error |
| Diagnostics | External-reference and PGA rail monitors enabled in relevant paths | Internal short/supplies/die-temperature, CRC counter, configuration/saturation/deadline handling; reference/rail monitoring not enabled by current baseline |
| Evidence | Collection schema import passed; no physical validation here | Prior module0/two-ADC MUX/CRC/internal smoke; physical IDAC current and sensor accuracy still unvalidated |

## Findings that matter before using the Wizath snapshot on this board

1. **SPI default needs correction/explicit override.** The C++ class in
   [ads124s08.h](../components/ads124s08/ads124s08.h) selects
   `CLOCK_PHASE_LEADING`. Its Python schema supplies no MODE1 default or guard.
   TI specifies DIN latched on falling SCLK and DOUT updated on rising SCLK
   (SBAS660C section9.5.1): use MODE1 with idle-low clock. An explicit YAML
   `spi_mode: MODE1` can override the inherited default; the issue is the default,
   not proof that every configuration fails.
2. **Alarm can be overwritten by success.** In `finish_channel_()`, an alarm calls
   `publish_status_(..., "alarm")` but does not return/reject the sample. If the
   subsequent math is finite, the same call publishes temperature and `"ok"`.
   CRC is also explicitly off (`REG_SYS=0`), so that implementation does not
   provide our frame-corruption rejection.
3. **Timing is manual.** `conversion_time` defaults to120ms independently of
   `adc_data_rate`. For example2.5SPS with the low-latency filter needs roughly
   406.5ms for the first conversion at the nominal clock (TI table13). Without
   DRDY, an unchanged120ms setting can read before completion. The README tells
   users to choose a sufficient delay; schema does not enforce it. Checking DRDY
   once marks the channel `not_ready` instead of waiting within a deadline.
4. **IDAC sequencing differs.** `configure_channel_()` changes INPMUX before
   disabling/rerouting a previously active source. Completion sends STOP but
   does not clear IDACMAG/IDACMUX. STOP puts the converter in standby; it does not
   disable excitation. Holding excitation may be a deliberate design choice,
   but it differs from our per-measurement pulsed-current contract. There is no
   separate configurable settling phase before START.
5. **Resistance formulas have wiring assumptions.** RTD uses
   `(code / (2^23 * gain)) * reference_resistance`, valid for the intended
   ratiometric reference-resistor circuit. NTC uses a ratiometric divider. The
   README documents this, but YAML allows internal reference without validating
   that the selected topology still satisfies the formula. Example: a100Ω RTD
   driven by500µA gives50mV; with internal2.5V reference, ratio0.02 times a
   configured430Ω reference resistor would yield8.6Ω. That is an incompatible
   configuration, not an argument against the valid ratiometric method.
6. **`rtd_wires` is descriptive in this snapshot.** It is accepted/stored but does
   not select different processing. Both2-/4-wire circuits can use the same RTD
   curve; wiring establishes lead-error rejection, and the README explicitly
   says2-wire resistance is not compensated. No hidden wire compensation should
   be inferred from that option.

The source review recommends retaining our acquisition engine for the commissioned
board and reusing/reviewing the RTD math in a separate resistance→temperature
component, with independent numeric tests. It does not certify external-sensor
accuracy of our driver. We have not flashed the Wizath variant or reproduced the
above source-level paths on physical hardware in this review.

TI reference: https://www.ti.com/lit/ds/symlink/ads124s08.pdf — sections9.4.3,
9.5.1 and table13. Findings above are based on the linked code and this datasheet.
