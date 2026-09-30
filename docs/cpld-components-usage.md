# Użycie trzech komponentów CPLD Bridge

Implementacja jest przeznaczona dla kontraktu `MODULIQ/CPLD_bridge` przy
`2b0752faf76c84b76e593727a427b732e0b44b99`, gałąź `feature/rev11-project-pll`.
Bazą biblioteki jest `113a2d7ba2746eb9e3091d8c41ca1853ee88ea4a`.
Testy i kompilacje korzystają z `codex-hil/esphome-hil` przy
`631bf39899a496578a0cafe1fc34cdad07509158`: Nix, ESPHome 2026.9.0,
ESP-IDF 5.5.5. Wyniki software nie potwierdzają działania sprzętu.

## Przykłady

- [cpld-readout.yaml](../examples/cpld-readout.yaml): I²C bez SPI i bez ADC,
  RTD16 rev1.0, statusy, cztery liczniki i dwa pola cyfrowego ID.
- [cpld-rtd16.yaml](../examples/cpld-rtd16.yaml): readout oraz expander CS3,
  ADS124S08 na CS0/CS1 mode1/100kHz, DRDY jako wejścia 11/15.
- [cpld-adc-flash.yaml](../examples/cpld-adc-flash.yaml): konfiguracja do
  kompilacji, generic ADC 10-bit oraz Flash na CS2. To hipotetyczna konfiguracja,
  nie schemat gotowej rev1.1 ani firmware dla stanowiska RTD16. Nie uruchamia
  automatycznie transferów pamięci. Przykładowy przedział ID nie jest kalibracją.

Ładuj jawną listę `external_components`, lokalnie z `../components`, a przy
udostępnianiu z Git po pełnym SHA implementacji. Readout samodzielny potrzebuje
wyłącznie `moduliq_cpld_i2c` oraz magistrali I²C. GPIO potrzebuje `spi` i
`addrspi`; Flash dodatkowo instancji `moduliq_cpld_gpio`. Nie ma czwartego
publicznego komponentu. Kontekst własności GPIO/ADC/Flash znajduje się w klasie
GPIO; readout bez GPIO ma własną maszynę ADC i nie ładuje SPI.

SPI w przykładach ma dwa poziomy `addrspi`: cztery HSPI_AD wybierają GA modułu,
a dwie HSPI_sAD wybierają układ. Oba adresy są ustawiane przed CS. Dla
istniejącego okablowania z nieruchomym HSPI_AD można użyć jednego poziomu, ale
trzeba zapewnić fizyczne GA zgodne z `module_address` przed pierwszą transakcją.
I²C adres to `0x50 + module_address`, nie numer kanału SPI.

## Profil i ochrona pinów

`board_profile: rtd16_rev10` wymusza maskę wejść DRDY `0x88`. Domyślnie
`upper_input_mask` wynosi `0xFF`, aby chronić również cyfrowe ID headów.
Zmniejszenie maski wymaga świadomego odłączenia ich zewnętrznych sterowników;
bitów 3/7 nie można usunąć. Readout sprawdza PROJECT_ID=3 i REV_ID=0x10,
wyklucza DRDY z pól ID i odrzuca ADC. GPIO bez readoutu opiera się na jawnie
wybranym profilu, nie rozpoznaje hardware przez SPI.

`board_profile: generic` wymaga jawnego `upper_input_mask`; dostępna jest też
`lower_input_mask`. Profile dotyczą PCB i jego zewnętrznych sterowników.
REV_ID nie rozpoznaje możliwości HDL. ADC wymaga jawnego bloku `adc`,
rozdzielczości `bits: 8|9|10`, poprawnego obrazu z ręcznym START
(`FREE_RUNNING=false`) i właściwego toru komparatorowego.

Piny 0..7 oznaczają REG_OUT, 8..15 oznaczają REG_IN. Piny można stosować
w zwykłych `output`, `switch` i `binary_sensor` ESPHome:

```yaml
pin:
  moduliq_cpld_gpio: bridge_gpio
  number: 5
  mode:
    output: true
    open_drain: true
```

Podciągnięcia są zewnętrzne. Nie obsługujemy wewnętrznych pull-up/pull-down.
Setup odczytuje DATA/DIR/OD/CFG/STATUS, następnie tylko usuwa kierunki wyjściowe
chronionych pinów. Zmiana trybu odłącza wyjście, zachowuje DATA, ustawia OD,
a dopiero potem DIR. `write_pin(pin, value)` i `configure_pin(pin, flags)`
zwracają wynik operacji. Standardowe GPIOPin ma zapis `void`: odrzucenie kolizji
jest zgłaszane w logu i nie trafia do latcha. Jeśli aplikacja musi potwierdzić
zapis, powinna użyć bezpośredniego API zwracającego `bool`.
`read_pin(pin, value)` czyta fizyczny pad, nie DATA latch. Po rozbieżności
zapis/readback driver odmawia dalszych zmian tego banku, aby nie nadpisać innych
bitów z nieaktualnego shadow. Wymaga ponownego setup; drugi bank i cleanup CFG
pozostają dostępne.

