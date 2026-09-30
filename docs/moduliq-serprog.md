# MODULIQ serprog przez TCP

Opcjonalny `moduliq_serprog` przekazuje surowe transakcje SPI do istniejącego
`moduliq_cpld_flash`. Baza układów, adresowanie, odblokowanie, programowanie,
kasowanie, opóźnienia i weryfikacja pozostają w flashrom na komputerze.
Driver nie rozpoznaje PROJECT_ID, kodów komend ani bitów statusu pamięci.
Nie odpytuje ADC temperatury i nie konfiguruje DRDY.

```yaml
moduliq_serprog:
  id: flash_programmer
  flash_id: target_flash
  port: 6054
  enabled: false
```

Dodaj komponent do `external_components`, zachowując `spi`, `addrspi`,
`moduliq_cpld_gpio` i `moduliq_cpld_flash` z tej kolekcji. Wymagane jest
`network` przez Wi-Fi/Ethernet. Przykład sprawdzono na ESP32/ESP-IDF z domyślnym
BSD socket backend ESPHome 2026.9.0. Serwer słucha IPv4 na porcie z YAML;
wyłączenie zamyka listener. Domyślnie nie otwiera portu ani nie przejmuje pamięci.
Wiele instancji wymaga różnych `flash_id` i portów.

[Przykład YAML](../examples/cpld-serprog.yaml) jest przeznaczony do kompilacji.
Ilustracyjny PROJECT_ID 0x42 nie jest profilem prawdziwej płytki. Gotowość targetu
pozostaje false, dopóki polityka aplikacji nie wykona i nie potwierdzi właściwej
procedury. Przykładowe sprawdzanie RDSR/busy jest polityką jednego profilu NOR,
nie wbudowanym algorytmem serwera. Przed zastosowaniem trzeba zastąpić procedury
przygotowania, oceny bezczynności i przywracania właściwymi dla danej płytki.

## Sesja i automatyzacje YAML

1. Przy przyjęciu klienta serwer rezerwuje wskazany driver Flash i emituje
   `on_prepare(session)`. Nie ustawia jeszcze CFG ani nie wysyła SPI.
2. YAML wykonuje wybraną po PROJECT_ID procedurę izolacji/resetu/boot.
   `confirm_target_ready(session)` jest jawnym potwierdzeniem, że target jest
   gotowy do bezpiecznego przejęcia, w tym że pamięć nie wykonuje operacji
   rozpoczętej wcześniej przez target. Dopiero wtedy driver przejmuje Flash.
   Negocjacja protokołu może trwać wcześniej; kompletne `O_SPIOP` czeka bez ACK.
3. Po EOF, błędzie TCP, wyłączeniu lub upływie `session_timeout` serwer zamyka
   klienta i emituje `on_release_requested(session)`. Limit bezczynności wynosi
   domyślnie 30 s, zakres YAML 1 s–1 h. Przygotowanie ma niezależny limit od jego
   początku: NOP-y nie przedłużają go bez końca.
4. Jeżeli wysłano surową transakcję, dzierżawa pozostaje aktywna do potwierdzenia
   zakończenia operacji pamięci. YAML może używać
   `cleanup_transfer(session, tx, write_size, rx, read_size)` do odczytu statusu
   wybranego profilu i wywołać `confirm_release_ready(session)` dopiero po
   wiarygodnym potwierdzeniu bezczynności. Długość programowania/kasowania nie
   jest zgadywana, a timeout sieciowy nie zwalnia pracującego układu.
5. Driver zwalnia CFG z weryfikacją. Nieudane zwolnienie zachowuje dzierżawę
   i jest ponawiane co 100 ms. `on_released(session)` następuje dopiero po
   potwierdzonym zwolnieniu oraz zakończeniu rezerwacji software. Wtedy YAML
   może przywrócić target i wyłączyć jego GPIO/Flash.

Token `session` ma typ `uint32_t`; potwierdzenia dotyczą tylko odpowiedniego
etapu i generacji. `current_session()` i `release_pending()` służą do warunków
skryptów. Nie zachowuj tokenów w trwałej pamięci. W
`on_release_requested` zatrzymaj opóźniony skrypt przygotowania; w `on_released`
zatrzymaj skrypt sprzątania. Zapobiega to działaniu starej polityki w nowej sesji.

Bez wysłanych transakcji dzierżawa może zostać zwolniona automatycznie, również
po anulowanym przygotowaniu. Brak `on_prepare` blokuje przejęcie do timeoutu.
Brak potwierdzenia bezczynności po SPI pozostawia pamięć zarezerwowaną bez
limitu czasu. Drugi klient jest zamykany także podczas przygotowania/sprzątania;
nie może przejąć dzierżawy ani zmienić CS innej sesji.

Podczas sesji zwykłe `acquire`, `transfer`, `release`, `transaction` i shutdown
Flash nie obchodzą rezerwacji. `set_enabled(false)` na Flash zapisuje wyłączenie,
ale zwraca false i nie porzuca dzierżawy sesji. Surowe odczyty sprzątania nadal
są dostępne. GPIO potrzebne do CFG musi pozostać aktywne do `on_released`.
Serwer nie potrafi zapewnić bezpieczeństwa po zaniku zasilania, sprzętowym
resecie ESP32 lub wymuszonym restarcie; procedura rozruchu płytki pozostaje
polityką aplikacji.

## Protokół i rzeczywiste ograniczenia

