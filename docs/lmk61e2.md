# Komponent ESPHome LMK61E2

Komponent ustawia częstotliwość LMK61E2 po I²C i udostępnia format oraz włączenie wyjścia w ESPHome i Home Assistant. Rdzeń C++ działa niezależnie od ESPHome, MSP430 i transportu. Przetestowano go na LMK61E2EVM przez USB2ANY z pomiarem MSO4104 CH4. Firmware ESP32-P4 skompilowano na ESPHome 2026.9.0 i wgrano przez OTA. [Test bezpośredniego I²C na P4](../reports/lmk61e2-p4-hil-2026-10-01.json) potwierdził dziesięć nastaw oraz sterowanie wyjściem z pomiarem MSO4104 CH4.

## Parametry i sterowanie

[Kompletny przykład YAML](../examples/lmk61e2.yaml) korzysta z lokalnych external_components. Piny ESP32 i Wi-Fi w przykładzie są przykładowe. Adres 0x59 odpowiada naszej EVM; standardowy domyślny adres komponentu to 0x58. Możliwe adresy: 0x58, 0x59, 0x5A.

```yaml
lmk61e2:
  id: clock_generator
  address: 0x59
  frequency: 100MHz
  output_format: LVPECL
  output_enabled: true
  allow_fractional: true
  frequency_number:
    name: Clock frequency
  format_select:
    name: Clock output format
  output_switch:
    name: Clock output enabled
  actual_frequency:
    name: Clock nominal frequency
  pll_ok:
    name: Clock PLL status OK
```

| Parametr | Znaczenie |
| --- | --- |
| frequency | Częstotliwość początkowa, domyślnie 100 MHz |
| output_format | LVPECL, LVDS lub HCSL; domyślnie LVPECL |
| output_enabled | Stan programowego wyłączenia bufora; domyślnie true |
| allow_fractional | Zezwolenie na eksperymentalny profil fractional; domyślnie false |
| frequency_number | Opcjonalna nastawa MHz w Home Assistant, krok 0,0001 MHz |
| format_select | Opcjonalny wybór formatu w Home Assistant |
| output_switch | Opcjonalne programowe włączenie/wyłączenie wyjścia |
| actual_frequency | Nominalna osiągalna częstotliwość z planera, nie pomiar wyjścia |
| pll_ok | Brak odczytanego loss of lock i aktywnej kalibracji |

