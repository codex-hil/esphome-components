# MOD-SH-MAG_COMP-36 — przegląd 2026-10-02

Źródło: dostarczony PDF, 4 strony, tytuł MOD-SH-MAG_COMP-36, rev 1.0.
PDF opisuje schemat, nie potwierdza obsady ani połączeń rzeczywistej PCB.
Ślad wejść i build: `results/preparation.json`. CPLD nie został dotknięty.

## Ustalona topologia

ESP32 → istniejące GPIO adresu modułu (4 bity) i chipu CPLD (2 bity)
→ fizyczny SPI/isoSPI → LTC6820 na głowicy → router bajtu TARGET
→ lokalny payload ADC/DAC. Tylko trzeci poziom jest bajtem w strumieniu SPI.
Nie dopisywać adresów modułu/chipu jako kolejnych bajtów.
GPIO adresowe zmieniać przy nieaktywnym CS, przed hardware burstem.

| Adres TARGET | Dekoder | Sieć | Urządzenie według PDF |
|---|---|---|---|
| 0x00 | IC13 pin 15, Y0 | CSn_HC165 | IC2 74HC165D, GPIO IN |
| 0x01 | IC13 pin 14, Y1 | CSn_HC595 | IC9 SN74HC595D, GPIO OUT |
| 0x02 | IC13 pin 13, Y2 | CSn_ADC1 | MCP3208-CI/SL, blok 1 |
| 0x03 | IC13 pin 12, Y3 | CSn_ADC2 | MCP3208-CI/SL, blok 2 |
| 0x04 | IC13 pin 11, Y4 | CSn_ADC3 | MCP3208-CI/SL, blok 3 |
| 0x05 | IC13 pin 10, Y5 | CSn_DAC | IC10 DAC80504BRTER |
| 0x06 | IC13 pin 9, Y6 | niepodłączone | spare |
| 0x07 | IC13 pin 7, Y7 | niepodłączone | spare |

Adresy GPIO IN/OUT są odwrotne względem pierwszego prompta.
Router: IC12 SN74HC595D, IC13 CD74HC138M96, IC16 CD74HC4520M96.
MISO GPIO IN jest buforowane przez IC6 SN74LVC1G125DBV.
Transport: IC15 LTC6820HMS, MSTR/POL/PHA/SLOW do GND przez rezystory 0 Ω.

