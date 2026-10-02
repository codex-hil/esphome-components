# Minimal ESP-IDF bring-up probe

Built and physically tested on the commissioned ESP32 using ESP-IDF 5.5.5.
It sends no SPI traffic at boot and never programs CPLD. ADC/DAC initialization
is explicit through raw UART commands, not automatic.

The published defaults leave GPIO pins at -1 and block SPI until wiring is
confirmed in `menuconfig → Moduliq head probe`. Verified bench wiring:
SCK14, MOSI12, MISO34, CS15; module address bits 2/4/13/18; chip bits 16/17.
Module 0 / chip 2 selects this head; chip 1 selects the neighbouring magnetometer.
Master SPI MODE0 / 100 kHz; head LTC6820 SLOW=HIGH and C13 installed.
These are fixture settings, not a pinout for arbitrary boards.

With ESP-IDF 5.5.5 activated, build from this directory:

```sh
idf.py set-target esp32
idf.py menuconfig
idf.py build
```

UART0 115200 commands:

```text
route 0 2
burst 04 06 C0 00
stats
```

`burst` accepts 2–9 bytes and sends the entire `[TARGET][native payload]` through
one `spi_device_polling_transmit`, hardware CS, full duplex, no DMA. RX retains
its first discard byte for diagnostics. The example reads ADC Z CH3; decode
`((RX[2] & 0x0F) << 8) | RX[3]`. `ESP_OK` means driver completion, not valid
slave response or analogue accuracy.

Address map: 00 GPIO IN, 01 GPIO OUT, 02/03/04 ADC X/Y/Z, 05 DAC.
DAC80504 external reference is physically connected through R24. Do not issue
soft reset; validated setup is CONFIG=0500, GAIN=010F and SYNC=0F00 async or
0F0F synchronous. ADC inputs and DAC output voltages need separate measurements.

HC165 /PL needs a hardware inverter. Its input readout is not validated.
Physical GPIO OUT measurements in the later ESPHome session are unvalidated
because CH4 was disconnected. See [full diary](../BRINGUP.md) for earlier
waveforms, corrected failures, register commands, instrument measurements
and explicit PASS/FAIL boundaries.
