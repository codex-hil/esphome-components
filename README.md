# MODULIQ ESPHome components

Wspólna, publiczna biblioteka komponentów z `gkasprow/esphome`, `wizath/esphome`
i prac HIL nad ADS124S08. Źródła są w tym repozytorium: nie trzeba mieć kopii
forków ani plików z komputera laboratoryjnego. To biblioteka komponentów,
nie kolejny fork całego ESPHome ani repozytorium toolchainów.

- [Katalog i statusy](catalog/README.md): autorzy, pochodzenie, zależności, testy.
- [Przypięte źródła i sumy plików](catalog/components.json).
- [Gałęzie i warianty](catalog/source-branches.json): zachowujemy informację
  o różnych wersjach; import nie scala automatycznie rozbieżnego kodu.
- [Przykłady](examples): konfiguracje przeznaczone do kompilacji; piny i prądy
  wymagają sprawdzenia z konkretnym schematem przed programowaniem urządzenia.

## Użycie na innym komputerze

Repozytorium można pobrać anonimowo, bez konta GitHub i bez tokenów:

```sh
git clone https://github.com/codex-hil/esphome-components.git
cd esphome-components
git checkout --detach 5faed5546b33d06b61566787a4270ca925bd0164
python3 scripts/verify.py
python3 -m venv .venv
. .venv/bin/activate
python -m pip install 'esphome==2026.9.0'
./scripts/build.sh examples/bridge-adc.yaml
```

