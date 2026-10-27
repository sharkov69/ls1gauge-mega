# Działanie i format danych

[Powrót do README](../README.md)

## Start i główna pętla

W `setup()` program uruchamia porty szeregowe, rozpoczyna inicjalizację modemu, konfiguruje czujniki i RTC, inicjalizuje SD, odczytuje EEPROM, uruchamia TFT oraz VPW. Na końcu włącza watchdog AVR z limitem 4 s. Brak RTC zatrzymuje start; błąd SD tylko generuje komunikat diagnostyczny i pozwala kontynuować.

W `loop()` program obsługuje odbiór i nadawanie VPW, ekrany, przyciski, czujniki, logi, terminal oraz automat modemu. Modem oddaje sterowanie między porcjami danych. Podczas oczekiwania na długą odpowiedź GPS pętla obsługuje VPW RX i modem, odkładając wolniejsze zadania do zakończenia odpowiedzi lub timeoutu 1 s. SD, grafika i DHT nadal mają koszty synchroniczne; poniższe interwały są nominalne.

| Zadanie | Interwał / warunek |
| --- | --- |
| Początek wyświetlania wskaźników | Około 5 s po zapisaniu czasu startu GUI |
| Odświeżanie parametrów TFT 1 | 400 ms |
| Odczyt DHT22 i DS18B20, próba pobrania GPS | 2500 ms; GPS tylko po inicjalizacji i poza wysyłką logu |
| Aktualizacja zegara i sumowanie pomiarów | Impuls SQW 1 Hz |
| Rekord w buforze | Co 2 obsłużone impulsy SQW |
| Zapis i próba wysyłki bufora | 30 rekordów, nominalnie około 60 s |
| Statystyki TX/RX | 20 przedziałów po 3 s, nominalnie ostatnia minuta |
| Obsługa watchdog AVR | Odświeżenie co około 2 s przy limicie 4 s |

Flaga `tick` jest typu `bool`. Jeśli pętla utknie na dłużej niż sekundę, kilka impulsów SQW może zlać się w jeden obsłużony cykl.

## Zapytania VPW

`send()` wybiera najbardziej opóźnione zapytanie z tablicy `cmd[]`. Respektuje minimalny odstęp `pause` oraz 10 ms od ostatniego przetworzonego odbioru. Wywołuje `vpw.send(..., 1)`, ograniczając oczekiwanie na wolną magistralę do 1 ms. Licznik TX i czas ostatniego wysłania danego PID aktualizuje tylko po sukcesie biblioteki. To potwierdzenie nadania bez wykrytej kolizji, nie odpowiedzi ECU.

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

Biblioteka mierzy odstępy między zboczami RX w przerwaniu. Używa bufora 32 ramek i rozmiaru `BS=8` bajtów na ramkę, łącznie z CRC. W odbiorze filtruje pierwszy bajt, dopuszczając `0x6C` i `0x48`; nie jest uniwersalnym rejestratorem wszystkich ramek VPW. `tryGetReceivedFrame()` sprawdza CRC każdej ramki i domyka ramkę także po ciszy na magistrali. Kolejka chroni odczyt krótką sekcją atomową; przepełnienie usuwa najstarszą ramkę i zwiększa licznik. Szkic dodatkowo sprawdza źródło `10`, adresowanie, usługę, PID i długość odpowiedzi.

Nadajnik generuje nominalne impulsy 64 i 128 µs oraz SOF 200 µs. Podczas nadawania odłącza przerwanie RX i sprawdza arbitraż przez odczyt wejścia. Dołączony kod ogranicza się do krótkich ramek i nie obsługuje danych IFR jako osobnego protokołu aplikacyjnego.

## Ekrany i demo

TFT 1 ma widok pełny i uproszczony. Łuk obrotomierza obejmuje zakres 0–6000 obr./min: zielony do 3500, żółty do 4500, dalej czerwony. W cyklu GUI program ogranicza przechowywane RPM do 0–6000 i zaokrągla je do 25 obr./min, co może wpływać również na log.

TFT 2 prezentuje zegar i czujniki w dwóch układach. Podczas pracy menu program wstrzymuje zwykłe rysowanie drugiego ekranu.

Demo nadpisuje wybrane pola wartościami losowymi co cykl GUI. Nie wyłącza odpytywania ECU ani rejestracji, dlatego log może zawierać mieszaninę pomiarów i danych demonstracyjnych.

## Bufor i CSV

Co obsłużony impuls SQW program dodaje bieżące wartości do sum. Po dwóch impulsach zapisuje średnią całkowitą jako `LogEntry`. Brak pomiaru oznaczony `-32768` w sumowaniu zastępuje zerem; nie ma osobnej flagi jakości ani terminu ważności odczytu.

Po zapełnieniu 30 rekordów program otwiera `l-YYMMDD.csv`, np. `l-261027.csv`, a kolejne iteracje dopisują po jednym rekordzie. Jeśli nowy pomiar nadpisze jeszcze niezapisany rekord, program przerywa zapis i zwiększa `sdErrors`. Pierwszy wiersz nowego pliku zawiera nagłówki. Kolejność 18 kolumn:

| Kolumna | Pole | Jednostka / zapis |
| --- | --- | --- |
| 1 | Datetime | `YYYY-MM-DD HH:MM:SS` |
| 2 | Air Temp | °C |
| 3 | Manifold Pressure | Wartość MAP / 100, jedno miejsce po przecinku |
| 4 | TPS | % |
| 5 | Battery | V, wartość / 10 |
| 6 | AFR Bank 2 | Wartość / 100, jedno miejsce po przecinku |
| 7 | Speed | km/h |
| 8 | Transmission Temp | °C |
| 9 | Oil Pressure | **CSV dzieli przez 100, choć ekran dzieli przez 10** |
| 10 | Engine RPM | obr./min |
| 11 | Coolant Temp | °C |
| 12 | AFR Bank 1 | Wartość / 100, jedno miejsce po przecinku |
| 13 | Vent Temp | °C, wartość / 10 |
| 14 | Vent Humidity | %, wartość / 10 |
| 15 | Outside Temp | °C, wartość / 10 |
| 16 | Inside Temp | °C, wartość / 10 |
| 17–18 | Latitude, Longitude | Stopnie dziesiętne, tylko jedno miejsce po przecinku |

