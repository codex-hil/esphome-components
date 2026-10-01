# Katalog komponentów

Stan importu: 2026-09-30. Baseline testów: ESPHome2026.9.0, ESP32/ESP-IDF5.5.5.
PASS importu oznacza załadowanie modułów Pythona. PASS kompilacji obejmuje tylko
konkretny przykład, nie wszystkie ustawienia ani działanie sprzętu.

| Komponent | Funkcja | Schematy | Kompilacja przykładu | Źródło |
|---|---|---|---|---|
| [ad9959](../components/ad9959) | DDS, cztery kanały | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/ad9959) |
| [addrspi](../components/addrspi) | SPI z wyborem adresu na GPIO | PASS | [bridge-adc](../examples/bridge-adc.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/addrspi) |
| [addrspi2](../components/addrspi2) | Adresowane kanały SPI z nagłówkiem protokołu | PASS | [addrspi2](../examples/addrspi2.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/addrspi2) |
| [ads124s08](../components/ads124s08) | ADC + kanały temperatury z forka Wizatha | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/ads124s08) |
| [ads8166](../components/ads8166) | Wielokanałowy ADC SPI | PASS | [ads8166](../examples/ads8166.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/ads8166) |
| [as6500](../components/as6500) | Konwerter czasu TDC | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/as6500) |
| [dac8775](../components/dac8775) | DAC | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/dac8775) |
| [dacx0504](../components/dacx0504) | Rodzina DACx0504 | PASS | [dacx0504](../examples/dacx0504.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/dacx0504) |
| [hc138](../components/hc138) | Dekoder wyboru kanału SPI | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/hc138) |
| [max112xx](../components/max112xx) | Rodzina ADC MAX112xx | PASS | [max112xx](../examples/max112xx.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/max112xx) |
| [mcp3208](../components/mcp3208) | Ośmiokanałowy ADC | PASS | [mcp3208](../examples/mcp3208.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/mcp3208) |
| [mmc5983](../components/mmc5983) | Magnetometr; [zgłoszony problem odczytów](../docs/known-issues.md) | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/mmc5983) |
| [mmc5983_spi](../components/mmc5983_spi) | Magnetometr SPI; [zgłoszony problem odczytów](../docs/known-issues.md) | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/mmc5983_spi) |
| [tmc5130](../components/tmc5130) | Sterownik silnika krokowego | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/tmc5130) |
| [spi](../components/spi) | Zmodyfikowana warstwa SPI dla magistral adresowanych | PASS | [bridge-adc](../examples/bridge-adc.yaml): PASS | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/spi) |
| [tca9548a](../components/tca9548a) | Multiplekser I²C; wariant zgodności z external_components | PASS | nie testowano | [wizath/esphome@16cb61e6](https://github.com/wizath/esphome/tree/16cb61e697b712d1ac20be9fb2b0b0fdcc37930c/esphome/components/tca9548a) |
| [max6966](../components/max6966) | Sterownik wyjść LED/PWM | PASS | nie testowano | [wizath/esphome@fdd5c666](https://github.com/wizath/esphome/tree/fdd5c666286da9e90be05ef6d182b99aff3d7d51/esphome/components/max6966) |
| [ads124s08_base](../components/ads124s08_base) | ADC, kanały napięciowe MUX/IDAC i diagnostyka CRC z HIL | PASS | [bridge-adc](../examples/bridge-adc.yaml): PASS | [codex-hil/esphome-hil@631bf398](https://github.com/codex-hil/esphome-hil/tree/631bf39899a496578a0cafe1fc34cdad07509158/components/ads124s08_base) |
| [resistance_sampler](../archive/resistance_sampler/components/resistance_sampler) | Historyczny interfejs próbkowania rezystancji | nie testowano | nie testowano | [gkasprow/esphome@fecae2f7](https://github.com/gkasprow/esphome/tree/fecae2f740c597d8d5348a2e79f04ab48102ffb5/esphome/components/resistance_sampler) |
| [spi_test](../archive/spi_test/components/spi_test) | Eksperymentalny generator transakcji SPI | nie testowano | nie testowano | [wizath/esphome@fdd5c666](https://github.com/wizath/esphome/tree/fdd5c666286da9e90be05ef6d182b99aff3d7d51/esphome/components/spi_test) |
| [ethernet_lan865x](../archive/lan865x/components/ethernet) | Historyczny wariant Ethernet LAN865x | nie testowano | nie testowano | [wizath/esphome@80169059](https://github.com/wizath/esphome/tree/80169059ab6354651981ee582d2cf95b57728ead/esphome/components/ethernet) |
| [rtd](../components/rtd) | Rezystancja → temperatura PT100/PT1000; IEC 60751 | PASS | [rtd](../examples/rtd.yaml): PASS | [Rozwój lokalny](../components/rtd) |
| [moduliq_cpld_i2c](../components/moduliq_cpld_i2c) | CPLD I²C: identyfikacja/statusy/liczniki i opcjonalny ADC | PASS | [cpld-readout](../examples/cpld-readout.yaml): PASS; [cpld-gpio](../examples/cpld-gpio.yaml): PASS; [cpld-adc-flash](../examples/cpld-adc-flash.yaml): PASS | [Rozwój lokalny](../components/moduliq_cpld_i2c) |
| [moduliq_cpld_gpio](../components/moduliq_cpld_gpio) | CPLD CS3: dwa banki GPIO, DIR/OD i wspólny CFG | PASS | [cpld-gpio](../examples/cpld-gpio.yaml): PASS; [cpld-adc-flash](../examples/cpld-adc-flash.yaml): PASS | [Rozwój lokalny](../components/moduliq_cpld_gpio) |
| [moduliq_cpld_flash](../components/moduliq_cpld_flash) | CPLD CS2: współdzielona Flash, transport i acquire/release przez CS3 | PASS | [cpld-adc-flash](../examples/cpld-adc-flash.yaml): PASS | [Rozwój lokalny](../components/moduliq_cpld_flash) |
| [lmk61e2](../components/lmk61e2) | Programowalny oscylator I²C; planner częstotliwości, format i enable | nie testowano | [lmk61e2](../examples/lmk61e2.yaml): PASS | [Rozwój lokalny](../components/lmk61e2) |

## Wybór wariantów i zależności

Bazą większości importów jest `wizath/esphome:esphome26.3` przy pełnym SHA
`16cb61e697b712d1ac20be9fb2b0b0fdcc37930c` (fork deklaruje ESPHome2026.3.1).
`gkasprow/esphome:dev` zawiera scalone dodatki Wizatha na starszej bazie2025.3.0-dev;
porównanie z `wizath:dev` wykazało16 dodatkowych commitów, m.in. sterowniki DDS,
DAC, ADC, TDC, SPI MUX i magnetometru. Gałęzie i alternatywne drzewa zachowuje
[inwentarz](source-branches.json). Nie kopiujemy wycofanych składników upstream
(np.RP2040/event_emitter) tylko dlatego, że nie występują już w ESPHome2026.9.0.

- `addrspi`, `addrspi2`, `hc138`: wybieraj również zebrane `spi`.
- `spi`, `mmc5983`, `tca9548a` zastępują wbudowane komponenty; włączaj je jawnie.
- `ads124s08` i `ads124s08_base` to dwa odrębne API. Dla obecnego bridge używamy
  `ads124s08_base`; import drugiej implementacji nie nadaje jej statusu HIL.
- `max6966` pochodzi ze starszej gałęzi `developement`; przechodzi import schematu,
  ale nie został w tej kolekcji zbudowany ani sprawdzony na sprzęcie.
- Archiwalne LAN865x, spi_test i resistance_sampler są poza `components/`.
  Wymagają oddzielnej oceny zgodności/dependencji; nie są częścią baseline.

## Znane problemy

[MMC5983: co drugi odczyt błędny — zgłoszenie użytkownika, bez diagnozy](../docs/known-issues.md).
[Porównanie ADS124S08 i znalezione problemy](../docs/ads124s08-comparison.md).

## Sprzęt i autorstwo

LMK61E2: [testy rdzenia na EVM i oscyloskopie](../reports/lmk61e2-hil-2026-10-01.json); transport ESP32 wymaga osobnej walidacji. Wcześniejsze testy `spi`/`addrspi`/
`ads124s08_base` na module0 i dwóch ADC są opisane w
[raporcie HIL](https://github.com/codex-hil/esphome-hil/blob/631bf39/reports/ads124s08-mux-2026-09-29.json).
Dotyczyły MUX/CRC i diagnostyki, z IDAC wyłączonym; nie potwierdzają pomiaru
temperatury lub fizycznej dokładności prądu. Pozostałe komponenty nie mają tutaj
nadanej walidacji sprzętowej.

Oryginalne `CODEOWNERS` pozostają w źródłach i [manifeście](components.json).
Główne pochodzenie dodatków: Wizath, mirror i integracja: gkasprow; oryginalni
autorzy ESPHome zachowują swoje oznaczenia. Pełne licencje: [LICENSE.md](../LICENSE.md).

Ten plik generuje `python3 scripts/catalog.py`. Zmiany wyników należy opierać
na rzeczywistych raportach, nie na samym istnieniu kodu.
