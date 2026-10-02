# SPI dla głowicy mixed-signal

`addrspi` pozostaje bez zmian. Te same sterowniki obsługują SPI bezpośrednie,
adresowanie równoległe oraz równoległe i szeregowe jednocześnie:

```text
hardware SPI ESP32
  → addrspi: 4 bity GPIO wybierające moduł
    → addrspi: 2 bity GPIO wybierające chip / kanał huba
      → addrspi2: 8-bitowy adres szeregowy
        → mcp3208 / dacx0504 / spi_shift_register
```

Bez dodatkowego adresowania szeregowego pomiń `addrspi2` i ustaw `spi_id`
urządzenia na kanał istniejącego `addrspi`. Nie trzeba przełączać wariantu
sterownika ADC/DAC. Przykłady:

- [Głowica: równoległe i szeregowe](../examples/mixed-signal-head.yaml).
- [Tylko równoległe, inna płytka — przykład kompilacji](../examples/mixed-signal-parallel-only.yaml).

Kod nie programuje CPLD ani nie korzysta z jego rejestrów I²C. Używa istniejących
pinów adresowych ESP32. Dla testowanej głowicy module=0, chip=2, SCK=14,
MOSI=12, MISO=34, CS=15. Magnetometr na kanale 1 pozostaje poza konfiguracją.

## Ramka i zgodność sterowników

`addrspi2` dodaje header **do bufora**, przed wysłaniem, zamiast transmitować go
w `begin_transaction()`. Adapter dostaje jeden pełny bufor natywny i przekazuje
`[ADDRESS][payload]` jednym wywołaniem transferu do hardware SPI. Przy RX pomija
pierwszy bajt. Przykład ADC Z CH3:

```text
TX: 04 06 C0 00
RX: xx xx 07 FD
native RX: xx 07 FD → code 2045
```

`addrspi` ustawia GPIO przed opuszczeniem CS, także w zagnieżdżonym stosie.
Obie selekcje działają przy każdej ramce. Lokalny latch i C13 pozostają częścią
hardware, a nie firmware.

Każdy klient musi wykonać **jedną operację buforową między enable/disable**.
Sterowniki wykonujące trzy kolejne `transfer_byte()` nie są automatycznie zgodne:
RX pierwszego bajtu nie można zwrócić, zanim znany jest cały payload. Adapter
odrzuca drugą operację w tym samym CS i loguje błąd; nie udaje pełnej zgodności
z dowolnym strumieniowym komponentem ESPHome. MCP3208 i DACx0504 zostały
zmienione na pełne trzybajtowe bufory; GPIO używa jednego bajtu.

Ramki payload 1–16 B nie alokują pamięci podczas transferu. Większe używają
bufora dynamicznego; limit 4091 B utrzymuje wire frame w pojedynczym transferze
ESP-IDF (4092 B). Nie są obsługiwane niepełne bajty ani quad/octal SPI.
Należy jawnie wybrać fizyczne `interface: hardware`. Walidator obsługuje
zagnieżdżone `addrspi`, odrzuca software SPI, cykle, zdublowane adresy i MODE1
klienta za routerem. Router wymaga MODE0/MSB first. `max_data_rate` ogranicza
zarejestrowane urządzenia; dla LTC6820 SLOW=HIGH ustaw 200kHz, obecnie używamy
100kHz. Nie zmieniaj zegara na szybszy bez ponownego pomiaru.

W przykładzie CS jest powtórzony dla klientów, ponieważ ich istniejące schematy
go wymagają. **Fizycznym CS zawsze zarządza hub `addrspi2`**. Powtórzone GPIO15
ma `allow_other_uses: true`. `shared_device: true` sprawia, że wszystkie lokalne
adresy głowicy używają jednego uchwytu hardware SPI. Każdy klient nadal ma własny
adapter headera; SPI ustawia równoległe adresy przed każdym CS. Hub i wszyscy
klienci muszą mieć jednakowe `data_rate`. Walidator odrzuca inne szybkości.
To pozwala obsługiwać sześć lokalnych urządzeń i magnetometr obok głowicy bez
przekroczenia limitu sześciu uchwytów ESP-IDF. `addrspi` pozostaje bez zmian.

