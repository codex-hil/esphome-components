# MODULIQ isoSPI mixed-signal head — bring-up

## Sprzęt zwolniony dla innego wątku — 2026-10-02

Zatrzymano aplikację testową przez wejście ESP32 do ROM bootloadera
(esptool --no-stub --after no_reset, potwierdzony MAC). Nie flashowano
ani nie programowano CPLD. SMU potwierdzone OFF/HIZ, oscyloskop STOP.
Połączenia zamknięte, brak aktywnych procesów testowych. Po resecie ESP32
ponownie uruchomi zapisany firmware testowy; ROM jest gotowy do nowego flasha.
[Stan przekazania](results/hardware-release.json).

## ESPHome driver HIL i koegzystencja z magnetometrem — 2026-10-02

**PASS: transport, DAC, dwa loopbacki ADC i naprzemienny dostęp do MMC.**
Głowica: module=0/chip=2 + serial header; MMC5983: module=0/chip=1 przez
niezmienione `addrspi`, bez serial headera. ESP32 potwierdzono MAC
68:25:dd:4c:49:e4 przed każdym flashem. CPLD nie programowano.
Użyto sprawdzonego drivera MMC z checkoutu esphome-components-mmc @ ead227b,
bez zmiany jego źródeł; nie nadaje to PASS staremu driverowi MMC w głównej kolekcji.

Końcowy firmware: 125 cykli, minimum 7631 ramek SPI, 24 wejścia ADC odczytane,
DAC readback i software LDAC/hold PASS, zero wykrytych błędów komunikacji MMC.
Trzy pełne przebiegi łącznie minimum 22893 transakcji. Tylko ADC Z CH3/CH4 mają
znane napięcia DAC2/3; pozostałe 22 wejścia sprawdzono wyłącznie zakresowo.

Pierwszy test FAIL przy SMU ON: ADC P spadł do 1,492676 V przy DAC 1,5 V.
Zachowano błąd. Diagnostyka 693 wierszy / 2772 odczyty pokazała rozrzut
podłączonego P po włączeniu SMU: 2040–2051 kodów; OFF: 2045–2046. Magnetometr
bez SMU nie wywołał tego efektu. Nie zwiększono tolerancji 6 kodów ani nie
filtrowano. Dokładny elektryczny mechanizm sprzężenia SMU pozostaje niezmierzony.
Ścisłą regresję wykonano z SMU OFF/HIZ, pomiar absolutny jako osobny sweep.
DAC2 SMU: −0,00012 / 0,74939 / 1,49928 / 2,24913 / 2,99896 V, PASS ±10 mV;
max błąd względem nominalnego VREF 3 V: 0,994 mV. VREF bezpośrednio NOT_RUN.

Znalezione i poprawione ograniczenia firmware: pętla ESPHome dawała ~17 ms
między próbkami zamiast zadanych 10 ms; ADC używa teraz HighFrequencyLoopRequester
przy aktywnym próbkowaniu. Pomiar końcowy: 9,999–10,016 ms. Publikacja sensorów
pozostaje osobnym parametrem 1 s. Opcjonalny `shared_device: true` w addrspi2
rezerwuje jeden uchwyt SPI dla wszystkich adresów głowicy, drugi dla magnetometru;
eliminuje przekroczenie limitu sześciu uchwytów po dodaniu GPIO IN. `addrspi` bez zmian.

Oscyloskop: ADC `04 07 00 00`, 32 takty, okres granicy header/payload ~10 µs,
SCK HIGH ~1,027 µs. GPIO `01 AA`, 16 ciągłych taktów — PASS dla wysłanej ramki.
Użytkownik potwierdził, że CH4 był odłączony. LOW z odłączonej sondy nie jest
wskazaniem usterki GPIO. Fizyczne GPIO OUT **NOT_VALIDATED — brak sondy**, HC165 **NOT_RUN — negator /PL nadal potrzebny**.
Brak sond /CS_HEAD, lokalnego ADC/DAC CS i MISO ogranicza niezależną obserwację
CS/timingów; nie deklarujemy sprawdzenia wszystkich możliwych glitchy.

Jeden wcześniejszy startowy odczyt MMC PID=FF odzyskany przez istniejący bounded
retry jest nadal problemem otwartym. Ostatni przebieg: 0 startup/runtime errors.
DAC2/3 przywrócono do 8000/8000; GPIO wysłano 07 (stan fizyczny niepotwierdzony),
SMU OFF/HIZ. ESP32 pozostaje na końcowym firmware moduliq-coexistence-hil.

- [Raport zbiorczy](results/esphome-driver-coexistence-summary.json).
- [Końcowy UART](results/esphome-coexistence-final/uart.log).
- [Ramka ADC](results/driver-full-adc-frame-frame.svg).
- [Ramka GPIO](results/esphome-coexistence-voltage-shared-gpio-frame.svg).
- [Próbkowanie po poprawce](results/driver-adc-rate-final-sampling.svg).
- [Opis komponentów](../../docs/mixed-signal-head.md).


## Przygotowane komponenty ESPHome — 2026-10-02