Wymagany Python3.12+ zgodny z ESPHome2026.9.0 i standardowe wymagania środowiska
budowania ESPHome. Pierwsza kompilacja pobiera narzędzia ESP-IDF przez ESPHome.
Nie instaluje się pakietów globalnie. Ten przykład przypina ESPHome, ale nie
jest deklaracją bitowej odtwarzalności wszystkich zależności pip/toolchainu.
Ściślej przypięte środowisko Nix i pomiary HIL pozostają w
[codex-hil/esphome-hil](https://github.com/codex-hil/esphome-hil/tree/631bf39).

We własnym projekcie można wskazać checkout biblioteki:

```yaml
external_components:
  - source:
      type: local
      path: ../esphome-components/components
    components: [spi, addrspi, ads124s08_base]
```

Albo pobierać bezpośrednio z GitHuba po pełnym SHA:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/codex-hil/esphome-components.git
      ref: 5faed5546b33d06b61566787a4270ca925bd0164
    components: [spi, addrspi, ads124s08_base]
    refresh: never
```

Repozytorium jest publiczne; pobieranie komponentów nie wymaga poświadczeń.
Wybieraj jawną listę komponentów: `spi`, `mmc5983` i `tca9548a` zastępują moduły
wbudowane. Nie włączaj całej kolekcji przez `components: all`.

## Rozwój i aktualizacje

`components/` zawiera22 wybrane komponenty; `archive/` trzy historyczne lub
eksperymentalne dodatki, poza domyślną ścieżką ładowania. Oba drivery ADS124S08
zachowują różne nazwy: `ads124s08` z forka Wizatha oraz `ads124s08_base` z HIL.
Nie są zamienne konfiguracją i nie dzielą tego samego zakresu walidacji.

Zmiany wprowadzamy przez gałąź i PR. Aktualizacja źródła wymaga jawnego wyboru
rewizji, porównania zmian oraz uaktualnienia manifestu i wyników testów. Nie ma
automatycznego śledzenia ruchomych gałęzi podczas budowy urządzeń. Przy zmianach
lokalnych zachowujemy źródłową rewizję i opisujemy je w `local_changes`.

```sh
python3 scripts/verify.py
python3 scripts/import-check.py
./scripts/build.sh examples/bridge-adc.yaml
```

`verify.py` sprawdza kompletność, SHA-256 i składnię Pythona. `import-check.py`
sprawdza import schematów; to jeszcze nie kompilacja ani test sprzętu.
[Wyniki](reports) wyraźnie oddzielają te etapy. CI uruchamia weryfikację i importy
na ESPHome2026.9.0. Testy sprzętowe pozostają w repozytorium HIL.

[Licencje i pochodzenie](LICENSE.md). Nie zmieniamy przypisania autorów.

## Sprawdzone pobieranie i budowanie

Dla rewizji `5faed5546b33d06b61566787a4270ca925bd0164`:

- 18/18 komponentów przechodzi import schematów w ESPHome2026.9.0.
- 6/6 przykładów kompiluje się dla klasycznego ESP32 z ESP-IDF5.5.5.
- Przed upublicznieniem: pobranie prywatnego repo przez `external_components`, po SHA i z pustym cache,
  zakończyło się poprawną walidacją konfiguracji.
- Świeży klon z GitHuba, pusta przestrzeń build/cache i `scripts/build.sh`
  dały firmware w przypiętym środowisku Nix w 50.42s.
- Weryfikacja źródeł i importów przeszła także na
  [runnerze GitHuba](https://github.com/codex-hil/esphome-components/actions/runs/36635934996), poza komputerem laboratoryjnym.

[Raporty](reports) rozdzielają import, kompilację i wcześniejszą walidację HIL.
W tym zadaniu nie programowano żadnego urządzenia.

## Znane problemy i porównania

- [MMC5983: zgłoszone błędne odczyty co drugi pomiar](docs/known-issues.md).
- [ADS124S08: porównanie implementacji Wizatha i HIL](docs/ads124s08-comparison.md).

Repozytorium przeniesiono z MODULIQ do codex-hil i upubliczniono na polecenie
właściciela. Starsze raporty zachowują ówczesny adres i wynik testu prywatnego
dostępu; nie opisują bieżącego wymogu uwierzytelniania.

Anonimowe pobieranie publicznego źródła przez ESPHome potwierdzono z pustym HOME
i wyłączoną konfiguracją uwierzytelniania Git: [raport](reports/public-source-2026-09-29.json).

## RTD: PT100 i PT1000

Nowy niezależny [komponent `rtd`](docs/rtd.md) przelicza rezystancję w omach
na temperaturę. [Przykład](examples/rtd.yaml) korzysta z syntetycznych danych;
testy symulatorem RTD i mezaninką pozostają do wykonania.

```sh
./scripts/test-rtd.sh # wymaga kompilatora C++17 i Pythona z ESPHome
./scripts/build.sh examples/rtd.yaml
```

## Trzy niezależne komponenty CPLD Bridge

`moduliq_cpld_i2c` obsługuje identyfikację/statusy/liczniki i opcjonalny ADC
identyfikacyjny CPLD. `moduliq_cpld_gpio` obsługuje CS3, a `moduliq_cpld_flash`
transport CS2 i acquire/release przez jawnie wskazany helper CFG na CS3.
Nie konfigurują ani nie odpytują ADC temperatury Texas Instruments.
Nie przypisują pinom roli DRDY ani nie narzucają konfiguracji innych urządzeń SPI.

I²C jest dostępne na wszystkich płytkach. GPIO, Flash i ADC identyfikacyjny
są domyślnie wyłączone w software. YAML wybiera ich użycie i moment aktywacji
po odczycie PROJECT_ID; nie ma automatycznych profili płyt ani powiązania po GA.
[Instrukcja i przykłady](docs/cpld-components-usage.md),
[raport software](reports/cpld-independent-software-2026-09-30.json).

```sh
./scripts/test-cpld.sh
../esphome-hil/scripts/build.sh examples/cpld-gpio.yaml
```

Weryfikacja obejmuje software i kompilację. W tej pracy nie programowano,
nie resetowano i nie odczytywano sprzętu.

## Opcjonalny programator Flash przez TCP

[`moduliq_serprog`](docs/moduliq-serprog.md) przekazuje surowe transakcje SPI
klienta flashrom przez istniejący driver Flash. YAML wybiera aktywację po
PROJECT_ID, potwierdza gotowość targetu i zakończenie operacji pamięci przed jej
oddaniem. [Przykład](examples/cpld-serprog.yaml) jest domyślnie wyłączony.
Testy obejmują rzeczywisty klient flashrom i emulowaną NOR; sprzętu nie programowano.

## I²C na GPIO CPLD

[`moduliq_cpld_soft_i2c`](docs/moduliq-cpld-soft-i2c.md) wystawia zwykłe magistrale
I²C ESPHome przez open-drain CPLD. Cztery niezależne pary mieszczą się na jednym
banku. [Przykład](examples/cpld-soft-i2c.yaml) ma cztery standardowe PCA9554 pod
jednakowym adresem na różnych busach. Testy software i kompilacja są oddzielone
od jeszcze niewykonanej walidacji sprzętowej.