Rok i miesiąc pochodzą z chwili zapisu całej paczki; rekord przechowuje tylko dzień i czas. Paczka obejmująca przejście miesiąca może więc otrzymać błędną datę części rekordów. Program zeruje indeks bufora także po nieudanym zapisie SD; nie ma odzyskiwania logów z RAM po restarcie.

## Modem, GPS i HTTP

Inicjalizacja sprawdza kartę SIM, ustawia tryb sieci i APN `internet`, otwiera połączenie oraz uruchamia GPS. Test startowy wykonuje HTTP GET na endpoint logów i oczekuje statusu `403` w `+HTTPACTION: 0,403,<długość>`. Zmiana zachowania serwera może zatrzymać przechodzenie do stanu gotowości.

Po inicjalizacji program kontroluje `AT+CGATT?` i `AT+CREG?`. Dopuszcza rejestrację w sieci macierzystej lub roamingu; po około 20 s braku gotowości uruchamia ponownie sekwencję inicjalizacji. Nie jest to sprzętowe odcięcie zasilania modemu.

GPS odczytuje przez `AT+CGPSINFO`. Parser sprawdza pola współrzędnych, zakresy i kierunki; brak fixa pozostawia ostatnią pozycję. Pozycję zamienia na stopnie dziesiętne, mnoży przez milion i dzieli na dwie połówki 16-bitowe. Dolne słowa przechowuje w `wartosc`, górne w `lastValue`. Logger nadal uśrednia połówki niezależnie, więc przejście przez granicę słowa może zniekształcić współrzędne.

Po zapełnieniu bufora program próbuje wysłać dwa POST-y po 15 rekordów do `http://ls1.cyra.com.pl/logger.php`, z typem `application/octet-stream`. Oczekuje odpowiedzi `+HTTPACTION: 1,200`. Podczas wysyłki wstrzymuje kontrolę sieci i odczyt GPS. Kod serwera nie znajduje się w repozytorium.

### Format pojedynczego POST-a

Poniższy układ wynika z kopiowania struktury w pamięci AVR. Przy przeniesieniu na inną architekturę trzeba sprawdzić wyrównanie i kolejność bajtów.

| Offset | Rozmiar | Pole |
| --- | --- | --- |
| 0 | 12 bajtów | Token, bez końcowego `NUL` |
| 12 | 2 bajty | Rok, little-endian |
| 14 | 1 bajt | Miesiąc |
| 15 | 630 bajtów | 15 rekordów po 42 bajty na AVR |
| 645 | 2 bajty | CRC16, młodszy bajt najpierw |

Razem: 647 bajtów, tyle samo rezerwuje `POSTBIN_SIZE`. CRC obejmuje token, rok, miesiąc i rekordy, używa wielomianu `0x1021`, wartości początkowej `0xFFFF`, bez końcowego XOR.

Rekord zawiera `day`, `hour`, `minute`, `second` jako cztery `uint8_t`, następnie 19 wartości `int16_t`: pola `cmd[0..16]` oraz górne słowa LAT i LON na pozycjach 17 i 18. Wartości binarne zachowują wewnętrzne skale programu, bez dzielników używanych przy zapisie CSV. Pakiet nie zawiera numeru części.

Wysyłka kopiuje pierwszą połowę logu do `postBin` przed ponownym użyciem bufora. Drugą kopiuje dopiero po wysłaniu pierwszej, sprawdzając generację i indeks zapisu. Nadpisanie drugiej połowy przerywa wysyłkę, zamiast mieszać minuty. UART ma jednego właściciela dla inicjalizacji, sieci, GPS i HTTP. Po błędzie HTTP program porzuca paczkę i czeka na 5,5 s ciszy przed ponowną inicjalizacją; spóźnione dane restartują ten okres. Nie ponawia POST w niepewnym stanie modemu. Program nie odsyła później zaległych CSV z karty. HTTP nie szyfruje tokenu ani pomiarów, w tym pozycji GPS.

## EEPROM

| Adres | Dane |
| --- | --- |
| 0 | Flaga czasu letniego |
| 1–12 | 12 znaków tokenu |
| 13 | Zakończenie tokenu `NUL` |
| 14 | Demo |
| 15 | Widok wskaźników |
| 16 | Widok zegara |
| 17 | Pauza VPW w ms |

Na starcie program ocenia token tylko po pierwszym znaku; nie sprawdza całego ciągu. Token pochodzi z `random()`, nie z generatora kryptograficznego. Pierwsze generowanie w `setup()` następuje przed `randomSeed(analogRead(A0))`.

Pusta EEPROM może włączyć demo i oba tryby widoku, ponieważ wartość `0xFF` przechodzi na `true`. Pauza ma osobną kontrolę zakresu, która ustawia 75 ms dla wartości nieprawidłowej.

## Granice tej dokumentacji

Etap 1 pozostawia przeliczniki i format logów. Znane rozbieżności skali oleju oraz uśredniania GPS wymagają osobnej zmiany formatu lub obliczeń. Wyniki kompilacji i zakres testów opisuje [raport etapu 1](ETAP1.md). Nie potwierdzono modelu modemu, schematu VPW ani dokładności rozszerzonych PID dla innych sterowników.