PASS (software): zachowano istniejący `addrspi` bez zmian; opcjonalny `addrspi2`
dodaje szeregowy header po równoległym wyborze module=0/chip=2. Pominięcie
`addrspi2` pozwala tym samym sterownikom ADC/DAC/GPIO używać samego `addrspi`.
ADC MCP3208 i DACx0504 przesyłają kompletny payload jednym buforem; adapter
przesyła header+payload jednym hardware SPI transfer i pomija RX headera.
Dodano skalowanie napięcia, parametr próbkowania ADC niezależny od publikacji,
obsługę synchronicznego commit DAC oraz buforowane GPIO HC165/HC595.

Oba przykłady zbudowano dla ESPHome 2026.9.0 / ESP-IDF 5.5.5. Testy transportu,
driverów, skalowania, 12 przypadków YAML i importów komponentów: PASS.
Nowe firmware: **HIL NOT RUN** — nie flashowano ESP32, nie zmieniano CPLD.
HC165 pozostaje programowo zablokowany do zamontowania negatora i sprawdzenia
/PL, kolejności bitów oraz MISO. Nie zastosowano obejścia błędu PCB.

- [Opis warstw i skalowania](../../docs/mixed-signal-head.md).
- [Konfiguracja głowicy](../../examples/mixed-signal-head.yaml).
- [Raport testów i sumy firmware](../../reports/mixed-signal-head-software-2026-10-02.json).


## Sterowanie HC165 w ESPHome i wariant poprawki /PL — 2026-10-02

Sprawdzono aktualny upstream ESPHome sn74hc165.cpp:
https://raw.githubusercontent.com/esphome/esphome/dev/esphome/components/sn74hc165/sn74hc165.cpp
Komponent nie używa hardware SPI. Ustawia LOAD LOW na 10 µs, następnie HIGH,
czeka 10 µs, odczytuje aktualny bit i generuje zegar GPIO (HIGH/LOW po 10 µs).
Nie da się użyć go bezpośrednio przez naszą magistralę isoSPI z szeregowym headerem.

Impuls z lokalnego /CS można uzyskać przez kondensator szeregowy /CS→/PL
oraz pull-up /PL do VCC. To układ różniczkujący, nie zwykłe opóźnienie RC.
Konieczne odłączenie bezpośredniego /CS od pin1; /OE bufora MISO pozostaje
sterowane oryginalnym /CS. /PL musi wrócić do pewnego HIGH przed pierwszym
narastającym SCK payloadu. Obecny zapas od aktywacji /CS do payloadu ~8,8 µs
przy 100 kHz. Trzeba zwalidować szerokość impulsu, wolne przejście przez progi,
przepięcie na zboczu powrotnym, start zasilania i tolerancje elementów.
Nie wybrano wartości RC ani nie zatwierdzono przeróbki jako PASS.

Prostsza deterministyczna poprawka: inwerter /CS→/PL (idle /PL LOW = load,
wybrany /PL HIGH = shift); /OE MISO nadal /CS. W obu wariantach należy po
przeróbce zweryfikować setup/hold i kolejność D7…D0 na rzeczywistym isoSPI.


## /PL HC165 potwierdzone bez przepinania sondy — 2026-10-02

Użytkownik potwierdził, że CH2 był i jest bezpośrednio na IC2 pin1 (/PL).
Nie trzeba było prosić o dodatkową sondę. Świeży odczyt `00 00` zwrócił 00.
CH2 /PL: LOW od 71.194 do 164.170 µs;
osiem narastających taktów payloadu od 79.998 do 149.998 µs
mieści się w LOW /PL. **FAIL: fizycznie potwierdzone blokowanie shiftowania przez /PL.**
D0=3,57 V zmierzone przez użytkownika. CH4=DIS B, nie D0. Potrzebna korekta
polaryzacji sterowania /PL, następnie ponowna walidacja bitów i timingów.
Dowody: `results/hc165-pl-confirmed.json`, CSV/BIN, `-edges.json`, SVG i UART.


## D0 HC165 zmierzone: 3,57 V — 2026-10-02

Użytkownik zmierzył 3,57 V bezpośrednio na IC2 pin11 (D0), przy 5 V na P5A pin2.
Zwora DIS B↔OK A usunięta; CH4 nadal na DIS B. To pomiar użytkownika,
nie oscyloskopowy pomiar D0. Kolejne trzy odczyty `00 00` zwróciły `00`.
**FAIL GPIO IN przy potwierdzonym HIGH na D0.** Dzielnik nie wyjaśnia zera.
Schemat ponownie sprawdzony: IC2 /PL pin1=CSn_HC165, CE pin15=GND,
CP pin2=SCK, D7 pin6=GND, Q7 pin9 przez IC6 do MISO. Przy /PL LOW
podczas całego payloadu rejestr pozostaje w parallel load, nie shiftuje;
Q7 odpowiada D7=0. Wymagana korekta sterowania /PL, bez software'owego
maskowania. Proponowane odwrócenie /PL względem lokalnego /CS wymaga
walidacji setup/hold i wyrównania bitów na rzeczywistym isoSPI; nie wolno
łączyć pin1 bezpośrednio z przeciwnym sygnałem bez odłączenia starej sieci.
Dowody: `results/hc165-external-5v.json`, `hc165-d0-3v57-uart.log`, PDF schematu.


## Doprecyzowanie sondy i zwory — 2026-10-02

