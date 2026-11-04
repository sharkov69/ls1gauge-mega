# Szybszy ekran, SD i LTE — 2026-11-03

Ten raport opisuje wersję `8d95a67`. Aktualizacja z 2026-11-04 przywraca nazwy `l-`, zmienia CSV na format polskiego Excela z jedną kolumną czasu i naprawia daty FAT. Obecny format oraz wyniki pamięci opisuje [CSV_EXCEL.md](CSV_EXCEL.md). Interwały TFT/SD/LTE z tego raportu pozostają aktualne.

Weryfikacja oryginału i wersji DPID potwierdziła, że logger nie zbierał wszystkich odebranych próbek. Odbiór zastępuje bieżące wartości w `cmd[]`. Ekran pobierał je co 400 ms, a logger co sekundę. Średnia z dwóch takich odczytów dawała jeden rekord co 2 s. Nie była to średnia ze wszystkich ramek między zapisami. Ten schemat pochodził z oryginalnego projektu.

## Nowe interwały

| Odbiorca | Wcześniej | Teraz, nominalnie |
| --- | --- | --- |
| TFT 1 | 400 ms, 2,5 Hz | 100 ms, 10 Hz |
| TFT 2 | Obsłużony impuls RTC 1 Hz | Bez zmiany; dane czujników nadal co 2500 ms |
| CSV na SD | Rekord co 2 s, zapis 30 rekordów około raz/min | Pojedynczy bieżący zestaw co 200 ms, do około 300 wierszy/min |
| Rekord w buforze LTE | Co 2 impulsy RTC | Co 1 impuls RTC, około 60 rekordów/min |
| Wysyłka LTE | 30 rekordów około co 60 s | 30 rekordów około co 30 s |
| Licznik ekranowy | TX i RX | Tylko RX; usunięto też liczenie TX |

TFT1 w obu widokach odmalowuje pola po zmianie wartości. Widok uproszczony nie czyści już liczb prędkości i RPM w każdym cyklu. Ograniczenie RPM do 6000 i zaokrąglenie do 25 obr./min dotyczy teraz tylko prezentacji; wcześniej GUI modyfikowało pomiar używany przez logger. Filtr obu AFR nadal obejmuje 4 próbki.

## SD i format CSV

Szybkie logi mają nazwę `f-YYMMDD.csv`, np. `f-261103.csv`. Dotychczasowe `l-*.csv` pozostają na karcie, ale ta wersja już do nich nie dopisuje. Nowy prefiks zapobiega mieszaniu dwóch układów kolumn.

Plik ma 19 kolumn: `Datetime`, `Uptime_ms` i 17 dotychczasowych wartości kanałów, w dotychczasowej kolejności i skalach. `Datetime` pochodzi z RTC i ma rozdzielczość jednej sekundy. `Uptime_ms` to liczba milisekund od startu Mega w chwili pobrania zestawu; różnica kolejnych wartości pokazuje rzeczywisty odstęp zapisów. Licznik zawija się po około 49,7 dnia, a restart zeruje go. Zapis SD nadal używa dotychczasowych dzielników CSV, w tym istniejącej rozbieżności skali oleju oraz tylko jednego miejsca po przecinku dla GPS.

SD zapisuje niezależny zestaw 17 wartości wprost z aktualnego stanu. Nie zmienia `logBuffer`, `logIndex` ani generacji paczki LTE. Bufor pojedynczego rekordu zajmuje 42 B na stosie, bez drugiej kolejki logów. Plik pozostaje otwarty; program wywołuje `sync()` co około sekundę oraz przed zmianą pliku na kolejny dzień. Błąd otwarcia/zapisu/synchronizacji zamyka plik i odracza kolejną próbę o 5 s.

Po opóźnieniu program zapisuje jeden aktualny zestaw, bez nadganiania zaległych terminów kopiami tej samej wartości. To nadal zapis stanu co 200 ms, nie zapis każdej ramki DPID. Przy 10 Hz danego kanału CSV zachowa nominalnie 5 zestawów/s; wolniejsze czujniki i GPS mogą powtarzać ostatnią wartość. Nie ma nowych flag świeżości pomiarów.

Komenda `SD` w terminalu 115200 zwraca np. `sd_rows=300 errors=0 interval_ms=200`. `sd_rows` oznacza ukończone w kodzie zapisy wierszy, nie dowód fizycznego utrwalenia na nośniku. Nagłe odłączenie zasilania może utracić ostatnie niesynchronizowane dane.

## Zgodność LTE z API

Każdy rekord `LogEntry` nadal ma 42 B, cały bufor 30 rekordów ma 1260 B, a pojedynczy POST 647 B. Nadal są to dwa POST-y po 15 rekordów, ten sam token, rok/miesiąc, kolejność pól, kodowanie i CRC16. Kolumna `Uptime_ms` należy wyłącznie do CSV; nie dodano jej do danych API.

Zmiana `SREDNIA` z 2 na 1 oznacza, że rekord LTE zawiera jeden stan pobrany przy impulsie 1 Hz, zamiast średniej z dwóch stanów. Bufor zapełnia się nominalnie po 30 s. Trwająca wysyłka blokuje rozpoczęcie kolejnej; istniejąca kontrola generacji/indeksu przerywa wysyłkę drugiej połowy, jeśli akwizycja już ją nadpisała. Przy 1 Hz druga połowa pozostaje dostępna przez około 15 s od ponownego użycia bufora. Nie zmieniono automatu HTTP ani procedury odzyskiwania po błędzie.

Test modemu potwierdza układ, długości i CRC obu POST. Nie wykonano rzeczywistego żądania do API ani testu obciążenia serwera częstszą wysyłką.

## Pamięć i ograniczenia czasowe

| Wersja | Flash | Statyczny SRAM | Pozostało na stos/stertę |
| --- | ---: | ---: | ---: |
| Poprzednia, z TX/RX | 81 464 B | 5431 B | 2761 B |
| Szybszy TFT/SD/LTE, tylko RX | 81 664 B | 5402 B | 2790 B |

Pomiar dotyczy Mega 2560, AVR Boards 1.8.8 oraz bibliotek opisanych w URUCHOMIENIE.md. Bufor LTE i bufor POST nie urosły. SRAM statyczny zmalał o 29 B; kompilator nie mierzy maksymalnego użycia stosu. Nie dodano dynamicznego `String`, oczekiwania w `delay()` ani pętli czekającej na SD.

Watchdog 4 s pozostał aktywny. Operacje SdFat, SPI i DHT nadal są synchroniczne; watchdog nie gwarantuje, że wadliwa karta lub zasilanie nie spowodują restartu. Priorytetowy odbiór GPS może odłożyć ekran i SD do odpowiedzi lub timeoutu 1 s. Flaga RTC jest pojedynczym `bool`, więc przy długim opóźnieniu impulsy mogą się zlać. Podane interwały są celami harmonogramu, a nie pomiarem na urządzeniu.

Pięć zestawów testów hosta obejmuje VPW, DPID, modem oraz SD/BMP. Test SD sprawdza interwał 200 ms, brak serii nadganiających, synchronizację, ponowienie po błędzie, zmianę dnia, zawinięcie zegara i brak naruszenia bufora LTE. Kompilacja AVR przechodzi. Na sprzęcie sprawdź `MEM`, `SD`, różnice `Uptime_ms`, oba pakiety POST i reakcję ekranu przy jednoczesnym GPS/LTE. RX nadal sumuje 20 przedziałów nominalnie po 3 s; wydłużenie pętli może wydłużyć to okno.