## Readout i identyfikacja

Opcjonalne sensory liczbowe: `project_id`, `revision_id`, `power_request`,
`errin`, `upper_gpio`, `lower_gpio`, `fault_mask`, `fault_status`.
`fault_status` zawiera tylko maskę `0x8F`; niewiarygodny bit6 PLL jest pomijany.
Opcjonalny text sensor `status` podaje błędy odczytu i pomiaru ADC.

`digital_ids` zawiera `bank`, `mask`, `shift` (domyślnie0), `xor_mask`
(dom.0), tabelę `codes` oraz text sensor `sensor`. Dekodowanie to
`((pads XOR xor_mask) AND mask) >> shift`. Nieznany kod lub błąd odczytu
publikuje `unknown`. Tabela zależy od konkretnej płyty i rozszerzeń.

`error_counters` mapuje `channel: 0..3` na sensor `sensor`. Gdy skonfigurowano
choć jeden, readout wykonuje jeden burst czterech liczników i kasuje wszystkie
cztery. Bez konfiguracji liczników nie czyta ich wcale. To wartości przedziałowe
modulo256, bez sumowania i bez deklaracji `total_increasing`. Błąd burstu
publikuje NaN, nie zero, i nie jest natychmiast ponawiany; liczba utraconych
zdarzeń jest nieznana. Kolejny odczyt następuje dopiero w następnym cyklu
pollingu. Jeden adres na danej magistrali I²C może mieć tylko jedną instancję
readoutu. Inni konsumenci nie mogą czytać rejestrów clear-on-read.

ADC korzysta z tego samego `update_interval` (domyślnie1s). `timeout` wynosi
500ms; walidacja odrzuca wartości krótsze od sweepu domyślnych parametrów HDL
z marginesem. Dla innego SETTLE_CYCLES/PWM_DIVIDER/clock trzeba wybrać
odpowiednio większy timeout. Pola kanału: `channel`, sensor `raw`, text sensor
`id` i opcjonalne rozłączne, domknięte `bands` (`min`, `max`, `id`).
Nie przeliczamy kodów na omy bez schematu i parametrów dzielnika.
Zero/full-scale mogą należeć do jawnego przedziału; same nie oznaczają
obecności ani braku rozszerzenia.

Jeśli GPIO tego modułu także jest skonfigurowane, `gpio_id` jest obowiązkowe
również dla readoutu bez ADC. Walidacja sprawdza zgodność GA/profilu i jednego
właściciela. W konfiguracji przyjmujemy jedno wspólne skojarzenie transportów
na wartość GA; niezależne stanowiska z tym samym GA należy rozdzielić konfiguracjami.

GPIO działa po resecie oraz poza pomiarem ADC. Start rezerwuje cały górny bank,
zapisuje CTRL=3 i sprawdza ENABLE. Pętla odpytuje DONE co5ms, nie trzymając
magistrali między odczytami. Dopiero jeden burst16 bajtów od0x32 dostarcza
pełny zestaw ośmiu słów little-endian. Wyniki są kopiowane przed disable;
publikacja następuje po potwierdzonym CTRL=0 i zwolnieniu rezerwacji.
Podczas rezerwacji zmiany górnego GPIO są odrzucane, dolny pozostaje dostępny.
Readback padów może wtedy przedstawiać komparatory. Digital ID readoutu nie
jest dekodowane podczas aktywnego pomiaru. Jeśli disable lub jego readback
zawiedzie, komponent zachowuje rezerwację i ponawia cleanup co100ms; nowych
pomiarów nie zaczyna. Timeout/błąd konwersji daje NaN/unknown, nigdy częściową
ramkę. Shutdown próbuje wyłączyć ADC, ale utrata I²C uniemożliwia potwierdzenie.

## Flash i kontrola targetu

`gpio_id`, `target_profile`, `spi_mode` i `data_rate` są wymagane.
`target_profile` jest jawną nazwą kontraktu PCB/pamięci używanego przez aplikację,
nie bazą rozpoznawanych modeli ani automatycznym skryptem bootowania.
Flash i GPIO muszą używać kanałów2/3 jednego muxa i tego samego hostowego CS.
STATUS musi potwierdzać CS2_MODE=CS3_MODE=1. Profil RTD16 rev1.0 jest odrzucany.

