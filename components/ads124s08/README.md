# ADS124S08 Hub

`ads124s08` reads RTD and NTC channels through a TI ADS124S08/ADS124S06-family ADC over SPI. Configure one hub per chip-select pin and add one entry under `channels` for each temperature input.

Set `model` to `ADS124S08` or `ADS124S06`. The component validates ADS124S06 analog inputs to AIN0-AIN5 plus AINCOM and checks the runtime device ID during setup.

## Supported Channel Models

- `pt100` / `pt1000`: ratiometric RTD measurement using one IDAC source and an external reference resistor. `rtd_wires` accepts `2` or `4`; 2-wire lead resistance is not compensated.
- `ntc`: beta-model NTC in a divider where the measured ADC ratio is `Vntc / Vref`. The reference and divider supply must be ratiometric for the resistance calculation to be valid.

## Key Assumptions

The component uses ADS124S08 single-shot conversions and schedules channel reads with `set_timeout()`. Set `conversion_time` long enough for the selected `adc_data_rate` and any external RC/filter settling. `drdy_pin` is optional; without it, the component trusts `conversion_time`.

If conversions are controlled by SPI commands, the ADS124S0x `START/SYNC` pin must be low. Configure `start_sync_pin` when that pin is connected to the MCU; otherwise tie it low in hardware.

The internal ADS124S08 reference is kept on because the datasheet requires it for IDAC operation. ADC reference selection is controlled by `reference` (`REF0`, `REF1`, or `INTERNAL`). When `INTERNAL` is selected, both reference buffers are bypassed as recommended by TI.

`idac_current` must be one of the ADS124S0x hardware values: `10uA`, `50uA`, `100uA`, `250uA`, `500uA`, `750uA`, `1000uA`, `1500uA`, or `2000uA`.

## Known Gaps

Three-wire RTD compensation is not implemented. Complex NTC topologies, Steinhart-Hart coefficients, per-channel data rates, and service-triggered self-calibration should be added only after the target adapter wiring is fixed.
