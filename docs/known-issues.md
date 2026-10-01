# Known issues

## MMC5983: alternating bad readings — user report

Reported by the owner on2026-09-29: approximately every other measurement can be
incorrect. **The alternating-sample issue has not been reproduced or resolved.** Exact affected
revision, I²C versus SPI variant, settings and sensor fixture remain to establish.
The report is flagged for both `mmc5983` and `mmc5983_spi` until the affected path
is identified; this does not assert both implementations have the same defect.
A Python import PASS does not establish reliable measurements.

For the next hardware session, preserve a raw trace of successive conversions,
status/data-ready and configuration, with timestamps and the active SET/RESET
sequence. Correlate wrong samples with conversion initiation/readiness and register
read order. These are investigation targets, not a diagnosed root cause; do not
hide the issue by dropping alternate samples or filtering them away.

## MMC5983 SPI startup/transport correction — 2026-10-01

[IsoSPI session and correction](mmc5983-isospi.md) establishes mode3 operation on
ch1/ch2, bounded identity retry and fail-closed validation. A final120s capture
contains119 valid samples per channel, without the reported large errors.
This does **not** close the historical intermittent-sample issue above.

## ADS124S08 from Wizath — source review

See [comparison and findings](ads124s08-comparison.md). In the pinned snapshot the
SPI default, alarm handling, CRC coverage and timing/excitation contract need
review before selecting it for the commissioned board. No automatic replacement
of `ads124s08_base` was made.
