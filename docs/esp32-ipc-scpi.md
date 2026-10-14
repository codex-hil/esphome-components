# ESP32_32E_IPC_V1.1: ESPHome i SCPI

Konfiguracja [esp32-ipc-scpi.yaml](../examples/esp32-ipc-scpi.yaml) obsługuje
płytkę RS485 z dwoma przekaźnikami i wejściami optoizolowanymi. Korzysta z
istniejącego komponentu SCPI z wątku „SCPI dla ESPHome”, skopiowanego bez zmian.
Pochodzenie i SHA-256 plików są w [manifeście](../catalog/components.json).
Nie wymaga plików z komputera laboratoryjnego. Nie zawiera sterowania Trenzem.

## Uruchomienie

W checkout tej gałęzi:

```sh
esphome config examples/esp32-ipc-scpi.yaml
esphome compile examples/esp32-ipc-scpi.yaml
esphome upload examples/esp32-ipc-scpi.yaml --device /dev/serial/by-id/usb-1a86_USB_Serial-if00-port0
```

Zweryfikowane wersje: ESPHome 2026.9.0, ESP-IDF 5.5.5, ESP32-D0WD-V3,
flash 4 MB. Przed programowaniem zachowano pełny backup fabrycznego flash.
Adres pobierany przez DHCP; w testach `192.168.2.11`, port TCP **5025**.
Serwer nie ma uwierzytelniania ani TLS. SCPI działa przez Ethernet/TCP;
RS485 jest skonfigurowane, ale komponent nie implementuje SCPI przez UART.

## Komendy SCPI

Wysyłaj ASCII, jedną komendę na linię, zakończenie LF (`\n`); CRLF też działa.
Wielkość liter nie ma znaczenia. Wielkie litery w pełnej nazwie pokazują jej
krótki alias. Komendy wykonawcze nie zwracają potwierdzenia; odpowiedź otrzymuje
się dla zapytań `?`. Nie czekaj na odpowiedź po `PULS:FIRS`.

| Pełna komenda | Skrót | Wynik / działanie |
|---|---|---|
| `*IDN?` | `*IDN?` | `Lab,ESP32_32E_IPC_V1.1,209BA98C8FAC,0.1.0` |
| `OUTPut:FIRSt?` | `OUTP:FIRS?` | Stan sterowania przekaźnika 1: liczba 0 lub 1 |
| `OUTPut:SECond?` | `OUTP:SEC?` | Stan sterowania przekaźnika 2: liczba 0 lub 1 |
| `INPut:FIRSt?` | `INP:FIRS?` | Wejście 1: 1 oznacza aktywne napięcie wejściowe |
| `INPut:SECond?` | `INP:SEC?` | Wejście 2: 1 oznacza aktywne napięcie wejściowe |
| `PULSe:FIRSt` | `PULS:FIRS` | Lokalny impuls przekaźnika 1, nominalnie 800 ms |
| `PULSe:SECond` | `PULS:SEC` | Lokalny impuls przekaźnika 2, nominalnie 800 ms |
| `SYSTem:ERRor?` | `SYST:ERR?` | Pobiera najstarszy błąd tego połączenia; `0,"No error"` gdy kolejka pusta |

Odpowiedzi stanów są liczbami dziesiętnymi; klient powinien odczytać je jako
liczbę, a nie zakładać konkretnej liczby miejsc po przecinku.
`OUTP:FIRS ON/OFF` i `OUTP:SEC ON/OFF` są celowo niedostępne: mapowania stanów
są tylko do odczytu. `PULS:FIRS?` też nie jest obsługiwane.

Impulsy wykonują lokalne skrypty ESPHome `mode: single`: zerwanie połączenia
TCP nie anuluje odliczania, a powtórzenie komendy podczas impulsu nie wydłuża
go. Każdy przekaźnik ma osobny skrypt; można uruchomić oba jednocześnie.
Komenda powtórzona po zakończeniu uruchomi nowy impuls — protokół nie zapewnia
identyfikatorów operacji ani wykonania exactly-once. Nie ponawiaj automatycznie
komend po niepewnym wyniku transmisji.

