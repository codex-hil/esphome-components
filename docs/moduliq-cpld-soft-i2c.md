# I²C przez GPIO CPLD

`moduliq_cpld_soft_i2c` wystawia zwykły `i2c::I2CBus` ESPHome. Urządzenia
używają `i2c_id`; nie potrzebują specjalnych driverów. Każda instancja zajmuje
dwa różne piny jednego banku. Można zadeklarować cztery niezależne magistrale
na banku (osiem na obu bankach, jeśli pinmux płytki na to pozwala).
Numery 0–7 oznaczają dolny bank, 8–15 górny. Ten sam adres urządzenia może
występować na każdej niezależnej magistrali.

```yaml
moduliq_cpld_soft_i2c:
  - id: head_bus_0
    gpio_id: bridge_gpio
    sda: 8
    scl: 9
    frequency: 1kHz
    stretch_timeout: 10ms
    scan: false

pca9554:
  - id: head_io_0
    i2c_id: head_bus_0
    address: 0x20
```

Pełny [przykład czterech magistral](../examples/cpld-soft-i2c.yaml) kompiluje się
na ESP32/ESP-IDF z przypiętym ESPHome 2026.9.0. Piny ESP32, adres modułu, modele
urządzeń i adresy I²C są przykładowe, do dopasowania przed testem sprzętowym.
W `external_components` dodaj `moduliq_cpld_soft_i2c` obok `spi`, `addrspi`
i `moduliq_cpld_gpio`. Zwykły blok `i2c:` jest potrzebny tylko dla fizycznego
I²C ESP32, np. do readoutu CPLD; same magistrale programowe go nie wymagają.

SDA i SCL są wyjściami open-drain: 0 ściąga linię, 1 ją zwalnia. Obie linie
wymagają zewnętrznych rezystorów podciągających do napięcia właściwego dla PCB.
Clock stretching sprawdza fizyczny pad SCL; ACK i dane sprawdzają fizyczny pad
SDA. Wymagany jest obraz CPLD z obsługą OD i odczytem padów obu banków.
Kompilacja i model software nie potwierdzają działania konkretnego obrazu HDL.

Komponent obsługuje adresy 7-bitowe, START, repeated START, STOP, ACK/NACK,
clock stretching oraz odzyskiwanie SDA przez maksymalnie dziewięć impulsów SCL
i STOP. Jest przeznaczony do magistral z jednym masterem. Nie realizuje
arbitrażu między wieloma masterami ani adresów 10-bitowych.

Częstotliwość 100–1000 Hz jest górnym limitem, nie gwarantowanym taktowaniem.
Weryfikowane zapisy rejestrów i odczyty padów przez SPI 100 kHz dodatkowo
wydłużają cykl. Aktualna implementacja jest synchroniczna: transfer obejmuje
maksymalnie 32 bajty danych łącznie (zapis + odczyt) i ma budżet 500 ms.
Sprzątanie STOP może dodatkowo czekać na SCL do `stretch_timeout` (100 µs–100 ms).
Dłuższe transfery zwracają `ERROR_TOO_LARGE`; wolny transfer może przekroczyć
budżet nawet przed osiągnięciem limitu długości. To interfejs dla krótkich
transakcji sensorów/expanderów, nie dla dużych ramek wyświetlaczy. Nie stosuje
ukrytych ponowień zapisów. `scan: true` sprawdza jeden adres na iterację pętli,
w zakresie 0x08–0x77.

Właścicielem maskowanych zapisów i wspólnej kopii DATA pozostaje expander.
Zmiana pary nie narusza innych bitów. Rezerwacje pinów trwają przez całe życie
komponentu; kolizje z innymi parami, zwykłym GPIO, maskami wejść i znanym
pinmux Flash są odrzucane. Również pinmux odczytany z STATUS jest sprawdzany
w runtime. Rezerwacja górnych pinów blokuje ADC identyfikacyjny CPLD na tym banku.
Żaden kod tego komponentu nie korzysta z ADC temperatury ani DRDY.

Wyłączenie expandera powoduje `ERROR_NOT_INITIALIZED`, bez transakcji SPI.
Po ponownym włączeniu OD i zwolnienie linii są odtwarzane przed użyciem busa.
Dla zwykłych urządzeń, które odpytują magistralę w `setup()`, expander musi być
aktywny już podczas ich inicjalizacji (`enabled: true` w przykładzie).
Późniejsze włączenie expandera nie naprawia automatycznie urządzenia oznaczonego
przez jego własny driver jako failed. Kolejność setup jest ustawiana na
addrspi 930, expander 920, software I²C 910, przed zwykłymi expanderami I/O 900.
Nie konfiguruj `setup_priority` busa niżej niż jego urządzeń.

Przy błędzie SPI expander odrzuca dalsze zapisy z niepewnej kopii banku.
Nie można wtedy zagwarantować fizycznego zwolnienia padów; wymagane jest
ponowne poprawne uruchomienie expandera. Po timeoutach I²C komponent próbuje
STOP i zwolnienia SDA/SCL bez ponawiania operacji urządzenia.

Testy software:

```sh
./scripts/test-soft-i2c.sh
../esphome-hil/scripts/build.sh examples/cpld-soft-i2c.yaml
```

Status: testy modelu i kompilacja przeszły, weryfikacja fizyczna pozostaje do
wykonania. Nie otwierano portów sprzętu, nie resetowano ani nie programowano PCB.
Do pierwszej próby potrzebne są model/adres urządzenia, bank i piny SDA/SCL,
pull-upy i napięcie I/O oraz identyfikacja obrazu CPLD. Na początku sprawdzamy
jedną parę i odczyt znanego rejestru z niezależną obserwacją przebiegu SDA/SCL.
