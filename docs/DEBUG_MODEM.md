# Log startu modemu

## Włączenie

W `ls1gauge-mega/modem_debug_config.h` zmień `#define DEBUG_MODEM 0` na `#define DEBUG_MODEM 1`, skompiluj i wgraj szkic. Pozostałe przełączniki `DEBUG`, `DEBUGVPW` pozostaw wyłączone: ich stare wydruki nie mają zabezpieczeń tej diagnostyki. Po zakończeniu diagnozy przywróć `DEBUG_MODEM 0`.

Log wychodzi przez USB Mega (`Serial`) z prędkością **115200, 8N1**. Port `Serial1` pozostaje połączeniem z modemem. Nie dodano logowania na SD ani zmian komend, timeoutów czy formatu API.

## Zapis do pliku w Windows

1. Rozpakuj ZIP z zachowaniem katalogów i wgraj szkic z włączonym debugiem.
2. Sprawdź numer COM Arduino w Arduino IDE. Zamknij Monitor portu szeregowego i inne programy używające tego portu.
3. Otwórz PowerShell w katalogu zawierającym folder `tools` z ZIP-a. Uruchom, zastępując `COM5` swoim portem:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\capture-modem.ps1 -Port COM5 -Seconds 180
```

Przełącznik Bypass dotyczy tylko uruchamianego procesu; skrypt nie zmienia zasad systemowych. Uruchomienie skryptu bez `-Port` wypisze dostępne porty. Nie wymaga Pythona ani dodatkowych pakietów.

4. Skrypt poda ścieżkę pliku `modem-YYYYMMDD-HHMMSS.log` i pokaże odbierany tekst. Otwarcie portu z DTR może zresetować Mega. Jeśli start nie pojawi się w logu, naciśnij RESET Arduino po rozpoczęciu zbierania. Sam reset Mega nie musi resetować modemu.
5. Po 180 s skrypt zamknie plik i zwolni port. Możesz też zakończyć go przez Ctrl+C. Istniejącego pliku o tej samej nazwie nie nadpisze.

Prześlij cały `.log`, także gdy modem w końcu wystartował. Dopisz model modemu, sposób zasilania, zachowanie diody i informację, czy reset dotyczył samego Arduino, czy również zasilania modemu. Warto zachować osobne logi nieudanego i udanego startu. Skrypt nie otwiera ani nie resetuje portu, dopóki nie uruchomisz go z `-Port`.

## Czytanie logu

Przykład formatu (nie zapis rzeczywistej próby):

```text
1200 M0 S1 R0 CQ AT+CPIN?
1202 M0 S1 R0 TX 10
1230 M0 S1 R0 RX +CPIN: READY
5000 M0 S4 R1 HB 2580/3/0
```

Pierwsza liczba to `millis()` w chwili przetwarzania zdarzenia, nie czas nadejścia bajtu na przewodzie. Zeruje się po restarcie, zawija po około 49,7 dnia. `M` oznacza właściciela UART, `S` etap inicjalizacji, `R` liczbę wcześniejszych nieudanych prób etapu. Po inicjalizacji `S` pozostaje na końcowej wartości i nie oznacza etapu HTTP.

| Kod | Znaczenie |
| --- | --- |
| BO | Surowa wartość MCUSR odczytana na początku setup, dziesiętnie. Bity AVR: 1 zasilanie, 2 reset zewnętrzny, 4 brown-out, 8 watchdog. Bootloader może je wcześniej skasować lub zmienić; zero nie wyklucza restartu. |
| ST | Start/restart inicjalizacji (0), gotowość (1) lub odzyskiwanie po błędzie (6) |
| CQ | Komenda przekazana do automatu, jeszcze nie dowód wysłania |
| TX | Liczba bajtów komendy wraz z CR/LF przyjętych do UART modemu |
| RX / R+ | Linia odpowiedzi / kolejny fragment długości 24 znaków |
| DX / D+ | Tekst odebrany podczas opróżniania UART; nie służył potwierdzeniu bieżącej komendy |
| OK / ER | Wynik transakcji i faza automatu |
| TO | Timeout i faza: 0 opróżnianie, 1 nadawanie komendy, 2 oczekiwanie na odpowiedź |
| OV | Linia przekroczyła bufor parsera AT; to nie licznik przepełnień sprzętowego UART |
| BS / BE | Początek / zakończenie przekazywania binarnego POST do UART, liczba bajtów; bez treści |
| HB | Co 5 s: wolny RAM / pominięte wpisy / skrócone wpisy. Pomiar RAM dotyczy tej chwili, nie maksymalnego użycia stosu. |

`M`: 0 inicjalizacja, 1 bezczynność, 2 kontrola CGATT, 3 kontrola CREG, 4 GPS, 5 HTTP, 6 odzyskiwanie. Znak `~` oznacza skrócenie szczegółu do 24 znaków. Długie RX dzielimy na fragmenty; brak miejsca w USB może usunąć dowolny z nich. `HB` może też zostać pominięty. Liczniki pominięć i skróceń zatrzymują się na 65535.

Etapy inicjalizacji:

| S | Komenda |
| ---: | --- |
| 0 | AT |
| 1 | AT+CPIN? |
| 2 | AT+CNMP=13 |
| 3 | AT+CGDCONT (APN internet) |
| 4 | AT+CREG? |
| 5 | ATE0 |
| 6 / 7 | AT+NETOPEN? / AT+NETOPEN |
| 8 | AT+IPADDR |
| 9 / 10 | AT+HTTPTERM / AT+HTTPINIT |
| 11 / 12 | HTTP CID / URL |
| 13 | AT+HTTPACTION=0, oczekiwana odpowiedź HTTP 403 |
| 14 | AT+HTTPTERM |
| 15 / 16 | AT+CGPS? / AT+CGPS=1 |

## Ograniczenia i weryfikacja

Każdy wpis mieści się w 63 bajtach. Przed zapisem program sprawdza miejsce na cały wpis w USB i pomija go, gdy miejsca brakuje. Nie używa `flush()`, `delay()`, `String`, sterty ani kolejki zaległych logów. Bufor fragmentu RX ma 25 B; bufor formatujący wpis 64 B leży na stosie. Formatowanie i przerwania nadawania USB mają koszt CPU, więc debug nie jest całkowicie obojętny czasowo.

Nie wypisujemy zawartości POST. Na czas nadawania jego treści i oczekiwania na potwierdzenie wyciszamy także RX, aby nie ujawnić ewentualnego echa tokenu. Po błędzie wyciszenie trwa do ponownej inicjalizacji. Pozostałe odpowiedzi mogą zawierać adres IP i pozycję GPS. Arduino HardwareSerial nie udostępnia tu licznika przepełnień RX; licznik HB dotyczy utraconych wpisów diagnostycznych, nie danych modemu.

Kompilacja Mega / AVR Boards 1.8.8:

| DEBUG_MODEM | Flash | Statyczny SRAM | Pozostało na stos/stertę |
| --- | ---: | ---: | ---: |
| 0 | 85 332 B | 5423 B | 2769 B |
| 1 | 86 376 B | 5458 B | 2734 B |

Sześć programów testowych przechodzi, w tym cały test modemu z debugiem wyłączonym i włączonym: pełny USB, powolny UART modemu, inicjalizacja, GPS, oba POST z CRC, timeouty i odzyskiwanie. Nie wykonano próby na fizycznym modemie. Skrypt sprawdzono składniowo oraz uruchomiono listowanie portów w Windows PowerShell 5.1; nie otwierano portu Arduino ani nie przechwytywano rzeczywistego logu.