Zakresy według [datasheet LMK61E2](https://www.ti.com/lit/ds/symlink/lmk61e2.pdf): LVPECL 10–1000 MHz, LVDS 10–900 MHz, HCSL 10–400 MHz. Zmiana formatu przy częstotliwości poza jego zakresem zostaje odrzucona. Format musi odpowiadać terminacji i odbiornikowi; dotychczasowe pomiary EVM wykonano w LVPECL. Wyboru LVDS/HCSL nie testowano na tym torze pomiarowym. Układ nie udostępnia programowej nastawy amplitudy lub mocy dBm. Stan programowego wyjścia nie zastępuje fizycznego pinu OE.

Częstotliwość można zmieniać także z automatyzacji:

```yaml
- lmk61e2.set_frequency:
    id: clock_generator
    frequency: 122.88MHz
```

Lub przez `id(clock_generator).set_frequency(122880000.0)` w lambdzie. Metoda zwraca powodzenie transakcji rejestrów; sprawdzenie kalibracji następuje asynchronicznie i może zgłosić ostrzeżenie komponentu. `frequency_number` i sensory ESPHome korzystają z float, więc ich precyzja jest ograniczona. W YAML i akcjach częstotliwość planera jest double; nie obiecujemy precyzji 1 Hz przez encję number. Nastawy encji nie są utrwalane w LMK EEPROM ani przez komponent w pamięci ESP; po restarcie stosuje się YAML.

## Algorytm planowania

[planner.h](../components/lmk61e2/planner.h) przeszukuje OUTDIV 5–511 z VCO 4,6–5,6 GHz i doublerem, czyli PFD nominalnie 100 MHz. Dla każdego kandydata wyznacza INT i ułamek NUM/DEN metodą ułamków łańcuchowych, ograniczając mianownik do 22 bitów. Kolejność wyboru: najmniejszy błąd częstotliwości, tryb integer, VCO najbliższe 5 GHz, mały mianownik. Zwraca osiągalną nominalną częstotliwość i błąd ppm. Jest to nasz algorytm; nie skopiowano optymalizatora TI.

Generator tworzy maski rejestrów, zachowując bity zastrzeżone. Profil integer odpowiada konfiguracji odczytanej z EVM i przykładom TI; profil fractional używa MASH3, weak dither, CP 1,6 mA i włączenia C3. Profil filtra R36=4, R37=0, R38=0, R39=1 przeszedł funkcjonalny test 122,88 MHz. To profil eksperymentalny, nie optymalizacja jitteru lub spurów. Inne częstotliwości fractional nie są kwalifikowane sprzętowo. `allow_fractional` stanowi świadomy wybór tego profilu.

Przed zapisem rdzeń czyta poprzednią konfigurację i identyfikuje układ. Na czas zmiany PLL wycisza bufor, zapisuje ulotne pola, sprawdza odczyt i wyzwala kalibrację R72.1, następnie stosuje stan wyjścia. Nie zapewnia zmiany bez przerwy ani glitchy. Błąd zapisu lub odczytu wywołuje próbę przywrócenia poprzednich wartości. Niepowodzenie przywrócenia zgłasza błąd; nie potwierdza się wtedy nastawy jako pomyślnie zastosowanej. Kalibrację/status sprawdza ESPHome bez blokowania przez timeouty. Status LOL/CAL jest odczytywany okresowo; odczyt może kasować flagi, więc nie jest to ciągły monitor ani pomiar jitteru.

## Walidacja i granice testów

- ESPHome 2026.9.0: walidacja YAML i pełna kompilacja ESP32 ESP-IDF 5.5.5, z number, select, switch, sensor, binary_sensor oraz akcją ustawienia częstotliwości.
- Rdzeń C++: 812 planów, wszystkie 10 wymaganych częstotliwości, granice formatów, 56 symulowanych pojedynczych błędów I/O z przywróceniem konfiguracji; AddressSanitizer i UndefinedBehaviorSanitizer.
- [Wyniki wspólnego rdzenia na EVM](../reports/lmk61e2-hil-2026-10-01.json): 10 częstotliwości 10–200 MHz, w tym 122,88 MHz, poprawne statusy PLL i sprawdzenie sterowania DIFFCTL. Przy wyłączeniu DIFFCTL=0x81, przy włączeniu 0x01. Pomiar szumu przy wyłączeniu około 0,1 Vpp nie jest specyfikacją tłumienia.
- Po testach przywrócono 100 MHz. NVM counter R48 pozostał 6; EEPROM i firmware MSP430 nietknięte.

Nie porównano jeszcze wyników z uruchomionym TICS Pro i nie wykonano kwalifikacji bardzo niskiego jitteru. Wgrano firmware ESP32-P4; firmware MSP430 pozostawiono bez zmian. Docelowy ESP32 łączy się bezpośrednio z LMK przez I²C 3,3 V ze wspólną masą; należy zapewnić dostęp do szyny bez równoległego sterowania przez MSP430. Komponent nie korzysta z USB EVM.

## Odtworzenie sprawdzeń

```bash
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/lmk61e2_planner.cpp -o /tmp/lmk61e2-planner-test
/tmp/lmk61e2-planner-test
esphome config examples/lmk61e2.yaml
esphome compile examples/lmk61e2.yaml
```


## Waveshare ESP32-P4-ETH

Połączenie: GPIO7/SDA → EVM J4 pin 1, GPIO8/SCL → J4 pin 2, GND → J4 pin 5. Obie płytki zasilane z własnego USB, bez łączenia szyn zasilania. I²C 100 kHz, LMK address 0x59. MSP430 pozostaje bezczynny; nie używać równolegle USB2ANY do I²C.

Testy bezpośredniego I²C: 10, 20, 25, 50, 80, 100, 122.88, 125, 156.25 i 200 MHz. Wszystkie miały PLL OK i pomiar w granicach funkcjonalnego kryterium ±1000 ppm. Wyniki z krótkiej akwizycji nie stanowią kalibrowanego pomiaru ppm ani kwalifikacji jitteru. Po teście przywrócono 100 MHz / LVPECL / wyjście włączone. Pomiar odbywał się na zatrzymanej akwizycji, aby metadane i próbki odnosiły się do tego samego rekordu.