Timer pracuje w głównej pętli ESPHome. Nominalne 800 ms nie jest sprzętowym
limitem czasu przy zawieszeniu firmware. Zmianę długości impulsu wykonuje się
w YAML (`delay`), nie przez SCPI. Konfiguracja udostępnia impulsy, nie osobną
operację power-cycle.

### Błędy i limity

`SYST:ERR?` zwraca kod i opis, np. `-113,"Undefined header"` dla nieznanej
komendy lub próby zapisu wyjścia, `-104,"Invalid parameter"` dla
`PULS:FIRS 123`, `-102,"Syntax error"` dla błędnej składni.
Kolejka jest przypisana do klienta i znika po zamknięciu połączenia.
Domyślnie: 2 klientów, linia 256 bajtów, bufor TX 1024 bajty,
kolejka 8 błędów, timeout bezczynności 10 s. Brak średnikowych list komend,
`*RST`, `*CLS` i `*OPC?`.

### Przykład klienta

```python
import socket
import time

with socket.create_connection(("192.168.2.11", 5025), timeout=2) as sock:
    sock.settimeout(2)
    stream = sock.makefile("rb")
    def query(command):
        sock.sendall((command + "\n").encode("ascii"))
        return stream.readline().decode("ascii").strip()
    print(query("*IDN?"))
    sock.sendall(b"PULS:FIRS\n")
    time.sleep(0.15)
    print("DI1:", query("INP:FIRS?"))
    time.sleep(1.0)
    print("Relay1:", query("OUTP:FIRS?"))
    print("Error:", query("SYST:ERR?"))
    stream.close()
```

## Zweryfikowane połączenia

| Funkcja | GPIO / ustawienie |
|---|---|
| Przekaźnik 1 (lewy), 2 (prawy) | 15, 2; aktywne HIGH |
| Wejście 1, 2 | 33, 32; aktywne LOW |
| LAN8720A MDC, MDIO | 23, 18 |
| Zewnętrzny REF_CLK 50 MHz | GPIO0, `CLK_EXT_IN` |
| Enable oscylatora | GPIO5 |
| Adres PHY | 1 |
| RS485 RX, TX, DE | 16, 17, 12; DE aktywne HIGH |
| UART RS485 | 19200, 8N1 |

## Testy fizyczne — 2026-10-14

[Raport](../reports/esp32-ipc-scpi-hardware-2026-10-14.json): **14/14 PASS**.
Przekaźniki były połączone z odpowiadającymi im wejściami przez zewnętrzne
12 V. Dla każdego kanału sprawdzono zwykły impuls, rozłączenie/reconnect TCP
w trakcie impulsu i powtórzenie polecenia w trakcie. Wejście potwierdzało
rzeczywiste przełączenie styków, a następnie wracało do nieaktywnego stanu.
Zmierzony hostem powrót: 0,795–0,861 s, z opóźnieniem odpytywania TCP.
Sprawdzono oba kanały naraz, odrzucanie nieprawidłowych komend i kolejkę błędów.
Stan końcowy: oba przekaźniki OFF i oba wejścia nieaktywne.

Ten test obejmuje utratę połączenia TCP, nie fizyczne odłączenie kabla.
Krótkie impulsy przy resecie, zimnym starcie i brownout pozostają
**NIEZAKWALIFIKOWANE**; `ALWAYS_OFF` opisuje inicjalizację programową.
GPIO2 i GPIO15 są pinami strapping ESP32. Odczyt `OUTP` jest stanem
programowego sterowania, nie sprzężeniem zwrotnym styków; takie potwierdzenie
w teście pochodziło z fizycznej pętli do wejść.