Własność i CFG obsługuje wyłącznie helper GPIO; Flash nie ma drugiej kopii CFG.
`acquire(true)` wymaga, aby aplikacja wcześniej potwierdziła izolację targetu
według swojego schematu. `false` odmawia przejęcia. Przykład API dla aplikacji,
po jej jawnej procedurze zatrzymania targetu:

```cpp
uint8_t tx[] = {0x9F, 0, 0, 0};  // Tylko dla pamięci, której datasheet definiuje ten opcode.
uint8_t rx[sizeof(tx)];
bool ok = id(target_flash).transaction(tx, rx, sizeof(tx), target_ready);
```

`transaction` obejmuje acquire, jedną ramkę CS2 i release, także po błędzie
argumentów. Dla kilku ramek można użyć `acquire(target_ready)`,
`transfer(tx, rx, size)` i `release()`. Rozmiar jednej ramki:1..4096 bajtów;
`tx` nie może być null, `rx` może być null dla ramki zapisu. Transfer jest
synchroniczny, więc dla responsywnej aplikacji wybieraj krótkie ramki.
Przed każdą ramką driver sprawdza własną rezerwację i bieżący CFG/STATUS.
Każda ramka kończy się CS high. To nadal nie zwalnia pamięci: CFG[0] pozostaje
ustawiony między transferami. `release()` czyści tylko CFG[0], jednym zapisem,
z zachowaniem pozostałych bitów. Potwierdza readback i STATUS. Nie przestawia
resetu/bootu targetu. Anulowanie sesji to jawne `release()`; shutdown także
próbuje release. Reset MCU nie zapewnia release w CPLD.

Błąd potwierdzenia acquire może oznaczać udany zapis do CPLD: lease pozostaje
w helperze, driver wykonuje cleanup. Błąd release zachowuje lease/stan owned,
odmawia dalszych transferów i ponawia cleanup co100ms. Niedostępny CS3 nie
pozwala potwierdzić zwolnienia. Istniejąca nieznana własność CFG[0] jest
pozostawiana bez zmian; driver jej nie przejmuje i nie kasuje.

W trybie Flash OUT1..4 są zarezerwowane także po release. OUT3/4 nigdy nie
wracają do zwykłego GPIO. `set_target_controls(cfg_levels)` aktualizuje tylko
CFG[2:1] przy zwolnionej Flash. Jawne
`configure_target_controls(cfg_levels, direction, open_drain)` odłącza
OUT1/2, ustawia CFG/OD i przywraca podane kierunki bez zmian pozostałych pinów.
`cfg_levels` używa CFG[2]=OUT1, CFG[1]=OUT2; `direction` i `open_drain` używają
masek padów OUT1/2 (`0x06`). Ta procedura wymaga zwolnionej Flash i respektuje
`lower_input_mask`. Nie jest wykonywana automatycznie przy setup/acquire/release.

SPI nie ma ACK ani wyniku błędu w delegacie ESPHome. Readback/STATUS wykrywają
część usterek zarządzania, ale `transfer()` nie potwierdza obecności pamięci,
CRC ani poprawności danych. Datasheet, geometria, opcodes, odpytywanie BUSY,
programowanie, erase i weryfikacja danych pozostają poza tym transportem.

## Powtarzalne testy software

Z checkoutami biblioteki i przypiętego `esphome-hil` obok siebie:

```sh
./scripts/test-cpld.sh
../esphome-hil/scripts/build.sh examples/cpld-readout.yaml
../esphome-hil/scripts/build.sh examples/cpld-rtd16.yaml
../esphome-hil/scripts/build.sh examples/cpld-adc-flash.yaml
```

Skrypt testów wymaga Nix i używa jego kompilatora oraz Pythona z closure
ESPHome. `CPLD_HIL_ROOT` może wskazać inne położenie tego samego przypiętego
checkoutu. Emulator magistral jest elektrycznie obojętny; testy kompilują
rzeczywiste klasy trzech driverów oraz istniejący delegat `addrspi`.
Osobno kompilowany jest sam I²C bez definicji/include GPIO. Python waliduje
prawdziwe schematy i całe YAML, także błędne powiązania, DRDY, ADC i duplikaty.
[Raport](../reports/cpld-software-2026-09-30.json) zapisuje zależności,
rewizje, hashe źródeł/logów/firmware oraz dokładny zakres weryfikacji.

Nie wykonywano resetu, flashowania, programowania CPLD/targetu ani odczytów
ze sprzętu. Nowe drivery, ADC i Flash wymagają osobnej walidacji fizycznej.
