# Log startu modemu: USB i EEPROM

Wgraj tę wersję szkicu **raz**. Potem debug włączasz i wyłączasz przez USB, bez ponownej kompilacji. `DEBUG_MODEM=1` w pliku konfiguracyjnym oznacza obecność tej funkcji w programie, a nie aktualne włączenie logowania. Pusty EEPROM oznacza debug OFF.

## Menu PowerShell

Zamknij Monitor portu szeregowego Arduino IDE. W PowerShell, z katalogu zawierającego folder `tools`, uruchom:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\capture-modem.ps1 -Port COM5 -Seconds 180
```

Zastąp COM5 portem Mega. Bez parametru Port skrypt pokaże listę i poprosi o wpisanie numeru. Bypass dotyczy tylko tego procesu, nie zmienia ustawień systemu. Skrypt nie potrzebuje Pythona ani dodatkowych pakietów.

Menu:

```text
1. Wlacz debug i zapisz flage w EEPROM
2. Rozpocznij zapis logu do pliku (Q konczy)
3. Wylacz debug i zapisz flage w EEPROM
0. Zamknij skrypt bez zmiany flagi
```

1. Wybierz **1**. Poczekaj na `DEBUG ON SAVED`, które potwierdza zapis EEPROM.
2. Wybierz **2**. Skrypt zapisuje plik `modem-YYYYMMDD-HHMMSS-fff.log`. Jeżeli debug jest OFF, poprosi o wybranie 1; sam go nie włącza.
3. Aby przechwycić start, naciśnij RESET Mega **po rozpoczęciu zapisu pliku**. Flaga ON przetrwa reset. Reset Mega nie musi resetować modemu; odnotuj, czy odłączałeś także jego zasilanie.
4. **Q** kończy zbieranie i wraca do menu. Po 180 s następuje to samo. **Nie zmienia to flagi w EEPROM ani nie wyłącza debugowania.**
5. Wyłącz debug dopiero przez **3** i poczekaj na `DEBUG OFF SAVED`. Możesz też wybrać **0** i pozostawić debug ON na następny start.

Koniec czasu, Q, wyjście i błąd skryptu nigdy nie wysyłają automatycznie DEBUG OFF. Ctrl+C kończy proces zamiast wracać do menu; do normalnego zakończenia zapisu używaj Q. Zamknięcie portu może zmienić DTR, a otwarcie USB może zresetować Mega, lecz nie zmienia zapisanej flagi. Podczas oczekiwania w menu skrypt nie zapisuje danych; bufor odbiorczy PC jest ograniczony, więc pełny log zbieraj opcją 2.

Prześlij cały `.log`, także z nieudanego startu. Dopisz model modemu, sposób zasilania, zachowanie diody i czy resetowałeś samo Arduino, czy również modem. `-OutputFile` pozwala podać własną nazwę; istniejący plik nie jest nadpisywany. Przy kolejnej próbie z tą samą nazwą otrzymasz błąd i wrócisz do menu.

## Komendy i trwałość ustawienia

Możesz użyć także terminala USB 115200, 8N1, z zakończeniem linii:

```text
DEBUG ON
DEBUG OFF
DEBUG STATUS
```

ON/OFF zmieniają stan bieżący oraz startowy. STATUS tylko odczytuje stan; nie zapisuje EEPROM. Odpowiedzi to `DEBUG ON SAVED`, `DEBUG OFF SAVED` albo `DEBUG SAVE ERROR`. Brak odpowiedzi nie dowodzi, że komenda nie zadziałała; skrypt ponawia ją i zgłasza brak potwierdzenia zamiast udawać sukces.

Ustawienie zajmuje adres **18**: A5 oznacza ON, 5A OFF; inne wartości (w tym pusty bajt FF) uruchamiają OFF. Dotychczasowe ustawienia pod adresami 0–17 pozostają bez zmian. Kod sprawdza gotowość EEPROM przed odczytem/zapisem, zapisuje tylko zmieniony bajt i sprawdza wynik w następnych wywołaniach. Nie czeka w pętli na zakończenie zapisu. SAVED wysyła dopiero po potwierdzeniu stanu i przy wolnym miejscu w USB. Przechowuje jedynie ostatnie żądanie/potwierdzenie, bez rosnącej kolejki. Wyłączenie logowania działa od razu; odcięcie zasilania przed SAVED może pozostawić poprzednią flagę.

Obsługa konsoli następuje w pętli aplikacji; podczas startu sprzętu lub priorytetowego odbioru GPS odpowiedź może się opóźnić. Skrypt ponawia komendę przez maksymalnie 20 s. Nie włączaj starych `DEBUG` i `DEBUGVPW` równocześnie: ich wydruki nie mają zabezpieczeń tej diagnostyki. Binarne dane LTE i tempo SD pozostają bez zmian.

## Czytanie logu

Komendy diagnostyczne przez USB (115200):

- `RTC`: `rtc` oznacza dostępny poprawny czas, `errors` liczy odrzucone odczyty
  i skoki zegara, `timeout=1` oznacza co najmniej jeden timeout I²C od startu.
  `epoch` opisuje czas lokalny RTC, a nie UTC.
- `QUEUE`: nazwa wybranego pliku, jego dostępny trwały rozmiar i offset potwierdzony
  przez API. `q=0` oznacza brak wybranego pliku. Gwiazdka LTE na ekranie sygnalizuje
  gotowość modemu; nie potwierdza przyjęcia CSV przez serwer.

Limit I²C wynosi 25 ms. Po błędzie odczytu RTC czas biegnie od ostatniej poprawnej
wartości według `millis()`. Program odrzuca niepełne dane, błędny BCD, daty spoza
2020–2099 i nieoczekiwane skoki większe niż 2 s. Jawne ustawienie czasu przez NTP,
USB lub zmianę DST aktualizuje także zapamiętany czas. NTP nadal koryguje tylko
różnicę ponad 30 s, chyba że nie ma jeszcze poprawnego czasu. SD nie odpytuje I²C
podczas zapisu ani aktualizacji daty FAT; przed pierwszym poprawnym czasem nie
tworzy logu CSV. Dokładność czasu zastępczego zależy od oscylatora Mega.

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
| PW | Etap PWRKEY: 0 impuls wyłączenia, 1 zwolnienie i oczekiwanie, 3 impuls włączenia, 4 oczekiwanie na start |
| RA / RK / RE | RTC przestawiony / czas w tolerancji 30 s / błąd synchronizacji; RA i RK podają epoch UTC |
| CQ | Komenda przekazana do automatu, jeszcze nie dowód wysłania |
| TX | Liczba bajtów komendy wraz z CR/LF przyjętych do UART modemu |
| RX / R+ | Linia odpowiedzi / kolejny fragment długości 24 znaków |
| DX / D+ | Tekst odebrany podczas opróżniania UART; nie służył potwierdzeniu bieżącej komendy |
| OK / ER | Wynik transakcji i faza automatu |
| TO | Timeout i faza: 0 opróżnianie, 1 nadawanie komendy, 2 oczekiwanie na odpowiedź |
| OV | Linia przekroczyła bufor parsera AT; to nie licznik przepełnień sprzętowego UART |
| BS / BE | Początek / zakończenie przekazywania binarnego POST do UART, liczba bajtów; bez treści |
| HB | Co 5 s: wolny RAM / pominięte wpisy / skrócone wpisy. Pomiar RAM dotyczy tej chwili, nie maksymalnego użycia stosu. |

`M`: 0 inicjalizacja, 1 bezczynność, 2 kontrola CGATT, 3 kontrola CREG, 4 GPS, 5 HTTP, 6 odzyskiwanie, 7 PWRKEY, 8 synchronizacja czasu. Znak `~` oznacza skrócenie szczegółu do 24 znaków. Długie RX dzielimy na fragmenty; brak miejsca w USB może usunąć dowolny z nich. `HB` może też zostać pominięty. Liczniki pominięć i skróceń zatrzymują się na 65535.

Etapy inicjalizacji bieżącego trybu CSV:

| S | Komenda |
| ---: | --- |
| 0 | AT |
| 1 | ATE0 |
| 2 | AT+CPIN? |
| 3 | AT+CNMP=2 (automatyczny wybór sieci) |
| 4 | AT+CGDCONT (APN internet) |
| 5 | AT+CGATT? |
| 6 / 7 | AT+NETOPEN? / AT+NETOPEN |
| 8 | AT+HTTPTERM (opcjonalne sprzątanie) |
| 9 / 10 | AT+CGPS? / AT+CGPS=1 |
| 11 | Inicjalizacja zakończona |

## Ograniczenia i weryfikacja

Każdy wpis mieści się w 63 bajtach. Przed zapisem program sprawdza miejsce na cały wpis w USB i pomija go, gdy miejsca brakuje. Nie używa `flush()`, `delay()`, `String`, sterty ani kolejki zaległych logów. Bufor fragmentu RX ma 25 B; bufor formatujący wpis 64 B leży na stosie. Formatowanie i przerwania nadawania USB mają koszt CPU, więc debug nie jest całkowicie obojętny czasowo.

Nie wypisujemy zawartości POST. Na czas nadawania jego treści i oczekiwania na potwierdzenie wyciszamy także RX, aby nie ujawnić ewentualnego echa tokenu. Po błędzie wyciszenie trwa do ponownej inicjalizacji. Pozostałe odpowiedzi mogą zawierać adres IP i pozycję GPS. Arduino HardwareSerial nie udostępnia tu licznika przepełnień RX; licznik HB dotyczy utraconych wpisów diagnostycznych, nie danych modemu.

Kompilacja z zabezpieczeniem RTC i ponawianiem DPID: 4814 B statycznego SRAM (+14 B względem wersji z 8 listopada). Diagnostyka działa również przy pełnym buforze USB, pomijając wpisy zamiast czekać. Testy hosta obejmują EEPROM, reset watchdogiem, parser, PWRKEY, NTP i transfer CSV. Próba sprzętowa potwierdziła wznowienie PWRKEY i NTP; heartbeat podczas próby raportował 3180 B wolnego RAM. Pomiar ten nie jest gwarancją maksymalnego zużycia stosu.