IC16A: SCK → E pin 2; Q3 pin 6 → CP pin 1 i Header_done; CSn → MR pin 7.
Zgodnie z tabelą 4520 przy CP LOW licznik reaguje na opadające E.
Po ósmym opadającym SCK Q3 przechodzi HIGH i blokuje dalsze zliczanie.
Header_done łapie dane na IC12 RCLK pin 12; R2=2,87 kΩ opóźnia enable
IC13 E3 pin 6 przez C13, którego wartość w PDF to **Undefined**.
IC13 E2 pin 5 = CSn, E1 pin 4 = GND. Należy zmierzyć kolejność latch→enable
i reset Q3 po zwolnieniu CSn. Nie utożsamiać idealnego schematu z timingiem PCB.
[TI CD74HC4520](https://www.ti.com/lit/ds/symlink/cd74hc4520.pdf),
[TI HC595](https://www.ti.com/lit/ds/symlink/sn74hc595.pdf),
[TI HC138](https://www.ti.com/lit/ds/symlink/cd74hc138.pdf).

## Problemy wynikające z analizy schematu

1. **GPIO IN /PL**: IC2 pin 1 i OE bufora IC6 pin 1 są sterowane
   CSn_HC165. LOW aktywujące MISO jednocześnie wymusza ciągły parallel load.
   Serial shift wymaga /PL HIGH. D7 jest do GND, więc odczyt samych zer
   nie potwierdzi SPI. Kandydat poprawki: /PL aktywne w idle i zwalniane
   podczas wyboru, CE kontrolujące shift; zweryfikować projekt poprawki
   wraz z momentem pierwszego bitu. Nie obejść tego sterownikiem.
   [Nexperia HC165](https://assets.nexperia.com/documents/data-sheet/74HC_HCT165.pdf?hkey=EF798316E3902B6ED9A73243A3159BB0).

2. **DAC REF**: REF pin 1 łączy się przez R24=0 Ω z REF3430 (3 V),
   a DAC domyślnie wystawia własną referencję 2,5 V. Wymaga sprawdzenia obsady R24
   i rozwiązania konfliktu źródeł już przy power-up; szybki zapis CONFIG
   nie usuwa konfliktu przed pierwszą transakcją. Do testu wewnętrznej referencji
   konieczne jest fizyczne odizolowanie REF od zewnętrznego źródła.
   [TI DAC80504, §8.3.2](https://www.ti.com/lit/ds/symlink/dac80504.pdf).

3. **DAC headroom**: VDD=P3V3A, REFDIV=LOW. Dla zewnętrznych 3 V i VDD≈3,3 V
   dzielnik musi być włączony według tabeli §7.3. Gain=2 z DIV=2 daje nominalny
   zakres 0…3 V. Strap REFDIV LOW może po reset powodować REF alarm i wyjścia
   0 V. Sprawdzić VDD, reference i rejestry zanim uznamy zerowe wyjście za błąd SPI.
   RSTSEL HIGH oznacza midscale przy reset, LDAC jest na stałe LOW.
   Aktualizację jednoczesną można badać przez SYNC i software LDAC.
   [TI DAC80504](https://www.ti.com/lit/ds/symlink/dac80504.pdf).

4. **Lokalna szerokość SCK**: LTC6820 ma SLOW=LOW (fast). Lokalne pulsy są
   regenerowane; zmiana zegara ESP32 zmienia odstęp, nie zapewnia wydłużenia
   impulsów. Fast dopuszcza pulsy 100 ns, a MCP3208 wymaga tHI/tLO≥250 ns.
   Pomiar na głowicy jest konieczny. Rozważyć strap SLOW HIGH dopiero na podstawie
   pomiaru/analizy; nie zmieniać CPLD. Przy slow minimalny okres mastera to 5 µs.
   DAC próbuje SDI na falling, 595 na rising; LTC6820 ustawia MOSI przed lokalnym
   pulsem. Należy zbadać setup/hold przy obu zboczach i MISO. Tryb ESP32 musi
   odpowiadać LTC6820 po stronie mastera, którego ustawień ten PDF nie zawiera.
   [ADI LTC6820, §Operation](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc6820.pdf),
   [Microchip MCP3208, §1](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21298e.pdf).

## Piny do pomiarów czterema sondami

Każda seria używa tego samego wzorca transakcji i zapisuje mapę sond.
Sondy ×10, krótkie masy wyłącznie do GND głowicy; nie do DAC3_N ani N5V0A.
Pomiar single-shot na powtarzalnej ramce, z dostateczną rozdzielczością dla glitchy.

| Seria | CH1 | CH2 | CH3 | CH4 | Cel |
|---|---|---|---|---|---|
| A | SCK, IC12.11 | MOSI, IC12.14 | Header_done, IC16.6 | wybrany CS, IC13.Yn | header 8 bitów, latch, wybór, szerokość zegara |
| B | SCK | CSn_HEAD, IC15.5 | enable IC13.6 za R2 | wybrany / błędny CS | idle, zwolnienie, RC i glitche |
| C | SCK | MOSI | MISO | lokalny ADC/DAC CS | native payload, odpowiedź, setup/hold |
| D | SCK | lokalny CS | MOSI lub Header_done | wyjście DAC | zapis i aktualizacja analogowa |

Powtarzać A/B dla wszystkich Y0…Y7, również niepodłączonych wyjść dekodera;
z czterema kanałami nie da się zaobserwować wszystkich ośmiu CS naraz.
Zapis kolejnych akwizycji nie uprawnia do twierdzenia, że obserwowano wszystkie
CS jednocześnie. DMM mierzy DAC względem GND; osobno można zmierzyć DACn−DAC3_N.

## ADC i loopback

MCP3208 jest 12-bitowym SAR bez OSR, programowalnego gain ani rejestru SCAN.
Tempo konwersji wynika z transakcji; update_interval jest tempem publikacji.
Istniejący komponent `../../components/mcp3208` jest właściwym
punktem wyjścia, lecz `read_raw_` robi trzy `transfer_byte`; należy zastąpić je
jedną pełną ramką i decode RX po odrzuceniu bajtu headera.
Komponent DACx0504 ma ten sam problem z podziałem ramki na bajty.
Istniejące GPIO-mux addrspi mogą tworzyć dwa zewnętrzne poziomy; nowa warstwa
serialnego adresowania nie powinna wysyłać headera w `begin_transaction()`
osobnym transferem. Wymagana jest jawna operacja na całej ramce.

ADC kanały 0…7: P5V_stat, N5V_stat, VCC_stat, VOUT_P, VOUT_N, TEMP, ERROR,
IMON. Pierwsze sześć nie ma na tej stronie dzielników; nazwy nie dowodzą
napięć dopuszczalnych. ERROR/IMON mają dzielniki 2,87 kΩ / 2,87 kΩ.
Nie podawać 5 V ani napięcia ujemnego bez sprawdzenia rzeczywistych poziomów.

BNC J2/SW1 przełącza parę IN_P/IN_N oraz DAC_P/DAC_N; IN_P/IN_N nie trafiają
wprost do MCP3208 na pokazanym schemacie. Sam kabel DAC→J2 nie jest jeszcze
loopbackiem ADC. Ustalić konkretny kanał ADC (np. odłączony VOUT_P na P5.10→CH3)
i odizolować dotychczasowe źródło. Zakres single-ended 0…zmierzone VREF.
Nie podłączać równolegle dwóch wyjść. Sprawdzić każdą z trzech kopii ADC_Channel.
[Microchip MCP3208](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/21298e.pdf).

## Stan

Analiza schematu: wykonana; problemy powyżej są wnioskami ze schematu/datasheetów.
Obsada PCB, power-off, power-up, przebiegi SPI, DAC DMM i loopback: **NOT_RUN**.
Build minimalnego programu: **PASS**, bez flashowania i bez pomiarów HW.
Adres modułu/chipu, aktualne podłączenie ESP32 i mapy sond: oczekują na potwierdzenie.
Finalne trzy komponenty ESPHome pozostają za bramką walidacji sprzętowej.