Opcja domyślnie jest `false`, co zachowuje osobne uchwyty klientów. W tym wariancie
jawne helpery `write_transaction` / `transfer_transaction` tworzą dodatkowy
uchwyt na czas ramki i wymagają wolnego slotu. Przy `shared_device: true` helpery
korzystają z istniejącego uchwytu głowicy. Każda ramka nadal jest jednym burstem.

## ADC MCP3208

To MCP3208, nie MCP3564. Natywny payload ma trzy bajty; 12-bitowy kod odczytuje
się z ostatnich dwóch. Napięcie wejścia:

```text
V = code × reference_voltage / 4096
```

Nie dzielimy przez 4095: pełny kod oznacza jeden LSB poniżej VREF. Ustaw
zmierzone `reference_voltage`; przykład używa nominalnych 3 V. Dzielniki i
kalibrację toru stosuj w standardowych filtrach sensora (`multiply`, `offset`,
`calibrate_linear`). Przykładowo ERROR/IMON mają dzielnik 1:2, więc `multiply: 2`
przywraca napięcie przed nim. Nie stosuj tego mnożnika do VOUT_P/N.

Opcjonalne `sample_rate: 100Hz` oznacza **100 konwersji/s łącznie na jeden ADC**,
w kolejności round-robin dla zarejestrowanych par kanał/tryb. Trzy sensory dają
około 33,3 konwersji/s na kanał. `update_interval: 1s` tylko publikuje ostatni
wynik z cache i nie wykonuje dodatkowej konwersji. Bez `sample_rate` zachowane
jest dotychczasowe próbkowanie na żądanie przy publikacji. Harmonogram działa
w pętli ESPHome, bez nadrabiania serii po opóźnieniu; nie gwarantuje czasu
rzeczywistego. Dobierz sumaryczne obciążenie wszystkich ADC do zegara SPI i
pozostałych zadań. MCP3208 nie ma rejestru sample rate, OSR, gain ani SCAN.

## DACx0504 i skalowanie

W tej głowicy:

```yaml
reference: EXTERNAL
reference_voltage: 3V
reference_divider: 2
gain: 2
spi_mode: MODE0
data_rate: 100kHz
fast_sdo: true
verify_registers: true
initial_values: [0.5, 0.5, 0.5, 0.5]
```

Bez soft-resetu: przy podłączonym R24 reset ponownie włącza referencję wewnętrzną.
Najpierw zapisujemy CONFIG (external reference, FSDO), następnie GAIN/SYNC.
`fast_sdo` wybiera aktualizację SDO na opadającym zboczu, co przeszło test isoSPI.
Przy `verify_registers` porównujemy ID/model, CONFIG/GAIN i alarm referencji;
niepowodzenie blokuje dalsze zapisy kodów. SPI ESPHome nie zwraca statusu
transmisji przez API buforowe; readback nie zastępuje diagnostyki elektrycznej.

Normalny output ESPHome nadal przyjmuje 0…1. Bez parametrów napięcia mapuje je
na kody 0…max. Dodanie `min_voltage` i `max_voltage` mapuje ten zakres na wolty,
np. 0…3 V lub 0,5…2,5 V. Również lambda może użyć:

```cpp
id(dac).set_channel_voltage(2, 1.5f);
```

Napięcie jest ograniczone do unipolarnego zakresu `VREF × gain / divider`;
wartości NaN są odrzucane. Dobierz zakres outputu do tego limitu. `initial_values`
są znormalizowane 0…1, a nie podane w woltach; 0,5 oznacza midscale.

Opcjonalne `synchronous_update: true` sprawia, że settery tylko przygotowują
kody. Po ustawieniu kilku kanałów wykonaj `id(dac).commit()`: użyje pinu LDAC,
jeśli skonfigurowany, albo wspólnego software LDAC. Nie ustawiaj tego trybu bez
zaplanowanego commit, ponieważ outputy nie będą aktualizowane automatycznie.

