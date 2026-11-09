# Działanie i format danych

[Powrót do README](../README.md)

Aktualizacja 2026-11-08: jedynym transportem jest CSV przez HTTPS. Konfigurację SD, PWRKEY i NTP opisuje [CSV_UPLOAD.md](CSV_UPLOAD.md). EEPROM 64–95 przechowuje trwały licznik plików.

## Start i główna pętla

W `setup()` program uruchamia porty szeregowe, rozpoczyna inicjalizację modemu, konfiguruje czujniki i RTC, inicjalizuje SD, odczytuje EEPROM, uruchamia TFT oraz VPW. Na końcu włącza watchdog AVR z limitem 4 s. Brak RTC zatrzymuje start; błąd SD tylko generuje komunikat diagnostyczny i pozwala kontynuować.

W `loop()` program obsługuje odbiór i nadawanie VPW, ekrany, przyciski, czujniki, logi, terminal oraz automat modemu. Modem oddaje sterowanie między porcjami danych. Podczas oczekiwania na długą odpowiedź GPS pętla obsługuje VPW RX i modem, odkładając wolniejsze zadania do zakończenia odpowiedzi lub timeoutu 1 s. SD, grafika i DHT nadal mają koszty synchroniczne; poniższe interwały są nominalne.

| Zadanie | Interwał / warunek |
| --- | --- |
| Początek wyświetlania wskaźników | Około 5 s po zapisaniu czasu startu GUI |
| Odświeżanie parametrów TFT 1 | 100 ms |
| Odczyt DHT22 i DS18B20, próba pobrania GPS | 2500 ms; GPS tylko po inicjalizacji i poza wysyłką logu |
| Aktualizacja zegara i sumowanie pomiarów | Impuls SQW 1 Hz |
| Wysyłka LTE/HTTPS | Z kolejki SD, do 1024 B CSV na żądanie; seria do 16 żądań, potem zwolnienie UART |
| Czas z internetu | Po połączeniu LTE i co 6 godzin; ponowienie błędu po 5 minutach, korekta RTC tylko powyżej 30 s |
| Zapis szybkiego CSV na SD | Jeden zestaw co 200 ms, niezależnie od LTE |
| Statystyka RX | 20 przedziałów po 3 s, nominalnie ostatnia minuta |
| Obsługa watchdog AVR | Odświeżenie co około 2 s przy limicie 4 s |

Flaga `tick` jest typu `bool`. Jeśli pętla utknie na dłużej niż sekundę, kilka impulsów SQW może zlać się w jeden obsłużony cykl.

## Zapytania VPW

Domyślnie program przełącza 11 kanałów na [dwa pakiety DPID](DPID.md), nominalnie co około 200 ms każdy. Poniższy harmonogram pojedynczych zapytań obowiązuje przed konfiguracją, po powrocie do legacy lub po wyłączeniu DPID. Przeliczniki obu trybów są wspólne.

`send()` wybiera najbardziej opóźnione zapytanie z tablicy `cmd[]`. Respektuje minimalny odstęp `pause` oraz 10 ms od ostatniego przetworzonego odbioru. Wywołuje `vpw.send(..., 1)`, ograniczając oczekiwanie na wolną magistralę do 1 ms. Czas ostatniego wysłania aktualizuje tylko po sukcesie biblioteki. Pomiar TX usunięto. Sposób zliczania kanałów w trybie pakietowym opisuje [DPID.md](DPID.md). To potwierdzenie nadania bez wykrytej kolizji, nie odpowiedzi ECU.

Zapytania trybu `01` używają nagłówka `68 6A F1`; rozszerzone zapytania trybu `22` nagłówka `6C 10 F1` i dodatkowego bajtu `01` po PID. Biblioteka dopisuje CRC.

| Indeks | Symbol | Znaczenie w programie | Tryb / PID | Nominalne odpytywanie |
| --- | --- | --- | --- | --- |
| 0 | `AIR` | Temperatura powietrza dolotowego | `01 / 0F` | 5000 ms |
| 1 | `MAP` | Ciśnienie w kolektorze | `01 / 0B` | 500 ms |
| 2 | `TPS` | Położenie przepustnicy | `01 / 11` | 250 ms |
| 3 | `BATT` | Napięcie | `22 / 1141` | 5000 ms |
| 4 | `O22` | AFR banku 2 według przelicznika projektu | `22 / 114E` | 500 ms |
| 5 | `SPEED` | Prędkość pojazdu | `01 / 0D` | 500 ms |
| 6 | `TRANS` | Temperatura skrzyni | `22 / 1940` | 5000 ms |
| 7 | `SPARK` | **Ciśnienie oleju**, mimo nazwy symbolu | `22 / 115C` | 2500 ms |
| 8 | `RPM` | Obroty silnika | `01 / 0C` | 250 ms |
| 9 | `WATER` | Temperatura płynu chłodzącego | `01 / 05` | 5000 ms |
| 10 | `O2` | AFR banku 1 według przelicznika projektu | `22 / 114B` | 500 ms |
| 11–14 | `TEMP_AC`, `HUM`, `TEMP_OUT`, `TEMP_IN` | DHT22 i DS18B20 | Bez zapytań VPW | 2500 ms |
| 15–16 | `LAT`, `LON` | Pozycja GPS z modemu | Bez zapytań VPW | Próba co 2500 ms |

