# Trzy niezależne drivery CPLD Bridge

`moduliq_cpld_i2c`, `moduliq_cpld_gpio` i `moduliq_cpld_flash` obsługują tylko
własne bloki CPLD. Nie konfigurują, nie odpytują i nie interpretują driverów
ADC temperatury Texas Instruments. Inne urządzenia SPI pozostają osobnymi
komponentami, z adresowaniem, trybem i metodą detekcji gotowości wybraną w YAML.
Żaden z tych trzech driverów nie przypisuje bitom GPIO znaczenia DRDY.

I²C jest dostępne na wszystkich płytkach. GPIO na CS3 i Flash na CS2 zależą
od konfiguracji konkretnego CPLD; niektóre płytki nie udostępniają expandera,
a inne mają Flash. Wybór należy do aplikacji YAML po odczycie PROJECT_ID.
Drivery nie rozpoznają typu płytki, nie zmieniają strapów trybu i nie wybierają
funkcji automatycznie na podstawie PROJECT_ID, REV_ID ani GA.

Kontrakt transportu audytowano przy `MODULIQ/CPLD_bridge` commit
`2b0752faf76c84b76e593727a427b732e0b44b99`. Przyjęte w starszym handoffie
powiązanie z RTD16/DRDY nie jest polityką tych generic driverów; bieżący zakres
właściciel określił jako osobne drivery wybierane w YAML.
Środowisko: `codex-hil/esphome-hil` przy
`631bf39899a496578a0cafe1fc34cdad07509158`, Nix, ESPHome2026.9.0, ESP-IDF5.5.5.

## Wybór w YAML

- [cpld-readout.yaml](../examples/cpld-readout.yaml): wyłącznie I²C, bez SPI,
  bez założeń o PROJECT_ID i bez ADC identyfikacyjnego.
- [cpld-gpio.yaml](../examples/cpld-gpio.yaml): jawnie włączony GPIO, readout
  niezależny od GPIO, bez automatycznego łączenia po GA.
- [cpld-adc-flash.yaml](../examples/cpld-adc-flash.yaml): GPIO, Flash i ADC
  identyfikacyjny początkowo wyłączone; `project_id.on_value` pokazuje ich
  warunkowe włączanie w YAML. PROJECT_ID0x42/0x43 oraz przedział ID są wyłącznie
  przykładami do kompilacji i wymagają rzeczywistej tabeli aplikacji.

`enabled` w GPIO i Flash domyślnie wynosi `false`. Blok `adc` readoutu ma
własne `enabled`, także domyślnie `false`. Sama obecność komponentu lub
konfiguracji ADC nie upoważnia go do transakcji w wyłączonym bloku.

Przykład aplikacyjnego wyboru po odczycie ID:

```yaml
project_id:
  name: Project ID
  on_value:
    then:
      - lambda: |-
          // Tylko przykład: rzeczywiste ID określa aplikacja.
          bool gpio_board = x == 0x42 || x == 0x43;
          id(target_flash).set_enabled(x == 0x42);
          id(readout).set_adc_enabled(x == 0x43);
          id(bridge_gpio).set_enabled(gpio_board);
```

`CPLDGPIO::set_enabled(true)` inicjalizuje GPIO i sprawdza lokalny STATUS
CS3. `set_enabled(false)` wyłącza dostęp software bez zmiany fizycznych stanów
pinów. Odmawia wyłączenia podczas aktywnej rezerwacji ADC/Flash. Nie jest to
przełączenie hardware w Hi-Z ani zmiana zatrzaśniętego CS3_MODE.

GPIOPin zapamiętuje konfigurację oraz początkowe stany wyjść zadane przez YAML
podczas wyłączenia. Przy aktywacji wykonuje je przez odłączenie wyjścia,
ustawienie DATA/OD i ponowne ustawienie DIR. Bezpośrednie API `write_pin` oraz
`configure_pin` odmawia działania w wyłączonym komponencie.

