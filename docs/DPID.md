# Odczyt P59 w dwóch pakietach

Domyślnie Mega konfiguruje dwa cykliczne pakiety DPID. Wszystkie 11 kanałów samochodu zajmuje łącznie 12 bajtów danych, więc mieszczą się w dwóch odpowiedziach po 6 bajtów. Nie zmienia to szybkości elektrycznej VPW ani przeliczników; usuwa potrzebę osobnego zapytania o każdy pomiar.

## Podział kanałów

Offset liczony od pierwszego bajtu danych za identyfikatorem pakietu:

| Pakiet | Offset | Bajty | Kanał | PID |
| --- | ---: | ---: | --- | --- |
| FE | 0 | 2 | RPM | 000C |
| FE | 2 | 1 | TPS | 0011 |
| FE | 3 | 1 | MAP | 000B |
| FE | 4 | 1 | Prędkość | 000D |
| FE | 5 | 1 | Ciśnienie oleju | 115C |
| FD | 0 | 1 | AFR bank 1 | 114B |
| FD | 1 | 1 | AFR bank 2 | 114E |
| FD | 2 | 1 | Napięcie | 1141 |
| FD | 3 | 1 | Temperatura płynu | 0005 |
| FD | 4 | 1 | Temperatura dolotu | 000F |
| FD | 5 | 1 | Temperatura skrzyni | 1940 |

Tryb `2A 14` pochodzi z implementacji tego samego P59 w `codex-p59-scanner`. W tym wzorcu ECU wysyła każdy pakiet nominalnie co około 200 ms, czyli około 5 aktualizacji/s każdego kanału. Wersja Arduino wymaga potwierdzenia okresu na samochodzie. Normalna praca potrzebuje około 10 odpowiedzi danych/s i podtrzymania sesji co 2 s.

Ekrany nadal odświeżają parametry co 400 ms, logger zapisuje średnią co dwa impulsy 1 Hz, a oba AFR używają filtra 4 próbek. Szybsza akwizycja nie zmienia częstotliwości zapisu CSV ani formatu POST.

## Start i powrót do legacy

1. Mega rozpoczyna od pojedynczych zapytań. Pierwsza poprawna odpowiedź rozpoznanego PID potwierdza obecność ECU.
2. Wysyła `2A 00`, aby zatrzymać ewentualny poprzedni strumień, i oczekuje `6A 00`.
3. Definiuje 11 pól przez `2C`, po jednym na komendę. Kolejne pole wysyła po potwierdzeniu identyfikatora i selektora.
4. Wysyła `2A 14 FE FD FE FD`. Pełne echo z kodem `23` traktuje zgodnie ze wzorcem jako oczekiwanie na start; samo nie dowodzi pracy strumienia.
5. Poprawne pakiety FE i FD potwierdzają tryb aktywny. Mega wysyła `3F` co 2 s i nie odpytuje równolegle pojedynczych PID.

Odpowiedzi konfiguracyjne mają timeout 2 s. Błąd definicji przed próbą startu przywraca zwykłe odpytywanie. Brak potwierdzenia początkowego STOP też pozwala wrócić do niego, jeśli Mega nie wysłała START i nie widziała strumienia. Każda późniejsza ramka DPID ponownie uruchamia zatrzymywanie, aby nie mieszać dwóch trybów.

Po próbie START lub wykryciu strumienia powrót wymaga odpowiedzi `6A 00`. Brak jednego pakietu przez 3 s uruchamia zatrzymywanie. Bez potwierdzenia STOP Mega ponawia jego próbę, pozostawiając odpytywanie PID wyłączone; pozostała aplikacja działa. Po przejściu do legacy ponowna konfiguracja DPID nastąpi przy restarcie Mega.

Używaj jednego testera diagnostycznego. Drugi skaner może zmienić definicje tych samych DPID; zgodny identyfikator pakietu nie dowodzi, kto zdefiniował jego zawartość.

## Konfiguracja i diagnostyka

W `cmd.h` ustaw `VPW_DPID_ENABLED` na `0`, aby skompilować dotychczasowy odczyt pojedynczych PID. Domyślnie jest `1`. Menu `VPW pause` określa odstęp własnych transmisji Mega i tempo legacy; nie zmienia okresu pakietów ECU.

Przez terminal 115200 wyślij `VPW` z zakończeniem linii. Przykład:

```text
dpid=4 fe=123 fd=123 fail=0 err=0
```

| `dpid` | Znaczenie |
| ---: | --- |
| 0 | Zwykłe odpytywanie, oczekiwanie na ECU |
| 1 | Zatrzymywanie strumienia |
| 2 | Definiowanie pól |
| 3 | Start, oczekiwanie na oba pakiety |
| 4 | Aktywny strumień FE/FD |
| 5 | Legacy po rezygnacji z DPID lub jego wyłączeniu w kompilacji |

`fe` i `fd` liczą zaakceptowane pakiety. `fail` liczy błędy/timeouty; `err` zachowuje ostatni powód: 1 to timeout, 2 to brak pakietu przez 3 s, pozostałe wartości to kod odmowy ECU zapisany dziesiętnie. Liczniki 16-bitowe mogą się przepełnić. `MEM` nadal pokazuje pamięć, CRC i przepełnienia kolejki. Przy pełnym TX terminala odpowiedź może zostać pominięta.

## Pamięć i weryfikacja

Kompilacja Mega 2560, Arduino AVR Boards 1.8.8 i te same biblioteki co w etapie 1:

| Wersja | Flash | Statyczny SRAM | Pozostało na stos/stertę |
| --- | ---: | ---: | ---: |
| Baza przed etapem 1 | 78 642 B | 6532 B | 1660 B |
| Etap 1 | 79 290 B | 5176 B | 3016 B |
| Dwa pakiety DPID | 81 080 B | 5389 B | 2803 B |

DPID dodaje 213 B statycznego SRAM, głównie przez powiększenie 32-elementowej kolejki ramek z 8 do 13 bajtów. Dane DPID mają 12 bajtów z nagłówkiem i CRC; pełna odmowa definicji z echem może mieć 13. Definicje są w PROGMEM, bez dodatkowej alokacji na stercie.

Pięć programów testowych hosta przechodzi: VPW, modem, SD/BMP, automat DPID i integracja z dekoderem. Nowe testy obejmują ramki 12/13 bajtów, wszystkie definicje, potwierdzenia, obcy ECU, złe długości, start, keepalive, timeout, zanik jednego pakietu, STOP, spóźniony strumień, zajęty TX, zawinięcie czasu i zgodność 11 wartości z legacy. Przegląd kodu objął także odstęp TX po RX. Uruchomienie: `./tests/run-host-tests.ps1`.

Nie wgrywano firmware ani nie mierzono magistrali. Do próby: wyłącz demo, sprawdź `dpid=4`, przyrost obu liczników oraz zgodność wartości z legacy. Zmierz okres FE/FD i liczniki CRC/drop przy DHT, odświeżaniu TFT i logowaniu. DHT nadal może blokować przerwania, a SD/TFT wykonują operacje synchroniczne.

Źródła implementacji: [MpviDynamicPlan.cs](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/src/Scanner.Agent/Hardware/MpviDynamicPlan.cs) i [MpviLiveSource.Dynamic.cs](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/src/Scanner.Agent/Hardware/MpviLiveSource.Dynamic.cs), odczytane 2026-10-30. Zmiana dotyczy diagnostyki, nie zapisu kalibracji ani Flash ECU.
