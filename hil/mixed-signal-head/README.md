# Mixed-signal head: evidence and minimal probe

[Bring-up diary](BRINGUP.md), [schematic review](SCHEMATIC_REVIEW.md),
[ESP-IDF raw probe](firmware/README.md), [final ESPHome HIL report](results/esphome-driver-coexistence-summary.json).

The diary retains initial failures, fixes confirmed by the owner, failed and passed
runs, instrument perturbation diagnostics, and the final hardware handoff.
GPIO OUT pin measurements are unvalidated because CH4 was disconnected; HC165
requires its hardware /PL correction. CPLD was not programmed.

JSON, UART logs and SVG views are under `results/`. Raw binary waveforms and CSV
samples are in `waveforms.tar.gz`; extract from this directory to restore the
relative paths used by the plotting scripts:

```sh
tar -xzf waveforms.tar.gz
```

The source PDF and manufacturers' datasheets are not republished; source links
remain in the review. Build directories and downloaded toolchains are excluded.
`evidence-manifest.json` records the published evidence checksums.

Hardware scripts are lab fixtures bound to the commissioned ESP32 and instrument
identities, not automatic commissioning of arbitrary hardware. Instrument access
requires the lab's `labinstruments.transport` library; Python serial requires
`pyserial`. Review the probe wiring and instrument endpoints before use.

ESP32 was handed over in ROM bootloader with test application stopped, SMU OFF/HIZ
and scope STOP. Reset launches the retained test firmware and resumes test traffic.

The Git-pinned MMC dependency in the publishable ESPHome fixture is byte-identical
to the validated local snapshot. Historical HIL configuration hashes refer to the
local-source YAML used during measurement, retained in the original reports;
changing dependency location does not claim another hardware run.