Audyt oparto na
[specyfikacji serprog v1](https://github.com/flashrom/flashrom/blob/204d0f2184d1c3a268f99443b1034ba18ffe3ad8/doc/supported_hw/supported_prog/serprog/serprog-protocol.rst)
i [kliencie flashrom](https://github.com/flashrom/flashrom/blob/204d0f2184d1c3a268f99443b1034ba18ffe3ad8/programmers/serprog.c).
Wersja interfejsu to 1; wartości wielobajtowe są little-endian, długości SPI
24-bit. `SYNCNOP` zwraca NAK+ACK. Bitmapa zgłasza dokładnie:

| Kod | Operacja | Wynik |
|---|---|---|
| 00 | NOP | ACK |
| 01 | Q_IFACE | ACK + uint16 1 |
| 02 | Q_CMDMAP | ACK + 32 bajty bitmapy |
| 03 | Q_PGMNAME | ACK + 16 bajtów, dopełnienie NUL |
| 04 | Q_SERBUF | ACK + 65535; backpressure TCP |
| 05 | Q_BUSTYPE | ACK + 08, tylko SPI |
| 08 | Q_WRNMAXLEN | ACK + uint24 1024 |
| 10 | SYNCNOP | NAK + ACK |
| 11 | Q_RDNMAXLEN | ACK + uint24 1024 |
| 12 | S_BUSTYPE | ACK dla 08, inaczej NAK |
| 13 | O_SPIOP | zapis i odczyt pod jednym CS |

Każda faza obejmuje maksymalnie 1024 bajty. Limit zapisu obejmuje także opcode,
adres i dummy bytes; nie oznacza rozmiaru strony układu. Dwie fazy dają najwyżej
2048 bajtów i mieszczą się w limicie ramki drivera Flash 4096 bajtów oraz w
pojedynczych transferach delegate ESP-IDF (4092 bajty). Próba przekroczenia limitu
lub obu długości zero zwraca NAK i zamyka strumień bez taktowania SPI. Bufory są
stałe: żądanie 1031 bajtów, odpowiedź 1025 bajtów. Nieznana komenda także kończy
strumień po NAK, aby jej payload nie został potraktowany jako nowe komendy.

Serwer przyjmuje cały zapis przed opuszczeniem CS, wykonuje obie fazy i podnosi
CS przed wysłaniem odpowiedzi TCP. Fragmentacja i backpressure nie przedłużają
CS. Nie dzieli dużej transakcji na niezależne ramki SPI. Górny limit ramki przy
100 kHz oznacza około 164 ms samego taktowania; YAML dobiera częstotliwość do
właściwego układu i wymagań aplikacji.

Nie zgłasza bufora operacji równoległych, zmiany częstotliwości/mode/CS ani
S_PIN_STATE. Parametry SPI pochodzą z jawnego `spi_mode`/`data_rate` Flash w YAML;
tryb SPI serprog (half/full duplex) jest odrębny od CPOL/CPHA. Klient flashrom
korzysta z domyślnego half-duplex i automatycznego CS. Nie używaj `cs=` do wyboru
pamięci: każda pamięć ma swój endpoint TCP. `spispeed=` nie zmienia konfiguracji
serwera. Algorytmy/układy wymagające większych pojedynczych ram nie są objęte
potwierdzoną zgodnością i otrzymają NAK.

Wspólna magistrala jest używana wyłącznie w kooperatywnej pętli ESPHome, bez
zadań roboczych. Frame nie wywołuje automatyzacji i nie oddaje sterowania w trakcie
CS. Zagnieżdżone addrspi odtwarza GA/CS przed każdą ramką, również gdy różne
instancje współdzielą fizyczne piny adresowe; delegate ESP-IDF blokuje
magistralę fizyczną przez oba etapy. Globalny guard Flash odrzuca wywołania reentrant
między instancjami. Dzierżawa pamięci nie blokuje magistrali podczas oczekiwania
na hosta lub podczas wewnętrznego kasowania: inne instancje mogą wtedy wykonywać
swoje kompletne ramki. API komponentu jest przeznaczone do pętli głównej,
nie do ISR lub innych wątków.

SPI delegate nie zwraca statusu błędów transferu. ACK oznacza wykonanie wywołania
drivera, a poprawność danych/protokołu pamięci sprawdza flashrom; polityka YAML
musi także sprawdzać wiarygodność odpowiedzi przy sprzątaniu. Brak komunikacji
z pamięcią nie uprawnia do wymuszonego zwolnienia.

## Testy software i użycie klienta

```sh
./scripts/test-serprog.sh
./scripts/test-cpld.sh
./scripts/test-serprog-flashrom.sh
HIL_STATE_DIR="$PWD/artifacts/serprog-build-state" \
  ../esphome-hil/scripts/build.sh "$PWD/examples/cpld-serprog.yaml"
```

Entry points wymagają przypiętego Nix z `esphome-hil`; brak środowiska nie włącza
fallback Python. Skrypt flashrom rozwiązuje pakiet 1.7.0 z tego samego przypiętego
nixpkgs i zachowuje logi pod `artifacts/`. Test korzysta z rzeczywistego kodu
serwera TCP, Flash i addrspi; POSIX adapter zastępuje socket backend, a pamięć
NOR i piny są emulowane. Obejmuje probe/read/write/verify/erase oraz przerwanie
po rozpoczęciu kasowania. Kod emulacji układu znajduje się wyłącznie w testach.

Przykładowa komenda odczytu po wdrożeniu właściwej polityki płytki:

```sh
flashrom -p serprog:ip=192.0.2.10:6054 -r image.bin
```

[Raport](../reports/serprog-software-2026-09-30.json) rozdziela audyt,
testy hostowe, zgodność flashrom i kompilację ESP32. W tym zadaniu nie otwierano
interfejsów sprzętowych, nie resetowano targetu ani nie programowano urządzeń.
