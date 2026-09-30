# Trzy komponenty MODULIQ CPLD Bridge

I²C, GPIO na CS3 i Flash na CS2 są osobnymi driverami. Każda płytka ma blok
I²C; dostępność GPIO oraz Flash wynika z jej konfiguracji CPLD. Aplikacja YAML
wybiera ich użycie i moment aktywacji po odczycie PROJECT_ID.

Drivery nie konfigurują ADC temperatury Texas Instruments, nie sprawdzają
jego adresu, trybu ani readiness i nie przypisują bitom GPIO roli DRDY.
Inne urządzenia pod dekoderem CPLD mają własne drivery i konfigurację YAML.

| Komponent | Transport | Zakres |
|---|---|---|
| `moduliq_cpld_i2c` | I²C0x50+GA | PROJECT_ID/REV_ID, GPIO, cyfrowe ID według YAML, clear-on-read liczniki i opcjonalny ADC identyfikacyjny CPLD |
| `moduliq_cpld_gpio` | CS3 mode0 | dwa banki, DIR, DATA, open-drain i wspólny helper CFG |
| `moduliq_cpld_flash` | CS2, CFG przez CS3 | jawne acquire, transport i release |

GPIO i Flash oraz ADC identyfikacyjny są domyślnie wyłączone w software.
Włącza je jawna konfiguracja lub lambda YAML po identyfikacji płyty.
Nie zmienia to strapów CS2_MODE/CS3_MODE CPLD.

Readout nie wymaga GPIO ani SPI. Opcjonalny `gpio_id` jest wyłącznie jawnym
powiązaniem dla koordynacji banku ADC identyfikacyjnego. Nie wynika z GA.
Flash wymaga jawnego `gpio_id`, ponieważ jego CFG jest na CS3, i korzysta z
jednego właściciela rejestru. Ochrona wejść pochodzi z masek w YAML, bez profili
PCB zaszytych w driverze. GPIO działa po resecie CPLD; ADC_CTRL.ENABLE zajmuje
górny bank tylko po świadomym uruchomieniu. Wyniki ADC są czytane jednym
burstem16 bajtów przed disable. Cleanup zasobów trwa do potwierdzenia release.

[Instrukcja i przykłady](cpld-components-usage.md),
[mapa transportu](cpld-register-contract.json),
[raport software](../reports/cpld-independent-software-2026-09-30.json).
Kontrakt rejestrów audytowano przy `MODULIQ/CPLD_bridge` commit
`2b0752faf76c84b76e593727a427b732e0b44b99`.
Opis historycznego stanowiska RTD16 w upstream handoffie nie określa polityki
tych driverów; obowiązuje bieżący podział właściciela i wybór w YAML.

Nie programowano sprzętu. Fizyczna walidacja driverów oraz analogowego ADC
identyfikacyjnego i Flash pozostaje osobnym etapem.
