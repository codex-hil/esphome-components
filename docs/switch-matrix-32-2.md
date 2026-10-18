# Switch Matrix 32:2

Komponent `switch_matrix_32_2` obsługuje kartę ze schematu
`Switch_Matrix_32_2 (1).PDF`, rewizja 2 (arkusze 2 i 3, ostatnia modyfikacja
21.08.2023). Dwa łańcuchy po cztery SN74LV595 sterują przekaźnikami A/B.
SPI MODE0, MSB first, domyślnie 200 kHz. Potrzeba SCK, MOSI, dwóch GPIO
zatrzasków CSnA/CSnB oraz jednego GPIO /OE; MISO nie jest wymagane.

[Pełny przykład z 32 kanałami](../examples/switch-matrix-32-2.yaml).
Numery GPIO są przykładowe. Trzeba wybrać właściwą drogę sygnałów na karcie
(Arduino/EEM); driver nie konfiguruje pokładowego multipleksera.

```yaml
external_components:
  - source:
      type: local
      path: ../components
    components: [switch_matrix_32_2]

spi:
  id: matrix_spi
  clk_pin: GPIO18
  mosi_pin: GPIO23

switch_matrix_32_2:
  id: matrix
  spi_id: matrix_spi
  cs_a_pin: GPIO16
  cs_b_pin: GPIO17
  oe_pin: GPIO21
  startup_state: BYPASS
  break_before_make: 10ms

select:
  - platform: switch_matrix_32_2
    matrix_id: matrix
    channel: 1
    id: matrix_ch1
    name: "Kanał 01"
```

Każda encja udostępnia opcje `A`, `B`, `BYPASS`. Kanały są numerowane 1–32.
Jedną encję można zadeklarować dla każdego kanału; pozostałe kanały nadal można
ustawiać maskami. W `BYPASS` OUT jest połączone z własnym IN, a nie rozłączone.
Wiele kanałów może być jednocześnie połączonych z tą samą wspólną parą A lub B.
A/B nie mają w kodzie przypisanego znaczenia AC/DC — określa je podłączenie.

Sterowanie jednym kanałem w dowolnej automatyzacji:

```yaml
then:
  - select.set:
      id: matrix_ch1
      option: "A"
```

Zbiorcze sterowanie:

```yaml
then:
  - switch_matrix_32_2.set_masks:
      id: matrix
      mask_a: "0x00000005"
      mask_b: "0x8000000A"
```

Maski są tekstem szesnastkowym: 1–8 cyfr, z opcjonalnym prefiksem `0x`/`0X`.
Bit 0 odpowiada kanałowi 1, bit 31 kanałowi 32. Powyżej CH1/3 → A,
CH2/4/32 → B, pozostałe → BYPASS. Nieprawidłowy format lub nakładające się
bity A/B odrzucają całe polecenie, zapisując ostrzeżenie w logu. Stan i
oczekujące poprawne polecenie pozostają zachowane.

```yaml
then:
  - switch_matrix_32_2.bypass_all:
      id: matrix
```

Natywne API (w urządzeniu z już skonfigurowanym Wi-Fi lub Ethernetem):

```yaml
api:
  actions:
    - action: set_matrix_masks
      variables:
        mask_a: string
        mask_b: string
      then:
        - switch_matrix_32_2.set_masks:
            id: matrix
            mask_a: !lambda 'return mask_a;'
            mask_b: !lambda 'return mask_b;'
```

## Przełączanie i stan

Przy starcie /OE jest wyłączane stanem wysokim, oba banki otrzymują zera,
następnie /OE jest włączane stanem niskim. Nie odtwarzamy stanu sprzed restartu.
Piny należy podawać bez `inverted: true`. Zachowanie przed uruchomieniem
firmware zależy od sprzętowych rezystorów i stanów GPIO; sekwencja inicjalizacji
nie gwarantuje stanu podczas resetu lub utraty zasilania.

Podczas zmiany najpierw zwalniane są usuwane trasy; niezmieniane połączenia
pozostają aktywne. Po nieblokującej przerwie `break_before_make` (minimum 1 ms,
domyślnie 10 ms) załączane są docelowe trasy. Czas trzeba dobrać do faktycznego
czasu zwalniania i drgań styków przekaźników. Kolejne poprawne polecenie zastępuje
oczekujący cel i rozpoczyna przerwę ponownie. Polecenia pojedynczego kanału
zachowują docelowe ustawienia pozostałych kanałów, także oczekujących.

Akcje wracają przed zakończeniem przełączania; dalsze kroki automatyzacji
wymagające ustalonego połączenia powinny uwzględnić tę przerwę oraz czas
załączania/stabilizacji przekaźnika. Encje publikują stan wpisany do zatrzasków,
w tym przejściowy BYPASS. Nie potwierdzają fizycznego położenia styków.
Przy kontrolowanym wyłączeniu firmware /OE jest wyłączane.

CSnA/B są wejściami RCLK, nie bramkują zegara rejestrów. Każdy transfer przesuwa
oba łańcuchy. Driver przesyła pełne 32 bity przed każdym osobnym narastającym
zboczem RCLK. Najpierw wysyła bity 31–24, na końcu 7–0: CTRL1 to QA pierwszego
rejestru, CTRL32 to QH ostatniego. Oba banki nie zatrzaskują się jednocześnie;
jedno polecenie masek jest wspólnie walidowane i wykonywane w sekwencji.

## Weryfikacja

- `bash tests/run-switch-matrix.sh`: model wspólnego przesuwania obu łańcuchów,
  mapowanie bitów, selektory, wymiana oczekujących poleceń, odrzucanie kolizji,
  parser masek i wyłączenie; ASan/UBSan.
- `<python ESPHome> tests/test_switch_matrix_schema.py`: komplet 32 kanałów,
  zakresy, duplikaty, kolizje GPIO, polaryzacja, MODE0, opóźnienie i API.
- `esphome compile examples/switch-matrix-32-2.yaml`: rzeczywisty ESP32/ESP-IDF.

Wariant z natywnym API i lambdami masek również skompilowano. Powtórzenie:
`python3 tests/prepare-switch-matrix-build.py`, następnie
`esphome compile artifacts/switch-matrix-build/api-compile-only.yaml`.
[Raport testów](../reports/switch-matrix-software-2026-10-17.json).

Stan fizycznych przekaźników i czasy przełączania pozostają nieweryfikowane.