Mapowanie fizyczne: DAC0=X, DAC1=Y, DAC2=Z, DAC3=VZERO/common. Punkt określony
w sesji jako pin18 złącza C **w pomiarze śledził DAC2**, więc zgodność numeracji
tego punktu z PDF pozostaje do sprawdzenia.

## GPIO

`spi_shift_register` obsługuje jeden HC595 lub HC165 na hub. HC595 zapisuje
pełny bajt, aktualizowany atomowo przez hardware na końcu lokalnego CS.
`initial_value: 0x07` ustawia DIS A/B/C HIGH. Outputy binarne modyfikują bity
shadow register; lambda `write_byte_value()` ustawia cały wzorzec naraz.

HC165 wymaga **sprzętowego** LOAD LOW przed odczytem oraz HIGH przez cały payload.
Nie generujemy LOAD osobnym GPIO ESP32, bo głowica nie ma takiej linii.
`load_pulse_verified: false` blokuje odczyty i pozostawia sensory bez publikacji;
po zamontowaniu negatora, zmierzeniu timingów i kolejności bitów zmień na `true`.
D0/D1/D2 odpowiadają OK A/B/C. Nie odwracamy ani nie przesuwamy danych, żeby
maskować obecny błąd /PL. Komponent pozostaje oznaczony jako hardware pending.

## Weryfikacja i dalszy bring-up

Software: testy rzeczywistego kodu C++ z atrapą SPI sprawdzają oba adresowania
razem, SPI bez headera, jeden transfer, RX discard, brak heap dla krótkiej ramki,
próbkowanie/cache, skalowanie DAC, readback i bramkę HC165. Testy konfiguracji
sprawdzają oba przykłady oraz błędne ustawienia. Oba firmware są budowane dla
ESPHome 2026.9.0 / ESP-IDF. Nie zostały wgrane podczas przygotowania sterowników.

Dotychczasowe HIL dotyczy minimalnego firmware: 1090 poprawnych transakcji,
dwóch torów loopback i pięciu poziomów DAC2 zmierzonych SMU. Nie nadaje to
automatycznie statusu HIL nowemu komponentowi ESPHome. W poniedziałek:

1. Zmierzyć /PL po negatorze, payload SCK i Q7/MISO; sprawdzić bit D0=1.
2. Przetestować GPIO 00/01/55/AA/FF w zakresie fizycznie dostępnych wejść/wyjść.
3. Wgrać przykład głowicy przy odłączonym stopniu mocy i zweryfikować init,
   ciągły burst oraz brak impulsów na obcych CS.
4. Powtórzyć loopback/SMU i regresję 1000 transakcji dla finalnej warstwy ESPHome.
5. Sprawdzić VREF i pozostałe kanały niezależnymi znanymi sygnałami.

## Wynik przygotowania driverów — przed testem sprzętu

ESPHome 2026.9.0 / ESP-IDF 5.5.5: oba przykłady skompilowane, testy C++
transportu i driverów oraz 12 przypadków walidacji YAML — PASS.
[Raport i sumy źródeł/firmware](../reports/mixed-signal-head-software-2026-10-02.json).
W etapie przygotowania firmware nie wgrywano. Wcześniejszy test sprzętu dotyczył surowego
programu bring-up, więc nie zastępuje testu tych komponentów na PCB.

## Walidacja współpracy z magnetometrem — 2026-10-02

Na jednym hardware SPI: głowica na module 0 / chip 2 z headerem szeregowym,
MMC5983 na module 0 / chip 1 przez samo `addrspi`. Użyto istniejącego,
zwalidowanego drivera MMC z sąsiedniego checkoutu `esphome-components-mmc`;
nie zmieniano jego źródeł. Labowe YAML `mixed-signal-coexistence-*.yaml` pobierają ten driver z GitHuba
po pełnym SHA `ead227b55cbcb71cd21383191e22b187c3299a26`; nie wymagają sąsiedniego
checkoutu. Wymagają sprzętu z odłączonym stopniem mocy. CPLD nie programowano.

Test wymusza przejście do magnetometru pomiędzy każdym odczytem ADC, po zapisach
DAC i GPIO, sprawdza identyfikację MMC oraz DAC readback. Dwa znane tory to
DAC2→ADC Z CH3 i DAC3→ADC Z CH4; pozostałe 22 kanały są odczytane i sprawdzone
pod kątem zakresu, ale nie mają podanych wzorcowych napięć.