To mapa obsługi obecnego programu, nie deklaracja zgodności rozszerzonych PID z każdym sterownikiem GM. Dopasowanie PID i przeliczników do innego ECU wymaga sprawdzenia.

### Przeliczniki

Dla odpowiedzi trybu `01` oznaczmy `A = pData[5]`, `B = pData[6]`. Kod oblicza RPM jako `(256*A+B)/4`, temperatury powietrza i płynu jako `A-40`, TPS jako `A*100/255`, prędkość jako `A`, a MAP przechowuje jako `A` i dzieli przez 100 przy prezentacji w barach.

Dla rozszerzonych odpowiedzi używa `C = pData[6]`: temperatura skrzyni to `C-40`, napięcie to `C/10` V, a surowa wartość oleju to `max(0, ((C*111)>>8)-10)`. Ekran dzieli wartość oleju przez 10.

Dla obu kanałów AFR program wyznacza `1029 + 3*C + ((C*117+128)>>8)`, a następnie średnią ruchomą z 4 wartości dla obu sond szerokopasmowych. Wartości spoza 1029–1911 zastępuje 1475; do prezentacji stosuje skalę 1/100. Ten przelicznik jest założeniem projektu, nie potwierdzeniem dokładności pomiaru AFR.

Wartości czujników temperatury dodatkowej i wilgotności mają skalę 1/10. DS18B20 pracują z rozdzielczością 11 bitów, a program rozpoczyna konwersję bez oczekiwania i odbiera wynik w kolejnym cyklu.

### Biblioteka `J1850-VPW-WC`

Biblioteka mierzy odstępy między zboczami RX w przerwaniu. Używa bufora 32 ramek i rozmiaru `BS=13` bajtów na ramkę, łącznie z CRC. W odbiorze filtruje pierwszy bajt, dopuszczając `0x6C` i `0x48`; nie jest uniwersalnym rejestratorem wszystkich ramek VPW. `tryGetReceivedFrame()` sprawdza CRC każdej ramki i domyka ramkę także po ciszy na magistrali. Kolejka chroni odczyt krótką sekcją atomową; przepełnienie usuwa najstarszą ramkę i zwiększa licznik. Szkic dodatkowo sprawdza źródło `10`, adresowanie, usługę, PID i długość odpowiedzi.

Nadajnik generuje nominalne impulsy 64 i 128 µs oraz SOF 200 µs. Podczas nadawania odłącza przerwanie RX i sprawdza arbitraż przez odczyt wejścia. Dołączony kod ogranicza się do krótkich ramek i nie obsługuje danych IFR jako osobnego protokołu aplikacyjnego.

## Ekrany i demo

TFT 1 ma widok pełny i uproszczony. Łuk obrotomierza obejmuje zakres 0–6000 obr./min: zielony do 3500, żółty do 4500, dalej czerwony. GUI ogranicza RPM do 0–6000 i zaokrągla je do 25 obr./min tylko w lokalnej wartości do prezentacji; nie modyfikuje tym pomiaru loggera.

TFT 2 prezentuje zegar i czujniki w dwóch układach. Podczas pracy menu program wstrzymuje zwykłe rysowanie drugiego ekranu.

Demo nadpisuje wybrane pola wartościami losowymi co 500 ms, niezależnie od odświeżania GUI. Włącza się je i wyłącza wyłącznie pozycją `Demo: ON/OFF` w menu; stan pozostaje w EEPROM. Nie wyłącza odpytywania ECU ani rejestracji, dlatego log może zawierać mieszaninę pomiarów i danych demonstracyjnych.

Pokaz obrazka zamyka menu i zachowuje logo na drugim ekranie. Odliczanie 5 sekund do powrotu obu ekranów do wskazań zaczyna się po zakończeniu ładowania BMP. Ikony A/C i FAN nie rysują się podczas logo ani menu. Pod zegarem są wiersze In, Out, Vent i Hum; Vent jest temperaturą z czujnika DHT, nie stanem żądania A/C.

