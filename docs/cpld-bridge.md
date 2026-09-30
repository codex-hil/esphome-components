# Trzy komponenty ESPHome dla MODULIQ CPLD Bridge

Implementacja trzech komponentów jest dostępna w `components/`.
[Instrukcja, przykłady i polityka własności](cpld-components-usage.md)
opisują działające schematy YAML i API. Zakres weryfikacji to testy software
oraz kompilacje w przypiętym Nix; sprzęt nie był programowany ani testowany.
Kontrakt audytowano przy `MODULIQ/CPLD_bridge` commit
`2b0752faf76c84b76e593727a427b732e0b44b99`, na bazie dokumentacji biblioteki
`113a2d7`. Wspólny helper jest częścią GPIO, bez czwartego publicznego komponentu.

Poniżej zachowujemy wymagania kontraktu HDL. Jawne decyzje implementacji
(odrzucanie kolizji ADC, rezerwacja Flash, publikacja po cleanup) są w instrukcji.

| Komponent | Transport | Odpowiedzialność |
|---|---|---|
| `moduliq_cpld_i2c` | I²C, adres 0x50 + GA | identyfikacja płyty i rozszerzeń, statusy GPIO, liczniki błędów, opcjonalny ADC identyfikacyjny |
| `moduliq_cpld_gpio` | adresowany SPI, subadres/CS3, mode 0 | dwa banki GPIO, DIR, push-pull/open-drain, odczyt padów, współdzielony dostęp do CFG |
| `moduliq_cpld_flash` | dane przez adresowany SPI subadres/CS2; sterowanie CFG przez CS3 | przejęcie współdzielonej Flash, transfery, zwolnienie linii dla targetu |

