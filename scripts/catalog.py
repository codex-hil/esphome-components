#!/usr/bin/env python3
"""Render the human-readable catalog from pinned sources and recorded results."""
import json
from pathlib import Path
R = Path(__file__).resolve().parents[1]
entries = json.loads((R/'catalog/components.json').read_text())['components']
imports = json.loads((R/'reports/imports-2026.9.0.json').read_text())['components']
buildfile = R/'reports/builds-2026.9.0.json'
builds = json.loads(buildfile.read_text())['examples'] if buildfile.exists() else {}
coverage = {'cpld-serprog':['moduliq_serprog'], 'cpld-readout':['moduliq_cpld_i2c'], 'cpld-gpio':['moduliq_cpld_i2c','moduliq_cpld_gpio'], 'cpld-adc-flash':['moduliq_cpld_i2c','moduliq_cpld_gpio','moduliq_cpld_flash'], 'rtd':['rtd'], 'bridge-adc': ['spi','addrspi','ads124s08_base'], 'addrspi2':['addrspi2'],
            'ads8166':['ads8166'], 'dacx0504':['dacx0504'], 'max112xx':['max112xx'], 'mcp3208':['mcp3208']}
desc = {
 'moduliq_serprog':'serprog TCP: surowe SPI dla flashrom, polityka targetu w YAML',
 'moduliq_cpld_i2c':'CPLD I²C: identyfikacja/statusy/liczniki i opcjonalny ADC',
 'moduliq_cpld_gpio':'CPLD CS3: dwa banki GPIO, DIR/OD i wspólny CFG',
 'moduliq_cpld_flash':'CPLD CS2: współdzielona Flash, transport i acquire/release przez CS3',
 'rtd':'Rezystancja → temperatura PT100/PT1000; IEC 60751',
 'ad9959':'DDS, cztery kanały', 'addrspi':'SPI z wyborem adresu na GPIO',
 'addrspi2':'Adresowane kanały SPI z nagłówkiem protokołu',
 'ads124s08':'ADC + kanały temperatury z forka Wizatha',
 'ads124s08_base':'ADC, kanały napięciowe MUX/IDAC i diagnostyka CRC z HIL',
 'ads8166':'Wielokanałowy ADC SPI', 'as6500':'Konwerter czasu TDC',
 'dac8775':'DAC', 'dacx0504':'Rodzina DACx0504', 'hc138':'Dekoder wyboru kanału SPI',
 'max112xx':'Rodzina ADC MAX112xx', 'max6966':'Sterownik wyjść LED/PWM',
 'mcp3208':'Ośmiokanałowy ADC', 'mmc5983':'Magnetometr; [zgłoszony problem odczytów](../docs/known-issues.md)',
 'mmc5983_spi':'Magnetometr SPI; [zgłoszony problem odczytów](../docs/known-issues.md)', 'spi':'Zmodyfikowana warstwa SPI dla magistral adresowanych',
 'tca9548a':'Multiplekser I²C; wariant zgodności z external_components',
 'tmc5130':'Sterownik silnika krokowego', 'resistance_sampler':'Historyczny interfejs próbkowania rezystancji',
 'spi_test':'Eksperymentalny generator transakcji SPI', 'ethernet_lan865x':'Historyczny wariant Ethernet LAN865x',
}
lines=['# Katalog komponentów', '',
       'Stan importu: 2026-09-30. Baseline testów: ESPHome2026.9.0, ESP32/ESP-IDF5.5.5.',
       'PASS importu oznacza załadowanie modułów Pythona. PASS kompilacji obejmuje tylko',
       'konkretny przykład, nie wszystkie ustawienia ani działanie sprzętu.', '',
       '| Komponent | Funkcja | Schematy | Kompilacja przykładu | Źródło |',
       '|---|---|---|---|---|']
for e in entries:
 s=e['source'];name=e['name']
 imp=imports.get(name,{}).get('result','nie testowano')
 checks=[f"[{example}]({ '../examples/'+example+'.yaml' }): {builds[example]['result']}" for example,names in coverage.items() if name in names and example in builds]
 compile='; '.join(checks) or 'nie testowano'
 source=(f"[Rozwój lokalny](../{e['path']})" if s.get("kind")=="local" else
         f"[{s['repository']}@{s['revision'][:8]}](https://github.com/{s['repository']}/tree/{s['revision']}/{s['path']})")
 lines.append(f"| [{name}](../{e['path']}) | {desc[name]} | {imp} | {compile} | {source} |")
lines += ['', '## Wybór wariantów i zależności', '',
 'Bazą większości importów jest `wizath/esphome:esphome26.3` przy pełnym SHA',
 '`16cb61e697b712d1ac20be9fb2b0b0fdcc37930c` (fork deklaruje ESPHome2026.3.1).',
 '`gkasprow/esphome:dev` zawiera scalone dodatki Wizatha na starszej bazie2025.3.0-dev;',
 'porównanie z `wizath:dev` wykazało16 dodatkowych commitów, m.in. sterowniki DDS,',
 'DAC, ADC, TDC, SPI MUX i magnetometru. Gałęzie i alternatywne drzewa zachowuje',
 '[inwentarz](source-branches.json). Nie kopiujemy wycofanych składników upstream',
 '(np.RP2040/event_emitter) tylko dlatego, że nie występują już w ESPHome2026.9.0.', '',
 '- `addrspi`, `addrspi2`, `hc138`: wybieraj również zebrane `spi`.',
 '- `spi`, `mmc5983`, `tca9548a` zastępują wbudowane komponenty; włączaj je jawnie.',
 '- `ads124s08` i `ads124s08_base` to dwa odrębne API. Dla obecnego bridge używamy',
 '  `ads124s08_base`; import drugiej implementacji nie nadaje jej statusu HIL.',
 '- `max6966` pochodzi ze starszej gałęzi `developement`; przechodzi import schematu,',
 '  ale nie został w tej kolekcji zbudowany ani sprawdzony na sprzęcie.',
 '- Archiwalne LAN865x, spi_test i resistance_sampler są poza `components/`.',
 '  Wymagają oddzielnej oceny zgodności/dependencji; nie są częścią baseline.', '',
 '## Znane problemy', '', '[MMC5983: co drugi odczyt błędny — zgłoszenie użytkownika, bez diagnozy](../docs/known-issues.md).', '[Porównanie ADS124S08 i znalezione problemy](../docs/ads124s08-comparison.md).', '', '## Sprzęt i autorstwo', '',
 'W tej operacji nie programowano sprzętu. Wcześniejsze testy `spi`/`addrspi`/',
 '`ads124s08_base` na module0 i dwóch ADC są opisane w',
 '[raporcie HIL](https://github.com/codex-hil/esphome-hil/blob/631bf39/reports/ads124s08-mux-2026-09-29.json).',
 'Dotyczyły MUX/CRC i diagnostyki, z IDAC wyłączonym; nie potwierdzają pomiaru',
 'temperatury lub fizycznej dokładności prądu. Pozostałe komponenty nie mają tutaj',
 'nadanej walidacji sprzętowej.', '',
 'Oryginalne `CODEOWNERS` pozostają w źródłach i [manifeście](components.json).',
 'Główne pochodzenie dodatków: Wizath, mirror i integracja: gkasprow; oryginalni',
 'autorzy ESPHome zachowują swoje oznaczenia. Pełne licencje: [LICENSE.md](../LICENSE.md).', '',
 'Ten plik generuje `python3 scripts/catalog.py`. Zmiany wyników należy opierać',
 'na rzeczywistych raportach, nie na samym istnieniu kodu.', '']
(R/'catalog/README.md').write_text('\n'.join(lines))