Użytkownik potwierdził usunięcie zwory DIS B↔OK A przed testem z 5 V na P5A pin2.
CH4 pozostaje na wyjściu DIS B, tam gdzie była zwora. Nie mierzy teraz OK A ani
D0 HC165. Z dotychczasowego CH4 nie wyciągamy wniosków o poziomie wymuszonego
wejścia. Osiem odczytów GPIO IN=00 pozostaje FAIL, ale napięcie na D0 nadal
wymaga pomiaru. Następny punkt sondy: IC2 HC165 D0, pin11, za dzielnikiem OK A.
Aktualne ustalenie zastępuje wcześniejsze wpisy o niepotwierdzonym usunięciu zwory.
Dowód: `results/digital-probe-map-confirmed.json`. Bez nowych transakcji HW.


## Absolutny pomiar DAC2 przez Keysight SMU — PASS, 2026-10-02

Użytkownik potwierdził, że wcześniej przewody były w Sense; przepiął na Force.
Pierwsza próba Force: przy DAC3=0 SMU odczytał 1,49921 V (DAC2 pozostał midscale).
Następnie sweep tylko DAC2 wykazał, że punkt opisany przez użytkownika jako
pin18 złącza trzeciego kanału śledzi **DAC2**, a nie DAC3. To powiązanie
funkcjonalne z pomiaru; zgodność numeracji złącza i nazw sieci PDF nadal wymaga
sprawdzenia. Nie przypisujemy tego punktu do DAC3 na podstawie nazwy IN_N.

| DAC2 krok | Keysight SMU [V] | ADC3 CH3 | Wynik |
|---|---|---|---|
| 0% | -0.00017 | 0 | PASS |
| 25% | 0.74934 | 1022 | PASS |
| 50% | 1.49923 | 2045 | PASS |
| 75% | 2.24909 | 3069 | PASS |
| 100% | 2.99890 | 4095 | PASS |

**PASS** dla wyjścia DAC2 i loopbacku ADC3 CH3 na pięciu poziomach, próg
napięcia ±10 mV względem nominalnego VREF=3 V. Maksymalna odchyłka od nominalnej
charakterystyki DAC: 1.054 mV.
To test funkcjonalny, nie pełna kalibracja ani weryfikacja wszystkich parametrów
układów. VREF bezpośrednio niezmierzony. DAC3 nie ma jeszcze niezależnego pomiaru
napięcia; jego loopback CH4 był zaliczony wcześniej.

SMU: źródło 0 A, local sense, zakres prądu 10 nA, compliance 3,3 V,
pomiar napięcia 20 V. Końcowy stan potwierdzony: OFF/HIZ, SYST:ERR=0.
DAC2 i DAC3 przywrócono do 0x8000. GPIO IN pozostaje FAIL; nie zmieniono
GPIO OUT ani CPLD. Usunięcie zwory DIS B↔OK A przed wymuszeniem 5 V nadal
niepotwierdzone.

Dowody: `results/smu-pin18-dac2-sweep.json` + UART. Historia błędnego
podłączenia Sense: `smu-pin18-sense-connection-history.json`; próba DAC3
na Force: `smu-pin18-dac3-force-history.json`. Powtórka:
`scripts/smu_pin18_sweep.py --dac-channel 2` (Python z pyserial, grupa dialout).


## Powtórka po poprawieniu kabla i wymuszeniu wejścia 5 V — 2026-10-02

Użytkownik potwierdził, że kabel SMU wcześniej odpadł, następnie poprawił kabel.
Ponowna próba DAC3=0 dała ADC3 CH4=0, ale SMU=-0,73461 V.
**FAIL zgodności napięcia** — przerwano dalsze kroki zgodnie z bramką testu.
Nie rozstrzygamy przyczyny bez weryfikacji Force HI/LO, kontaktu i punktu pin18.
Poprzedni pełny sweep z odpadniętym kablem zachowany jako
`results/smu-pin18-cable-detached-history.json`; aktualny pomiar w
`smu-pin18-cable-reattached.json`. Stan końcowy SMU OFF/HIZ, bez błędów;
DAC2/DAC3=0x8000.

Użytkownik podał 5 V na P5A pin2 (PWR_STAGE_OK A). Schemat: R13=1,2 kΩ
szeregowo, R12=2,87 kΩ do GND; nominalnie na D0 ~3,53 V.
Osiem odczytów GPIO IN (cztery payload00, cztery FF) dało **00**, zamiast
oczekiwanego 01 przy jednym HIGH. **FAIL**. Napięcie D0 bezpośrednio niezmierzone.
/PL HC165 według schematu jest LOW przez cały payload — blokuje shiftowanie;
D7=GND, więc szeregowe Q7 pozostaje zerem. Wymaga fizycznej walidacji i korekty.
Nie wykonano zapisów GPIO OUT w tym teście. Wysłano pytanie, czy wcześniejsza
zwora DIS B↔OK A została usunięta przed podaniem 5 V; odpowiedź jeszcze nieznana.
Dowody: `results/hc165-external-5v.json` + UART.


## SMU po resecie — pomiar pin18, 2026-10-02

Po resecie Keysight B2910BL wrócił: SYST:ERR=0, OUTP=0, zapytania konfiguracji
odpowiadają. Skonfigurowano i zweryfikowano źródło CURR=0 A, zakres 10 nA,
local sense, compliance napięciowe 3,3 V, zakres pomiaru napięcia 20 V.
Dopiero po readback ustawień włączono przekaźnik pomiarowy. Nie wymuszano napięcia.

