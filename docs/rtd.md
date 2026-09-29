# RTD: rezystancja → temperatura

Komponent `rtd` przyjmuje stan dowolnego sensora ESPHome **w omach** i publikuje
°C. Obsługuje platynowe PT100/PT1000 o charakterystyce IEC 60751, α≈0.00385.
Nie jest sterownikiem ADC: MUX, IDAC, pomiar napięcia i wyznaczanie rezystancji
pozostają w warstwie pomiarowej. NTC korzysta z komponentu `ntc` ESPHome.

```yaml
external_components:
  - source: github://codex-hil/esphome-components@feature/rtd
    components: [rtd]

sensor:
  # Osobno zdefiniuj sensor rezystancji z id: probe_resistance, publikujący Ω.
  - platform: rtd
    sensor: probe_resistance
    nominal_resistance: 100 Ohm  # 1000 Ohm dla PT1000
    name: "Temperatura RTD"
```

Gałąź służy rozwojowi. Dla powtarzalnego firmware przypnij pełny SHA commitu
zawierającego komponent. Kompletny [przykład do kompilacji](../examples/rtd.yaml)
ma syntetyczne źródła dla obu typów; nie wymaga płytki ani pinów.

`sensor` i `nominal_resistance` są obowiązkowe. Akceptowane R₀ to wyłącznie
100 lub 1000 Ω. Komponent przelicza każdą opublikowaną zmianę źródła, także jego
istniejący stan podczas startu; nie ma własnego `update_interval`.
Standardowe filtry sensora ESPHome można zastosować na wejściu lub wyjściu.

Model obejmuje −200…850°C. Ujemna temperatura wykorzystuje pełny człon CVD
z C, dodatnia stabilną postać rozwiązania równania kwadratowego. Odwrotność
poniżej zera wyznacza ograniczona do 32 kroków bisekcja.
Nieskończoność, NaN i rezystancja poza zakresem powodują publikację NaN oraz
ostrzeżenie komponentu. Poprawny kolejny odczyt automatycznie usuwa ostrzeżenie.
Nie pozostaje ostatnia poprawna temperatura jako pozornie aktualny pomiar.

Granice rezystancji są zaokrąglone do reprezentacji float ESPHome. Dla PT100
wynoszą około 18.52008 i 390.481125 Ω (PT1000: dziesięć razy więcej).
Zaokrąglona tabela, np. 18.52 Ω przy −200°C, może wypaść poza ścisły zakres;
nie dodajemy arbitralnej tolerancji ani ekstrapolacji. Zakres konkretnej sondy
może być węższy od zakresu modelu.

Krzywa matematyczna jest wspólna dla sond 2- i 4-przewodowych. Usuwanie wpływu
przewodów, pomiar ratiometryczny, kalibracja toru i kontrola samonagrzewania
muszą wynikać z połączeń mezaninki i konfiguracji pomiaru. Ten komponent nie
zgaduje tych parametrów. Nie obsługuje innych współczynników platyny ani
krzywych czujników kriogenicznych.

## Weryfikacja

`./scripts/test-rtd.sh`: 21002 punkty w całym zakresie obu R₀, monotoniczność,
wartości tabelaryczne, granice, NaN/Inf, zmiany stanu i powrót po błędzie oraz
walidacja rzeczywistego schematu ESPHome. Test adaptera kompiluje rzeczywisty
`rtd.cpp` z minimalnymi atrapami sensora; pełny przykład sprawdza integrację
z właściwym ESPHome. Testy są również podłączone do CI.

Błąd numeryczny w teście float poniżej 0.001°C nie jest deklaracją dokładności
fizycznego pomiaru. Testy sprzętowe czekają na symulator RTD i mezaninkę.

Źródła modelu: [TI, A Basic Guide to RTD Measurements](https://www.ti.com/lit/pdf/sbaa275).
Niezależne punkty tabelaryczne −100, −50, 0, 50, 100°C:
[Analog Devices, Positive Analog Feedback Compensates PT100 Transducer](https://www.analog.com/en/resources/technical-articles/positive-analog-feedback-compensates-pt100-transducer.html).