Flash dopiero przy jawnym acquire/transfer korzysta z CS2 i CFG na CS3.
`set_enabled(false)` próbuje zwolnić aktywną sesję. Jeśli potwierdzenie release
zawiedzie, dalsze transfery są zablokowane, a cleanup jest ponawiany w pętli.
I²C z wyłączonym ADC czyta tylko swój blok readoutu; `set_adc_enabled(true)`
uruchamia pomiary dopiero w cyklu update, po uprzątnięciu poprzedniego stanu
ADC_CTRL. Wyłączenie w trakcie pomiaru przerywa go i kontynuuje cleanup do
potwierdzenia CTRL=0. Nie traci rezerwacji po błędzie transportu.

## Osobne transporty i jawne powiązania

Readout wymaga tylko I²C; adres7-bitowy to `0x50 + module_address` (GA).
GPIO wymaga `spi` i `addrspi`, subadresu3 i mode0. Flash wymaga subadresu2
oraz jawnego `gpio_id`, bo CFG jest dostępny tylko przez CS3. Flash wymaga też
`target_profile`, `spi_mode` i `data_rate`. Nie ma czwartego komponentu.

Obecność GPIO o tym samym GA nie łączy go automatycznie z readoutem.
Readout ma opcjonalny `gpio_id` wyłącznie do koordynacji górnego banku z
ADC identyfikacyjnym CPLD. Wskazane ID musi mieć zgodne `module_address`;
bez tego ID readout pozostaje niezależny. Nie wymaga CS3 również przy ADC.
Aplikacja odpowiada za jawne powiązanie, jeśli równocześnie używa ADC i GPIO
na fizycznie wspólnym banku. Flash/GPIO mają jednego właściciela CFG.

Przykłady używają dwóch poziomów `addrspi`: cztery HSPI_AD wybierają GA,
a dwie HSPI_sAD wybierają układ przed CS. Przy nieruchomym HSPI_AD można użyć
jednego poziomu i zapewnić adresowanie modułu poza driverem. Drivery CPLD
nie sprawdzają kanałów ani konfiguracji jakiegokolwiek innego urządzenia SPI.

## GPIO i wejścia

Piny0..7 oznaczają REG_OUT, piny8..15 oznaczają REG_IN. Kierunek i open-drain
konfiguruje standardowy pin schema ESPHome. Podciągnięcia są zewnętrzne;
wewnętrznych pull-up/pull-down nie obsługujemy.

`upper_input_mask` i `lower_input_mask` są generic maskami ustawianymi przez
YAML (domyślnie0). Chronią dowolne wejścia sterowane przez obwód zewnętrzny;
nie wynikają z nazwy płytki ani roli sygnału. Driver nie ma wbudowanej maski
DRDY ani tabeli RTD16. Po aktywacji setup odczytuje DATA/DIR/OD/CFG/STATUS,
następnie usuwa z DIR tylko bity jawnych masek wejściowych.

Zapis jednego pinu zachowuje pozostałe bity. Odczyt dotyczy pada, nie latcha.
Błąd potwierdzenia zapisu blokuje dalsze zmiany tego banku do ponownej
inicjalizacji; drugi bank i cleanup CFG pozostają dostępne. Po rezerwacji
ADC zmiany górnego banku są odrzucane, dolny pozostaje dostępny. W Flash mode
OUT1..4 pozostają zarezerwowane także po release.

## I²C i opcjonalny ADC identyfikacyjny CPLD

Sensory: `project_id`, `revision_id`, `power_request`, `errin`, `upper_gpio`,
`lower_gpio`, `fault_mask`, `fault_status` i text sensor `status`.
PROJECT_ID i REV_ID są danymi dla aplikacji; wszystkie wartości są akceptowane.
Niewiarygodny bit6 statusu fault jest pomijany, pozostaje maska0x8F.

`digital_ids` definiuje pola przez `bank`, `mask`, `shift`, `xor_mask`,
tabelę `codes` i text sensor `sensor`. Wartość to
`((pads XOR xor_mask) AND mask) >> shift`; nieznany kod/błąd daje `unknown`.
Żaden bit nie ma znaczenia przypisanego przez driver.

`error_counters` mapuje kanały0..3 na sensory. Gdy skonfigurowano choć jeden,
readout wykonuje jeden burst czterech liczników i kasuje wszystkie cztery.
Bez konfiguracji nie czyta ich wcale. To przedziałowe wartości modulo256;
błąd daje NaN i nie jest natychmiast ponawiany. Odczyt mógł już utracić
zdarzenia. Jeden adres na magistrali I²C ma jedną instancję readoutu.