| Kod DAC3 | ADC3 CH4 | SMU GND↔pin18 [V] |
|---|---|---|
| 0 | 0 | -1.85548 |
| 16384 | 1022 | -2.05079 |
| 32768 | 2046 | -1.81394 |
| 49152 | 3070 | -1.93045 |
| 65535 | 4094 | -1.79609 |

**FAIL pomiaru napięcia na wskazanym punkcie:** SMU nie śledzi zmian DAC3,
mimo poprawnego readback i odpowiedzi ADC3 CH4. Brak podstaw do rozstrzygnięcia,
czy przyczyną jest punkt pomiaru, kontakt przewodów, zaciski Force/Sense czy SMU.
Według PDF P5 pin18=IN_N, pin12=VOUT_N bezpośrednio do ADC CH4; pin18 nie jest
w schemacie bezpośrednio połączony z DAC ani ADC. Weryfikacja wymaga HI FORCE
na P5 trzeciego kanału pin12, LO FORCE na GND. Użytkownik nadal ma pin18.

Stan końcowy potwierdzony: SMU OUTP=0, OFF MODE=HIZ, SYST:ERR=0;
DAC2=DAC3=0x8000. `scripts/smu_pin18_sweep.py` i
`results/smu-pin18-sweep.json` + UART. Kolejne wykonania skryptu zatrzymują
sweep po pierwszej niezgodności, aby najpierw sprawdzić połączenia.


## Pomiar na pinie 18 — Keysight SMU, 2026-10-02

Użytkownik doprecyzował: miernik to Keysight SMU, nie Keithley DMM7510.
Połączenie GND↔pin18 złącza trzeciego kanału. Dodatkowy sweep DAC3 na odłączonym
Keithley nie jest pomiarem napięcia PCB; daje jednak ADC3 CH4=0/1022/2046/3070/4094,
z potwierdzonym readback DAC. DAC2/DAC3 przywrócono do 0x8000.

Keysight B2910BL, MY63320305, 192.168.2.35:5025 odpowiada na *IDN?,
SYST:LANG?=DEF i SYST:ERR?. Zapytania stanu wyjścia/konfiguracji kanału timeout.
FETCh:VOLT? zwrócił ostatnią próbkę +10.00316 V (NIE bieżący pomiar);
MEASure:VOLTage? dał +9.91e37 — wynik nieważny, nie napięcie.
Następny odczyt błędu: -241, "Hardware missing; To recover channel, execute *TST?; Channel1".
**Pomiar absolutny BLOCKED przez błąd kanału SMU.** Wysłano AUTO OFF, OFF MODE HIZ,
OUTP OFF; readback OFF niedostępny, więc nie twierdzimy, że został potwierdzony.
Przed self-testem należy odłączyć przewody SMU od PCB. Nie wysłano OUTP ON.
Dowody: `results/smu-preflight.json`, `dmm-pin18-sweep.json` + UART.


## ADC/DAC loopback potwierdzony — 2026-10-02

Użytkownik potwierdził odłączony stopień mocy oraz VOUTC_N→INC_N i VOUTC_P→INC_P.
Fizyczne oznaczenia tych punktów nie są jednoznacznie przypisane do PDF, ale odpowiedź
analogowa potwierdza funkcjonalnie DAC2→ADC3 CH3 i DAC3→ADC3 CH4.

DAC CONFIG=0x0500 (zewnętrzna referencja, FSDO=1), GAIN=0x010F (DIV2, gain2
wszystkich wyjść): odczyt zgodny. STATUS zmienił się z 0x0001 na 0x0000 — alarm
referencji ustąpił. Nominalny zakres 0…3 V; VREF i napięcia wyjść nie są jeszcze
zmierzone niezależnie w tym teście. Starsze odczyty zer nie stanowią dowodu awarii ADC:
przed poprawną konfiguracją DAC miał aktywny alarm referencji.

| DAC2 krok | Kod DAC2 | ADC3 CH3 | ADC3 CH4 (DAC3=midscale) |
|---|---|---|---|
| 0% | 0 | 0 | 2046 |
| 25% | 16384 | 1022 | 2046 |
| 50% | 32768 | 2045 | 2046 |
| 75% | 49152 | 3069 | 2046 |
| 100% | 65535 | 4093 | 2046 |

**PASS względnego loopbacku.** Regresja wykonała 1090 transakcji SPI,
36 cykli niezależnych zmian DAC2/DAC3, readback obu kodów i odczyt wszystkich
24 wejść ADC. Zero błędów sterownika, zero niezgodności readback i loopbacku.
Maksymalna odchyłka CH3/CH4 od idealnego stosunku kodów: 3.0000 LSB ADC,
przy progu regresji 8 LSB. Pozostałe 22 wejścia odczytano, lecz nie zwalidowano
znanym sygnałem. Nie zaliczamy na tej podstawie pełnego ADC ani całej PCB.

**PASS funkcji aktualizacji synchronicznej DAC2/3:** SYNC=0x0F0C,
zapisy quarter/three-quarter nie zmieniły wejść ADC (2045/2046), dopiero
TRIGGER LDAC=0x0010 dał 1022/3069. Jednoczesność analogowa i skew niezmierzone.
Po testach SYNC=0x0F00, DAC2=DAC3=0x8000 (nominalnie różnica 0 V).