Poza menu obrót enkodera w lewo usypia oba ekrany, a w prawo je budzi. Powtórzenie ruchu w tę samą stronę nie przełącza stanu. Naciśnięcie otwiera menu i budzi ekrany. Odstęp ochronny między zaakceptowanymi naciśnięciami wynosi 450 ms, bez blokowania pętli.

Pozycja `Clock View` zmienia układ zegara, zapisuje wybór i zamyka menu, aby od razu pokazać nowy widok.

Pozycja `Dark: ON/OFF` ustawia intensywność kolorów obu ekranów na 40% lub 100%. Obejmuje tekst, ikony, łuki, menu, logo i BMP; nie steruje podświetleniem. Zmiana odrysowuje ekrany i zapisuje stan w EEPROM. Pusta komórka oznacza OFF. Funkcja przelicza składowe RGB565 bez bufora obrazu; zużycie statycznego RAM wzrosło o 1 bajt.

## CSV i pauza logowania

Podczas aktywnego logowania program zapisuje bieżący zestaw co 200 ms do l-YYMMDD.csv. CSV ma 36 kolumn, separator średnik i przecinek dziesiętny. Datetime zawiera milisekundy, GPS sześć miejsc po przecinku. Pole jest puste tylko przed pierwszym poprawnym pomiarem; potem pozostaje ostatnia wartość. Zapis nie odtwarza każdej ramki DPID.

Zapis uruchamia rzeczywista odpowiedź Battery (PID 1141), także z pakietu DPID FD. RPM=0 nie zatrzymuje logowania. Kolejne odpowiedzi Battery podtrzymują zapis, nawet gdy napięcie się nie zmienia. Po ponad 15 s bez Battery program synchronizuje i zamyka plik; pierwsza nowa odpowiedź wznawia zapis. Przed pierwszą odpowiedzią ECM nie tworzy pustych logów. Demo nie udaje odpowiedzi Battery. Pauza nie zatrzymuje wysyłania wcześniej zapisanych danych do API.

Format Excela i daty FAT opisuje [CSV_EXCEL.md](CSV_EXCEL.md), a bieżące kolumny [CSV-P59.md](CSV-P59.md).

## Modem, GPS i HTTPS

Inicjalizacja sprawdza SIM, ustawia automatyczny wybór sieci i APN internet, sprawdza CGATT, otwiera połączenie i uruchamia GPS. Nie odpytuje starego serwera. Powtarzający się brak odpowiedzi AT uruchamia nieblokującą sekwencję PWRKEY na D12. Brak zasięgu przy odpowiadającym modemie nie uruchamia tej sekwencji.

GPS korzysta z AT+CGPSINFO. Parser sprawdza współrzędne, zakresy i kierunki; brak fixa pozostawia ostatnią pozycję. Dolne słowa współrzędnych są w channels, górne w gpsHigh.

UART ma jednego właściciela: inicjalizacja, kontrola sieci, GPS, HTTPS, recovery, PWRKEY lub NTP. HTTPS przesyła zatwierdzone na SD bajty CSV do API. Po utracie potwierdzenia pyta o offset serwera; nie usuwa lokalnych logów. Szczegóły protokołu P59C/P59A, kwalifikacji TLS, błędów i ograniczeń opisuje [CSV_UPLOAD.md](CSV_UPLOAD.md).

NTP ustawia RTC tylko przy różnicy większej niż 30 s. Program przelicza UTC na czas Warszawy i uwzględnia godzinę zmiany czasu. Oczekiwanie na modem nie używa delay(). Operacje SD, I2C i grafika nadal mają własny synchroniczny koszt wykonania.

## EEPROM

| Adres | Dane |
| --- | --- |
| 0 | Flaga czasu letniego |
| 1–13 | Nieużywane po usunięciu starego tokenu |
| 14 | Demo |
| 15 | Widok wskaźników |
| 16 | Widok zegara |
| 17 | Pauza VPW w ms |
| 18 | Flaga DEBUG_MODEM |
| 19 | Tryb Dark (1 = ON, pozostałe = OFF) |
| 64–95 | Trwały licznik plików CSV |


Demo uruchamia się z EEPROM tylko dla zapisanej wartości `1`; pusta komórka `0xFF` go nie włącza. Oba tryby widoku nadal odczytują wartość jako `bool`. Pauza ma osobną kontrolę zakresu, która ustawia 75 ms dla wartości nieprawidłowej; usunięcie jej pozycji z menu nie zmienia harmonogramu VPW.

## Granice tej dokumentacji

Testy na stole nie zastępują próby z ECM w samochodzie. Nie zmieniono odczytu VPW ani skal PID. Dokładność rozszerzonych PID dla innych sterowników wymaga osobnego potwierdzenia.