Pierwszy test zatrzymał się po odchyłce ADC przy SMU ON. Diagnostyka zachowała
693 wiersze / 2772 odczyty obu torów: SMU OFF dawało 2045–2046 kodów przy 1,5 V,
SMU ON rozszerzało rozrzut podłączonego toru do 2040–2051. Magnetometr bez SMU
nie wywołał takiego rozrzutu. Jest to dowód zależności od podłączenia SMU,
a nie ustalenie dokładnego mechanizmu elektrycznego. Tolerancji regresji ADC
nie zwiększano, nie uśredniano i nie pomijano błędnego pomiaru. Ścisły loopback
wykonuje się z SMU OFF/HIZ; osobny sweep SMU sprawdza napięcia DAC.

Przy próbkowaniu w `loop()` wykryto odstęp 17 ms zamiast zadanych 10 ms:
domyślna pętla ESPHome ma okres 16 ms. ADC używa teraz
`HighFrequencyLoopRequester` tylko przy włączonym próbkowaniu i zarejestrowanych
kanałach; zatrzymuje go po wyłączeniu próbkowania, nieudanym setup i shutdown.
Próbkowanie pozostaje zależne od dostępności magistrali i blokujących operacji
innych klientów; nie gwarantuje twardego harmonogramu czasu rzeczywistego.

GPIO OUT: na oscyloskopie potwierdzono ramkę `01 AA` z 16 ciągłymi taktami.
Użytkownik potwierdził odłączenie CH4. Zarejestrowane LOW nie jest pomiarem
wyjścia GPIO ani wskazaniem jego usterki.
Nie nadajemy PASS fizycznym wyjściom QA…QH. HC165 nadal wymaga negatora.

[Raport zbiorczy i ograniczenia](../reports/mixed-signal-head-hil-2026-10-02.json).

Końcowa regresja firmware: **125 cykli, minimum 7631 transakcji, 0 wykrytych
błędów komunikacji**. W trzech pełnych przebiegach było minimum 22893 transakcji.
Końcowy pomiar oscyloskopowy daje okresy ADC 9,999–10,016 ms (łącznie ok. 100 Hz
między trzema zarejestrowanymi kanałami), a publikacja sensorów ma osobne 1 s.
Załączenie blokującej konwersji magnetometru może chwilowo opóźnić próbkę.
Zmierzona ramka ADC `04 07 00 00`: 32 takty, granica header/payload 10 µs,
SCK HIGH ok. 1,027 µs. Ramka GPIO `01 AA`: 16 taktów; CH4 był odłączony — napięcia wyjścia nie zmierzono.

Osobny sweep DAC2 przez SMU: −0,00012 / 0,74939 / 1,49928 / 2,24913 / 2,99896 V
przy 0/25/50/75/100%; największa odchyłka od nominalnego VREF 3 V: 0,994 mV.
VREF bezpośrednio nie mierzono. DAC3 potwierdzono loopbackiem, bez absolutnego
pomiaru SMU. DAC2/3 przywrócono do 0x8000, SMU pozostawiono OFF/HIZ.

Zachowano osobno pierwszy FAIL z SMU ON, surowe próbki diagnostyczne i wszystkie
przebiegi. Jednorazowy startowy PID=FF MMC został odzyskany przez istniejący
ograniczony retry i jest zapisany jako nierozwiązany problem inicjalizacji.
Końcowa regresja nie wymagała retry i nie wykryła błędów MMC.

Pełny [dziennik bring-up i archiwum przebiegów](../hil/mixed-signal-head/README.md)
zawiera również wcześniejsze błędy PCB, minimalny ESP-IDF probe i przekazanie sprzętu.

Do publikacji przykład koegzystencji ponownie skompilowano z driverem MMC
pobranym po Git SHA. C++/H są identyczne z testowanym snapshotem; nie flashowano
ponownie po zwolnieniu sprzętu. [Kontrola publikacji](../reports/mixed-signal-publication-2026-10-02.json).