Użytkownik potwierdził po testach: multimetr był odłączony. Zapisane odczyty
DMM (-0,74…+1,26 mV) pochodzą z niepodłączonych wejść i nie są pomiarami DAC.
Pomiar napięć bezwzględnych oraz VREF: NOT_RUN. Loopback potwierdza stosunek
kodów ADC/DAC, bez niezależnej walidacji napięcia referencji. GPIO IN nadal nierozwiązane.

Powtarzalne skrypty: `scripts/analog_sweep.py`, `scripts/analog_regression.py`.
Uruchamiać przez istniejącą grupę dialout, Python z pyserial; wymagają
odłączonego stopnia mocy i potwierdzonego loopbacku. Dowody:
`results/analog-sweep.json`, `analog-regression.json` + UART,
`dac-sync-test.json` + UART i `analog-validation-summary.json`.
CPLD i magnetometr niezmienione.


## Pierwszy kontakt z ADC/DAC — 2026-10-02

Na polecenie użytkownika testy analogowe rozpoczęto osobno, mimo nierozwiązanego GPIO IN. CPLD bez zmian.

DAC: pierwsze readbacki były przesunięte o jeden bit. Zapis CONFIG=0x0500
wyłączył wewnętrzną referencję (zewnętrzne VREF przez R24) i ustawił FSDO=1,
zgodnie z datasheetem DAC80504. Powtórne odczyty: ID=0x0497, CONFIG=0x0500,
GAIN=0x0000, STATUS=0x0001 (REF-ALM), kod kanału DAC2=0x8000. Alarm referencji jest zgodny z brakiem zapasu napięcia przy obecnym REFDIV=0 i wymaga sprawdzenia po konfiguracji dzielnika. PASS dla ID i zapisu/odczytu CONFIG. Nie przesuwamy danych software'em.
Nie zmieniono kodów wyjść ani GAIN; sweep napięć oczekuje na identyfikację
kanału multimetru, wejścia loopback i połączeń ze stopniem mocy.

ADC: wysłano natywne komendy MCP3208 do adresów 02/03/04, wszystkie 8 kanałów.
Ostatnia seria 24 odczytów zwróciła zera. W pierwszej serii ADC3 wystąpiły również
surowe wartości 7; nie są to zwalidowane napięcia. Wynik FAIL / wymaga diagnostyki.
Potrzebny pomiar lokalnego CS ADC i MISO, oraz znany sygnał wejściowy.
DMM7510 w trybie DC V odczytał -1,154544 mV; punkt przyłączenia jeszcze niepotwierdzony.

Pełne surowe ramki i wyniki: `results/analog-first-contact.json` oraz wskazane tam UART logs.


## Po montażu C13 i ustawieniu SLOW=HIGH — 2026-10-02

Użytkownik potwierdził montaż C13 (wartość niepotwierdzona) oraz SLOW=HIGH.
Pomiary: `results/after-fix-out02*` i `results/after-fix-in02*`.

- SCK HIGH: mediana 1,027 µs, zamiast wcześniejszych ~114,5 ns. PASS dla minimum HIGH MCP3208; pozostałe parametry ADC nadal wymagają walidacji.
- OUT `01 02`: CSn_HC165 cały czas HIGH (3,24..3,60 V), brak wykrytego błędnego impulsu przy próbkowaniu 4 ns. B przechodzi HIGH po ramce (~164,19 µs). PASS dla tego przejścia.
- IN `00 00`: CSn_HC165 LOW od ~71,20 do ~164,17 µs; pierwszy payload SCK ~80 µs. B pozostaje HIGH (3,08..3,52 V). Wcześniejsze kasowanie OUT nie wystąpiło. PASS dla zachowania wyjścia.
- RX payload nadal `00`: GPIO IN nie przechodzi testu loopback. Nie potwierdzono poziomu bezpośrednio na wejściu IC2 ani poprawnego /PL; schemat wskazuje /PL LOW podczas shiftowania.
- Użytkownik podał dzielnik wejścia GPIO 1,2 kΩ : 2,87 kΩ. Nie potwierdzono orientacji ani strony zwory względem sondy; nie przypisujemy zmierzonych ~3,3 V bezpośrednio do wejścia IC2.

Wyjście B pozostawione HIGH po tej próbie do pomiaru wejścia. ADC/DAC nadal NOT_RUN. CPLD bez zmian. Starsze sekcje poniżej opisują stan sprzed korekty.


## Fizyczny test cyfrowy na zworze — 2026-10-02

CH4 przepięte przez użytkownika na DIS B ↔ OK A.

| Wzorzec OUT | Oczekiwany bit B | Zmierzona mediana | Wynik B |
|---|---|---|---|
| 00 | 0 | 0,12 V | PASS |
| 55 | 0 | 0,12 V | PASS |
| AA | 1 | 3,32 V | PASS |
| FF | 1 | 3,32 V | PASS |

To walidacja jednego wyjścia (QB), nie wszystkich QA…QH.
`02` ustawia B HIGH; przejście następuje po ramce, ~165,06 µs od pierwszego
SCK, ~14,95 µs po ostatnim opadającym SCK. Nie obserwowano zmiany B podczas payloadu.