Bieżący kontrakt HDL: `18c18ff51ce8a689fd4717d73e6e8332b5c58c64`, gałąź
`feature/rev11-project-pll`. Stan sprzętu pozostaje przy testowanym obrazie
`fae8341`; nowe ADC i runtime ENABLE mają symulacje/buildy, bez testu na płytce.
[Pełne przekazanie](https://github.com/MODULIQ/CPLD_bridge/blob/e8d225aecaec8ec17b3054724a4d4936dca1ec43/docs/esphome-driver-handoff.md) zawiera piny, fixture i wyniki HIL.
[Kontrakt JSON](cpld-register-contract.json) jest indeksem interfejsów.

## 1. I²C readout i identyfikacja

Ten komponent działa samodzielnie, także gdy CS3 nie udostępnia expandera,
bo wszystkie chip selecty obsługują chipset SPI. Nie wymaga SPI do odczytu
GPIO/ID, liczników ani sterowania nowym ADC. Nie ustawia DIR/DATA/OD przez I²C;
takich rejestrów GPIO zapisu nie ma.

| Adres I²C | Dane / skutek uboczny |
|---|---|
| `0x00` | PROJECT_ID, 8 bitów |
| `0x01` | REV_ID, 8 bitów |
| `0x02` | PWR_REQ w 3:0, próbka ERRIN w 7:4 |
| `0x03` | fizyczne wejścia górnego banku REG_IN[7:0], po synchronizacji |
| `0x04` | fizyczne pady dolnego banku REG_OUT[7:0], nie latch DATA |
| `0x10..0x13` | cztery 8-bitowe liczniki zboczy narastających ERRIN; odczyt kasuje |
| `0x20` | maska fault, 3:0, R/W |
| `0x21` | ERRIN w 3:0, sticky fault w 7; bit6 niewiarygodny w obecnym topie |
| `0x22` | zapis bit7=1 kasuje sticky fault |
| `0x30` | ADC_CTRL: bit1 ENABLE R/W, bit0 START jako komenda |
| `0x31` | ADC_STATUS: bit0 BUSY, bit1 DONE |
| `0x32..0x41` | osiem wyników ADC, każdy 16-bit little-endian, niewykorzystane bity zero |

W starszych modułach komparator okienkowy i enkoder priorytetowy wystawiają
**cyfrowy ID** heada na GPIO. Moduł temperatury podobnie wystawia ID mezaninki.
Readout interpretuje te bity przez profil konkretnej płyty: maskę, przesunięcie,
polaryzację i tabelę kod → ID. Nie zastępuje tego odczytu nowym ADC i nie zakłada,
że całe 8-bitowe GPIO jest jednym ID. Dla RTD16 górne bity0..2 i4..6 są polami
ID, a bity3/7 to DRDY istniejących ADS124S08, nie część ID.

Na płytach z rezystorami identyfikacyjnymi i nowym torem RC/komparatory readout
korzysta z generic `pwm_adc_8ch`. Profile płyty dostarczają powiązanie kanałów
z headami/mezaninkami oraz dopuszczalne przedziały kodów. HDL nie zna tych
pojęć. Drivery powinny udostępniać także surowe kody diagnostyczne. Przeliczenie
kodu na rezystancję wymaga schematu dzielnika i jego parametrów; nie ma jednej
uniwersalnej tabeli oporów ani napięć dla wszystkich PCB. Kody0/full-scale są
wartościami saturacji, a nie samodzielnym dowodem obecności/braku rozszerzenia.

ADC jest **opcjonalny, jawnie skonfigurowany dla płyty i obrazu HDL**, zwykle
odpytywany kilka razy na sekundę lub rzadziej. REV_ID opisuje hardware, nie
wersję firmware ani capabilities. Nie włączaj ADC automatycznie na podstawie
REV_ID; obecny obraz RTD16 rev1.0 nie ma tego ADC. To osobny tor od ADS124S08
używanych na CS0/CS1.

Sekwencja pojedynczego pomiaru:

1. Zarezerwuj górny bank w kontekście modułu, jeśli działa też komponent GPIO.
2. Zapisz `0x03` do `0x30` (ENABLE + START); ENABLE przejmuje wszystkie osiem
   wejść, a nie pojedynczy wybrany kanał. GPIO DIR/OD/DATA pozostają zapisane.
3. Sprawdź CTRL (`0x02` oznacza ENABLE=1) i odpytywanie DONE w `0x31`
   wykonuj bez blokowania pętli ESPHome. Nie trzymaj magistrali podczas settling.
4. Po DONE odczytaj **jeden burst 16 bajtów od `0x32`**, zdekoduj osiem słów
   little-endian i dopiero wtedy opublikuj cały zestaw wyników.
5. Zapisz `0x00` do `0x30`, zwolnij rezerwację. To przywraca GPIO według jego
   najnowszych zachowanych rejestrów oraz zeruje ADC wyniki/BUSY/DONE/PWM.
   Przy timeout/błędzie także spróbuj wyłączyć ADC i zgłoś nieudany pomiar.

CTRL `0x02` włącza bez START; `0x01` utrzymuje ENABLE=0 i ignoruje START.
START czyta się jako0 i jest ignorowany podczas BUSY. DONE jest sticky do
następnego zaakceptowanego START, wyłączenia lub resetu. Konwersja i publikacja
wyników są autonomiczne. I²C BUS_ACTIVE opóźnia commit, żeby burst nie mieszał
wyników różnych konwersji. Osobne transakcje po jednym bajcie nie tworzą
atomowego wielokanałowego odczytu. Można pozostawić ENABLE=1 i rzadko wystawiać
START, ale przez cały ten czas bank pozostaje wejściem komparatorów.

Timeout dobierz do parametrów obrazu: przy OSCH10.23MHz i SETTLE_CYCLES1024
pełny sweep trwa do około31.9ms dla8 bitów albo204.8ms dla10 bitów, plus
oczekiwanie na zakończenie transakcji I²C i narzut software. Nie zakładaj
100ms limitu dla wszystkich rozdzielczości. [Parametry i ograniczenia ADC](https://github.com/MODULIQ/CPLD_bridge/blob/e8d225aecaec8ec17b3054724a4d4936dca1ec43/docs/pwm-adc.md).

Liczniki odczytuje jeden właściciel, właśnie readout. Zwracają liczbę od
ostatniego odczytu, **modulo256**, nie sumę od uruchomienia ani licznik
nasycający. Ewentualną sumę/deltę udostępnia software. Kasowanie ma pierwszeństwo
nad zdarzeniem w tym samym cyklu CLK; nie jest to bezstratny rejestr zdarzeń.
Odczyt może skasować licznik nawet przed poprawnym odebraniem całego bajtu;
po błędzie transportu nie traktuj retry jako idempotentnego odczytu i nie
publikuj błędnej transakcji jako prawidłowego zera.

## 2. Expander GPIO na CS3

Wymagany zatrzaśnięty CS3_MODE=1. Bank jest wewnętrznym odbiorcą CS3;
zewnętrzny chip na tym CS nie może wtedy korzystać z selekcji. SPI mode0,
MSB first, komenda+dane/dummy, jeden CS niski przez16 bitów. Odpowiedź odczytu
jest w drugim bajcie. Adresy i reset w [gpio-register-map.json](https://github.com/MODULIQ/CPLD_bridge/blob/e8d225aecaec8ec17b3054724a4d4936dca1ec43/hw/gpio-register-map.json).

Publiczne piny: indeksy0..7 = dolny REG_OUT, 8..15 = górny REG_IN.
Driver obsługuje DIR, push-pull/open-drain i odczyt fizycznych padów. Shadow DATA
oraz cache padów to różne wartości; zapis jednego pinu zachowuje resztę bajtu.
Setup najpierw czyta istniejące DATA/DIR/OD/CFG/STATUS, zamiast zapisywać ślepo
wartości resetowe. CS nie resetuje stanu. Przy zmianie funkcji odłącz wyjście,
ustaw DATA/OD, dopiero potem DIR; nie ma atomowego SET/CLEAR pojedynczego bitu.

ADC-capable build zachowuje pełne GPIO po resecie i gdy CTRL.ENABLE=0.
Dopiero ENABLE=1 maskuje wyjścia górnego banku niezależnie od DIR/OD. Readback
tego banku pokazuje wtedy komparatory. Expander nie może udawać, że digital_write
zmieniło fizyczny poziom podczas rezerwacji ADC. Zgłaszaj/odraczaj kolidujące
operacje górnego banku według jawnej polityki; dolny bank pozostaje dostępny.
Zachowane rejestry po disable wracają na pady, także jeśli zapisano je podczas ADC.

Profile ograniczają piny z zewnętrznymi sterownikami: RTD16 rev1.0 wymaga DIR=0
na REG_IN3/7 (maska0x88), a ID podłączonych headów także respektują swoje
sterowniki. Globalny driver nie może wywodzić dozwolonych kierunków z samego
numeru GPIO. DEV_OE=0 odcina GPIO i downstream SPI, ale nie resetuje rejestrów.

W Flash mode niższe OUT3/OUT4 są zarezerwowane jako MOSI/CLK nawet po release;
GPIO nie może ich odzyskać zapisem DATA/DIR. OUT1/OUT2 sterują odpowiednio
CFG[2]/CFG[1], nie latch DATA; wymagają jawnej obsługi aliasów pinmuxu lub
rezerwacji dla komponentu Flash. Pozostałe bity i górny bank działają zgodnie
z DIR/OD, poza czasem rezerwacji ADC.

## 3. Współdzielona Flash na CS2

To transport do pamięci współdzielonej z targetem, nie prywatna Flash ESP32 ani
pamięć konfiguracji CPLD. Wymagany CS2_MODE=1 oraz dostęp do lokalnego CFG na
CS3 (CS3_MODE=1). Nie ma kontroli CFG przez I²C ani przez Flash CS2.

Dane i opcodes pamięci przechodzą przez adresowany kanał2. Driver Flash
korzysta ze wspólnego interfejsu konfiguracji komponentu GPIO/CS3 do przejęcia
linii; nie utrzymuje niezależnej, niekoordynowanej kopii CFG. Model pamięci,
SPI mode, częstotliwość, geometria i procedura bootowania targetu wynikają
z jego schematu/datasheet/profilu; obecny HDL nie narzuca protokołu pamięci.

| Linia | Pad CPLD / pinmux |
|---|---|
| Flash MOSI | REG_OUT3 / pin58 |
| Flash CLK | REG_OUT4 / pin59 |
| Flash CSn | SPI_CSn2 / pin40 |
| Flash MISO | SPI_MISO2 / pin34, zawsze wejście |
| Sterowanie konfiguracji targetu | OUT1=CFG[2], OUT2=CFG[1], nadal z DIR/OD |

Sekwencja własności:

1. Zweryfikuj STATUS na CS3, tryby, profil oraz dostępność linii targetu.
2. Przez wspólny helper CFG ustaw `CFG |= 0x01` (READ_CFG `0x23`, WRITE_CFG
   `0x12`); zachowaj wszystkie pozostałe bity. Przy domyślnym CFG06 daje07.
3. Transfery wykonuj przez CS2, z CS nieaktywnym między transakcjami.
   CFG[0]=1 **nadal steruje liniami między transakcjami**; sam CS high nie
   zwalnia Flash. Sterowanie reset/boot targetu wymaga osobnej jawnej procedury.
4. Po zakończeniu i po dezaktywacji hostowego CS wyczyść jedną operacją
   `CFG &= 0xFE` przez CS3. To razem zwalnia MOSI, CLK i CS2 do Hi-Z.
   Domyślny przykład TX na CS3: `12 06`; nie zapisuj bezwarunkowo06 przy innym CFG.
5. Od tej chwili target może pobrać firmware z Flash. OUT1/OUT2 nie są przez
   release automatycznie przestawiane: zachowują poziomy konfiguracji/resetu.
   Jeśli target wymaga zmiany tych linii, wykonaj ją zgodnie z profilem PCB.

Zapewnij release także w ścieżce błędu/anulowania po zakończeniu aktywnej
transakcji. Nie zakładaj, że reset ESP32 lub DEV_OE=0 zeruje CFG. Niedostępny
CS3 uniemożliwia potwierdzone zwolnienie przez software. Na stanowisku RTD16
CS2_MODE=0: pinmux/release ma testy symulacyjne, nie fizyczny test Flash.
Programowanie/erase i weryfikacja danych są osobnym przyszłym zakresem drivera;
żaden pomiar HIL GPIO/ADS124S08 nie potwierdza ich działania.

## Współpraca, konfiguracja i testy driverów

- Jeden moduł jest identyfikowany przez GA i właściwe transporty. I²C adres
  0x50+GA, HSPI_AD=GA i subadresy chipów2/3 to odrębne informacje. Nie myl
  GPIO I²C0x03 z SPI chip3 ani poleceń SPI0x21 z rejestrem I²C0x21.
- Trzy publiczne komponenty mogą dzielić wewnętrzny kontekst modułu. Rezerwacja
  ADC dotyczy górnego banku, Flash dotyczy wskazanych dolnych pinów/CFG/CS2;
  oba tryby mogą współistnieć, gdy profil PCB na to pozwala. Nie trzymaj
  blokady SPI/I²C przez cały czas konwersji lub oczekiwania pamięci.
- Każdy konsument CFG korzysta z jednego serializowanego read-modify-write
  i wspólnego shadow/cache. GPIO setup/digital_write nie mogą przypadkiem
  przejąć Flash, wyczyścić jej własności ani zmienić resetów targetu.
- Readout nie kasuje liczników przez pomocnicze odczyty innych komponentów.
  GPIO nie włącza ADC; Flash nie zmienia ADC_CTRL ani kierunków górnego banku.
- Zachowaj działające ADS124S08 na kanałach0/1, mode1/100kHz. GPIO używa mode0
  na kanale3, Flash swojego skonfigurowanego trybu na kanale2. Rejestruj osobne
  urządzenia SPI z ich własnym trybem, bez globalnego przestawiania magistrali.

Następny etap sprzętowy pozostaje oddzielny: walidacja transportów z niezależną
obserwacją, ADC na właściwym PCB oraz Flash z dokładnym profilem pamięci i targetu.
[Raport software](../reports/cpld-software-2026-09-30.json) nie nadaje tym
komponentom statusu HIL.