ADC w tym komponencie to wyłącznie opcjonalny PWM/comparator ADC CPLD do
rezystorów ID, nie ADC Texas Instruments do pomiaru temperatur.
Wymaga jawnego bloku `adc`, `bits: 8|9|10`, `enabled: true` lub runtime
`set_adc_enabled(true)`, odpowiedniego obrazu i toru komparatorów.
Nie jest wykrywany po REV_ID. Pomiary używają `update_interval` (dom.1s),
`timeout` (dom.500ms), a każdy kanał może mieć sensor `raw`, text sensor `id`
i rozłączne domknięte `bands` (`min`, `max`, `id`).
Przeliczanie na omy wymaga schematu i kalibracji poza driverem.

START zapisuje CTRL=3 po rezerwacji banku, kontroluje ENABLE i odpytuje DONE
co5ms. Jeden burst16 bajtów od0x32 dostarcza osiem słów little-endian.
Wyniki są kopiowane przed disable i publikowane po potwierdzonym CTRL=0.
Timeout/błąd daje NaN/unknown, nigdy częściową ramkę. Nieudany disable zachowuje
rezerwację i jest ponawiany co100ms; także po wyłączeniu pomiarów w YAML.
Wymagany jest ręczny START w HDL, `FREE_RUNNING=false`; timeout trzeba dopasować
do parametrów obrazu. Zero/full-scale nie są automatycznym dowodem obecności ID.

## Flash

`acquire(target_ready)` wymaga wcześniejszej jawnej izolacji targetu przez
aplikację według jego PCB/datasheet. `false` odmawia przejęcia. STATUS musi
potwierdzać CS2_MODE=CS3_MODE=1; identyfikacja płyty nie jest wykonywana.
`transfer(tx, rx, size)` wysyła1..4096 bajtów pod jednym CS (rx może być null),
a `release()` deassertuje własność przez wyczyszczenie tylko CFG[0].
Preferowane `transaction(tx, rx, size, target_ready)` obejmuje całą sesję,
łącznie ze zwolnieniem po błędzie argumentów. Transfer jest synchroniczny.

Wszystkie zmiany CFG przechodzą przez helper GPIO, zachowują inne bity i nie
przestawiają resetu/bootu targetu. CS high nie oznacza release. Błąd acquire
może oznaczać udany zapis do CPLD: lease jest zachowany i uprzątany. Release
jest potwierdzany readbackiem i STATUS; błąd zachowuje lease i ponawia cleanup
co100ms. Nieznana wcześniejsza własność nie jest przejmowana ani kasowana.

`set_target_controls(cfg_levels)` zmienia tylko CFG[2:1] przy wolnej Flash.
Jawne `configure_target_controls(cfg_levels, direction, open_drain)` ustawia
poziomy CFG oraz DIR/OD OUT1/2, z odłączeniem przed zmianą i zachowaniem innych
pinów. Kierunki/OD używają maski padów0x06; CFG[2]=OUT1, CFG[1]=OUT2.
Żadna z tych procedur nie uruchamia się automatycznie.

SPI nie ma ACK ani wyniku błędu w delegacie ESPHome. Readback/STATUS nie
potwierdzają poprawności danych pamięci. Geometria, opcodes, BUSY, erase,
programowanie i boot targetu wymagają osobnego profilu/procedury aplikacji.
Shutdown próbuje zwolnić zasoby, ale reset MCU lub brak CS3/I²C nie gwarantują
potwierdzonego release sprzętowego.

## Weryfikacja

```sh
./scripts/test-cpld.sh
../esphome-hil/scripts/build.sh examples/cpld-readout.yaml
../esphome-hil/scripts/build.sh examples/cpld-gpio.yaml
../esphome-hil/scripts/build.sh examples/cpld-adc-flash.yaml
```

Testy używają przypiętego Nix, rzeczywistych klas C++/addrspi i elektrycznie
obojętnych emulatorów oraz prawdziwych schematów/YAML. Sam I²C jest też
kompilowany bez include/definicji GPIO. [Raport bieżącej implementacji](../reports/cpld-independent-software-2026-09-30.json)
oddziela software od hardware. Nie resetowano, nie odczytywano ani nie
programowano sprzętu; walidacja fizyczna pozostaje osobnym etapem.