**FAIL: odczyt GPIO IN narusza GPIO OUT.** Po ustawieniu B=HIGH ramka `00 00`
sprowadza B do LOW ~70,18 µs od pierwszego SCK, po ósmym takcie headera,
przed pierwszym taktem payloadu (~80 µs). RX payload=00. To dowód funkcjonalnego
skutku wyścigu dekodera/latch. Nie można przypisać dotychczasowych odczytów zer
wyłącznie do /PL HC165. Poprawić router i powtórzyć przy stałym HIGH na wejściu.

Dowody: `results/digital-physical-patterns.json`, `gpio-out02-full-frame.svg`,
`gpio-in-after02-false-latch.svg` oraz odpowiadające JSON/CSV/BIN i UART.
Po serii wysłano OUT=00. C13 pozostaje niepotwierdzony — po montażu ponowny test.
Nadal nie wykonano testów ADC/DAC i pełnej regresji 1000 transakcji z walidacją.

## Pomiary po odnalezieniu oscyloskopu — 2026-10-02

MSO4104: **192.168.2.4:4000**, C020817. DMM: **192.168.2.27:5025**.
Ścieżka do głowicy potwierdzona przebiegiem: module=0, chip=2, tryb ESP32=0,
100 kHz. CPLD bez zmian. Wykonano 14 transakcji SPI na głowicy.

| Test | Wynik | Dowód |
|---|---|---|
| Idle CSn_HC165 / Header_done | PARTIAL PASS: HIGH / LOW | idle-before-route.json + CSV/BIN |
| Header dla 00 00 | 8 taktów, potem wybór HC165 | router00.json / router00-edges.json |
| Przejście 00→01 | **FAIL: błędny CSn_HC165 LOW ~8 ns** | router01.json + router01-boundary.svg |
| Szerokość SCK | **FAIL dla MCP3208: ~114,5 ns vs min 250 ns** | router00-detail.json / -edges.json, 0,2 ns/sample |
| GPIO wzorce 00/55/AA/FF/02 | wysłane; IN zawsze 00, **loopback nie przechodzi** | gpio-patterns.json / -uart.log |
| Fizyczny OUT / atomic update | NOT_RUN; oczekuje na CH4 na zworze | aktualnie CH4=Header_done |
| ADC / DAC | NOT_RUN z powodu timingów routera i ADC | hardware-session.json |

Nie zmierzono wszystkich lokalnych CS jednocześnie ani CS_HEAD. Timingi relative
względem Header_done mają niekalibrowany skew sond/kanałów. Zegar oszacowano
z analogowego przecięcia 1,65 V. Cała ramka: 4 ns/sample; lokalny zoom: 0,2 ns/sample.

Przygotowana propozycja korekty: C13=47 pF C0G/NP0 (R2 2,87 kΩ, tau≈135 ns),
po montażu ponowny pomiar glitche na CS. Osobno SLOW LTC6820 HIGH zamiast GND
przez R19 — po zmianie ponowny pomiar szerokości SCK. Obsada obu zmian
pozostaje niepotwierdzona. Do przypisania błędu GPIO IN trzeba najpierw potwierdzić
oscyloskopem poziom na DIS B↔OK A; /PL HC165 po schemacie jest LOW podczas wyboru.

## Stan przygotowania przed pomiarami (historia)

Aktualizacja 2026-10-02: schemat otrzymany i przeanalizowany; minimalny raw probe
skompilowany w ESP-IDF 5.5.5 (**BUILD PASS**). Etap 0: **PARTIAL/BLOCKED**
(identyfikacja rzeczywistej obsady i połączeń w toku). Etapy HW 1–8 i regresja: **NOT_RUN**.

Bieżące ustalenia, poprawione adresy GPIO i problemy schematu: [SCHEMATIC_REVIEW.md](SCHEMATIC_REVIEW.md).
Kod i instrukcja: [firmware/README.md](firmware/README.md).
Raport przygotowania: `results/preparation.json`. CPLD bez zmian.

Pozostała część zachowuje checklistę z 2026-09-30; szczegóły ustalone teraz są
w przeglądzie schematu i mają pierwszeństwo przed wcześniejszymi niewiadomymi.
Sesja po podłączeniu 2026-10-02: potwierdzono MAC 68:25:dd:4c:49:e4,
wgrano minimalny probe i potwierdzono start/UART (**PASS**). SPI attempts=0.
CPLD bez zmian. Oscyloskop 192.168.2.9:4000 i DMM 192.168.2.10:5025 są
pod dawnymi adresami były nieosiągalne (TCP timeout / ARP incomplete).
DMM odnaleziono teraz pod **192.168.2.27:5025**, z tym samym numerem seryjnym;
odczyt DC -0,0007075753 V, punkt pomiarowy niepotwierdzony. MSO4104 nie
odnaleziono w skanie podsieci na TCP4000; test SPI pozostaje **BLOCKED**.
Wyniki odkrywania: `results/instrument-discovery.json`, `results/scope-discovery.json`.
Wyniki: `results/live-preflight.json`, `results/flash.json`, `results/runtime-idle.log`.
Mapowanie sond: CH1 SCK, CH2 CSn_HC165, CH3 MOSI, CH4 Header_done.
Użytkownik potwierdził poprawne napięcia, bez wartości liczbowych, i obsadę R24.
Loopback: trzecia grupa ADC↔DAC i PWR_STAGE_OK A↔PWR_STAGE_DIS B;
dokładne wejście analogowe wymaga potwierdzenia. Głowica na wyjściu huba 2,
magnetometr na wyjściu 1. Program nie inicjalizuje GPIO/SPI przed komendą route.
Brak pomiarów PCB; brak podstaw do PASS/FAIL elektrycznego.
Wyniki identyfikacji środowiska: `results/stage0.json`.

CP2102 (serial 0001) jest pod `/dev/ttyUSB2`; stabilna ścieżka:
`/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0`.
Historyczny raport `../bridge-spi-inspection/NOTES.md` wskazuje ESP32-D0WD-V3
rev 3.1, MAC 68:25:dd:4c:49:e4, pracujący z CPLD/RTD16. To nie jest aktualna
identyfikacja nowej głowicy. `/dev/ttyUSB0` i `/dev/ttyUSB1` to interfejsy Lattice;
`/dev/ttyACM0` jest urządzeniem QinHeng, bez potwierdzonego przypisania do tej głowicy.

Odpowiedzi na wykonane teraz, wyłącznie odczytowe `*IDN?`:

| Przyrząd | Endpoint | Tożsamość |
|---|---|---|
| Oscyloskop | 192.168.2.9:4000 | Tektronix MSO4104, C020817 |
| DMM | 192.168.2.10:5025 | Keithley DMM7510, 04679690 |

Sterowniki są w `../lab-instruments`. Fizyczne połączenia sond i zasilacza
z nową PCB nie są znane. Samo połączenie LAN nie stanowi dowodu pomiaru PCB.

W przeszukanych lokalizacjach znaleziono projekt `moduliq-debug-head`, dokumenty
Hub ESP32/RTD16 oraz komponent ESPHome hc138. Przed otrzymaniem PDF nie potwierdzono, aby którykolwiek
opisywał tę nową PCB. Teraz źródłem jest `inputs/MOD-SH-MAG_COMP-36.PDF`. Dostępny komponent hc138 ustawia adres trzema GPIO — nie
realizuje protokołu adresowego z bajtem nagłówka. Nie używać go jako zamiennika.

## Warunki odblokowania etapu 0

- Potwierdzić port, rewizję PCB i fizyczne połączenie ESP32 z głowicą.
- Schemat otrzymany; potwierdzić BOM/netlistę i oznaczenia zamontowanych IC.
- Potwierdzić numery GPIO SCK/MOSI/MISO/CS_HEAD; nie kopiować pinów z RTD16.
- Ustalić punkty pomiarowe, podłączenie sond i parametry zasilania.
- Dla każdego IC zapisać producenta, pełny numer, obudowę, designator i rewizję
  datasheetu. Dopiero wtedy porównać numery pinów i sieci.

| Blok | Znane z wymagań | Zamontowany typ / pinout |
|---|---|---|
| Licznik | CD74HC4520M96 IC16 wg PDF | obsada niepotwierdzona |
| Router shift register | SN74HC595D IC12 wg PDF | obsada niepotwierdzona |
| Dekoder | CD74HC138M96 IC13 wg PDF | obsada niepotwierdzona |
| GPIO OUT | SN74HC595D IC9 wg PDF | obsada niepotwierdzona |
| GPIO IN | 74HC165D IC2 wg PDF | obsada niepotwierdzona |
| DAC | DAC80504BRTER IC10 wg PDF | obsada niepotwierdzona |
| ADC X/Y/Z | 3 × MCP3208-CI/SL wg PDF | obsada niepotwierdzona |

## Power-off

Odłączyć wszystkie źródła zasilania, także USB i możliwe zasilanie przez sygnały.
Sprawdzić zwarcia szyn do GND i między szynami; zapisać rezystancje po ustaleniu
odczytu, uwzględniając ładowanie kondensatorów. Sprawdzić orientację pin 1,
obudowy, lutowanie i zgodność z BOM. Porównać VCC/GND/VREF każdego IC z jego
datasheetem. Zweryfikować RESET, OE, ENABLE, pull-up/pull-down, reset licznika
przez CS_HEAD oraz napięcia logiczne na granicy ESP32/PCB. Nie zgadywać pinoutu.

## Etap 1 — pomiary przed SPI

Ustalić napięcia i limit prądu na podstawie schematu; nie przyjmować wartości
domyślnych. Zmierzyć każdą szynę przy IC, pobór prądu i VREF ADC/DAC. Sprawdzić
wszystkie lokalne CS, również spare. Każdy musi być HIGH w idle.
Jakikolwiek LOW oznacza FAIL i zatrzymanie; znaleźć przyczynę przed SPI.

Każdy zapis pomiaru ma zawierać punkt, wartość/jednostkę, oczekiwany przedział
z jego źródłem, przyrząd/sondę, czas, PASS/FAIL i plik dowodowy.
Szablon: `results/measurement-template.json`. Brak odczytu = NOT_RUN/BLOCKED.

## Protokół i adresy

ESP32 ma jeden fizyczny slave. Transakcja: CS_HEAD LOW, `[TARGET][payload]`,
CS_HEAD HIGH. Nagłówek i payload muszą być jednym hardware SPI burst.
Numery GPIO, tryb SPI, kolejność bitów, poziomy logiczne i dopuszczalny zegar
pozostają niezweryfikowane do analizy schematu/datasheetów.

| TARGET | Urządzenie |
|---|---|
| 0x00 | GPIO_IN (wg otrzymanego schematu) |
| 0x01 | GPIO_OUT (wg otrzymanego schematu) |
| 0x02 | ADC_X |
| 0x03 | ADC_Y |
| 0x04 | ADC_Z |
| 0x05 | DAC |
| 0x06 | spare |
| 0x07 | spare |

## Etap 2 — router (dopiero po PASS 0/1)

Minimalny firmware ma być sterowany ręcznie, bez automatycznych ramek przy
starcie. Wysyła kolejno `00 00` … `07 00`. Nie inicjalizuje ADC ani DAC.
Przed tym testem ustalić z datasheetów, czy jeden bajt payloadu jest bezpieczny
dla podłączonych urządzeń; brak init nie gwarantuje braku skutków ramki.

Dla każdego adresu rejestrować CS_HEAD, SCK, MOSI i wszystkie lokalne CS:
8 taktów nagłówka przy wszystkich CS HIGH; potem tylko właściwy CS LOW;
pierwszy takt payloadu przy aktywnym CS; zwolnienie po CS_HEAD HIGH;
ponowne liczenie od zera w następnej transakcji. Sprawdzić przejścia między
różnymi adresami, szczególnie 0↔7 i powtórzenia tego samego adresu.

Osobną akwizycją analogową zbadać HEADER_DONE, latch 595, A/B/C i enable 138
oraz Yn. Zmierzyć opóźnienia względem odpowiednich zboczy, szerokości impulsów
i margines do pierwszego zegara payloadu. Szukać impulsu na błędnym CS.
Jeżeli jest RC enable, zmierzyć jego narastanie i progi; porównać z datasheetem.
Zapis jitteru i rozdzielczości akwizycji jest wymagany. LA sam może nie wykryć
wąskiego glitcha. Nie maskować problemu dodatkowym software delay.

## Etapy 3/4 — GPIO

OUT: ramki `01 00`, `01 55`, `01 AA`, `01 FF`; mierzyć QA..QH, kolejność bitów
i moment latch. Wyjścia mają zmienić się atomowo po lokalnej ramce.
IN: wymusić 00/01/55/AA/FF, odczytać 8 bitów, zweryfikować parallel load,
shift i tri-state MISO. Testować także nieaktywne urządzenia oraz konflikt
na wspólnej linii. Zapis TX/RX bez wymuszenia wejść nie potwierdza poprawności.

## Etap 5 — generic SPI

Dopiero po PASS routera i GPIO przygotować `addressed_spi_write` oraz
`addressed_spi_transfer`. Jeden hardware transfer bufora `[address][payload]`,
jedno CS_HEAD, RX bez pierwszego bajtu. Dla payloadów 1–8 B użyć stałych buforów
bez heap; nie dodawać osobnego transferu nagłówka. Zmierzyć granicę bajtów LA/scope.
Nie implementowano helpera przed wymaganym przez użytkownika PASS etapów 0–4.

## Etapy 6/7 — DAC/ADC

Po identyfikacji rzeczywistych IC pobrać aktualne datasheety producenta.
DAC: bezpieczny stan wszystkich kanałów, reference, zakres, kody 0/25/50/75/100%
z pomiarami DMM i tolerancjami; sprawdzić update synchroniczny, jeśli dostępny.
Jeśli DACx0504, mapowanie DAC0=X, DAC1=Y, DAC2=Z, DAC3=VZERO.
Bezpiecznego napięcia nie utożsamiać automatycznie z kodem zero.

ADC: najpierw jeden kanał, potem osiem, potem SCAN. Jeśli MCP3564/R,
zweryfikować rejestry i polling ze źródłem producenta. Parametry sample_rate/OSR,
gain i reference oddzielić od update_interval publikacji (np. 1 s).
Polling SPI bez dodatkowego DRDY, jeśli wystarcza. Testować każdy ADC X/Y/Z.

## Etap 8 i regresja

Po potwierdzeniu low-level SPI przygotować ESPHome z lambdami/własnym .h,
init po inicjalizacji SPI i osobną warstwą addressed-SPI.
Regresję sprzętową przygotować po identyfikacji układów i punktów pomiarowych:
GPIO 00/55/AA/FF z fizycznym odczytem lub potwierdzonym loopback,
kilka kodów DAC z DMM, wszystkie kanały wszystkich ADC z granicami dla znanych
sygnałów. Wykonać ≥1000 transakcji i raportować liczbę, błędy drivera,
odczytu rejestrów/CRC (jeśli wspierane), timeouty i niespójności danych.
Sukces funkcji SPI nie dowodzi odpowiedzi slave. Nie raportować całej regresji
PASS, jeśli dowolny wymagany pomiar jest nieobecny. Zapisywać dane surowe,
konfigurację, wersję firmware i raport w `results/`.

## Przebiegi, problemy, zmiany PCB

Zmierzone przebiegi/timingi: brak — NOT_RUN.
Potwierdzone problemy PCB: brak danych; nie oznacza to poprawności PCB.
Blokady środowiskowe opisano powyżej. Rekomendacje zmian PCB pozostają otwarte
do analizy schematu i pomiarów; nie wydano zaleceń opartych na domniemanym pinoucie.
Minimalny raw probe jest przygotowany i skompilowany (2026-10-02); uruchomienie
na PCB, firmware ESPHome i regresja sprzętowa oczekują na przejście etapów HW.
