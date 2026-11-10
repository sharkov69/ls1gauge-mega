# Działanie i format danych

[Powrót do README](../README.md)

Aktualizacja 2026-11-08: jedynym transportem jest CSV przez HTTPS. Konfigurację SD, PWRKEY i NTP opisuje [CSV_UPLOAD.md](CSV_UPLOAD.md). EEPROM 64–95 przechowuje trwały licznik plików.

## Start i główna pętla

W `setup()` program uruchamia porty szeregowe, rozpoczyna inicjalizację modemu, konfiguruje czujniki i RTC, inicjalizuje SD, odczytuje EEPROM, uruchamia TFT oraz VPW. Na końcu włącza watchdog AVR z limitem 4 s. Brak RTC zatrzymuje start; błąd SD tylko generuje komunikat diagnostyczny i pozwala kontynuować.

W `loop()` program obsługuje odbiór i nadawanie VPW, ekrany, przyciski, czujniki, logi, terminal oraz automat modemu. Modem oddaje sterowanie między porcjami danych. Podczas oczekiwania na długą odpowiedź GPS pętla obsługuje VPW RX i modem, odkładając wolniejsze zadania do zakończenia odpowiedzi lub timeoutu 1 s. SD, grafika i DHT nadal mają koszty synchroniczne; poniższe interwały są nominalne.

| Zadanie | Interwał / warunek |
| --- | --- |
| Początek wyświetlania wskaźników | Około 5 s po zapisaniu czasu startu GUI |
| Odświeżanie parametrów TFT 1 | 200 ms |
| Odczyt DHT22 i DS18B20, próba pobrania GPS | 2500 ms; GPS tylko po inicjalizacji i poza wysyłką logu |
| Aktualizacja zegara i sumowanie pomiarów | Impuls SQW 1 Hz |
| Wysyłka LTE/HTTPS | Z kolejki SD, do 4096 B CSV na żądanie; seria do 16 żądań, potem zwolnienie UART |
| Czas z internetu | Po połączeniu LTE i co 6 godzin; ponowienie błędu po 5 minutach, korekta RTC tylko powyżej 30 s |
| Zapis szybkiego CSV na SD | Cel: jeden zestaw co 200 ms; szybka ścieżka LTE może opóźnić zapis |
| Statystyka RX | 20 przedziałów po 3 s, nominalnie ostatnia minuta |
| Obsługa watchdog AVR | Odświeżenie co około 2 s przy limicie 4 s |

Flaga `tick` jest typu `bool`. Jeśli pętla utknie na dłużej niż sekundę, kilka impulsów SQW może zlać się w jeden obsłużony cykl.

## Zapytania VPW

W trybie TX: ON program konfiguruje 21 PID-ów w czterech pakietach DPID; powtórzenie pakietów w dwóch grupach daje cel około 100 ms na pakiet. TX: OFF korzysta z konfiguracji drugiego adaptera, bez własnych zapytań. Poniższy harmonogram pojedynczych zapytań obowiązuje przed konfiguracją, po powrocie do legacy lub po wyłączeniu DPID. Przeliczniki obu trybów są wspólne.

`send()` wybiera najbardziej opóźnione zapytanie z katalogu `DPID_FIELDS`. Respektuje minimalny odstęp `pause` oraz 10 ms od ostatniego przetworzonego odbioru. Wywołuje `vpw.send(..., 1)`, ograniczając oczekiwanie na wolną magistralę do 1 ms. Znacznik ostatniej próby TX aktualizuje po wywołaniu biblioteki; harmonogram konkretnego PID-u przesuwa tylko po sukcesie. Pomiar TX usunięto. Sposób zliczania kanałów w trybie pakietowym opisuje [DPID.md](DPID.md). To potwierdzenie nadania bez wykrytej kolizji, nie odpowiedzi ECU.

Zapytania trybu `01` używają nagłówka `68 6A F1`; rozszerzone zapytania trybu `22` nagłówka `6C 10 F1` i dodatkowego bajtu `01` po PID. Biblioteka dopisuje CRC.

| Kanał | PID | Interwał fallback |
| --- | --- | --- |
| `RPM` | `0x000c` | 250 ms |
| `TPS` | `0x0011` | 250 ms |
| `MAP` | `0x000b` | 250 ms |
| `SPEED` | `0x000d` | 250 ms |
| `OIL` | `0x115c` | 5000 ms |
| `O2` | `0x114b` | 250 ms |
| `O22` | `0x114e` | 250 ms |
| `BATT` | `0x1141` | 5000 ms |
| `WATER` | `0x0005` | 5000 ms |
| `AIR` | `0x000f` | 5000 ms |
| `TRANS` | `0x1940` | 5000 ms |
| `INJ1` | `0x125a` | 1000 ms |
| `INJ2` | `0x125b` | 1000 ms |
| `SA` | `0x000e` | 250 ms |
| `PRNDL` | `0x1113` | 1000 ms |
| `TCC` | `0x1900` | 1000 ms |
| `SHIFT_MODE` | `0x1973` | 1000 ms |
| `AC_STATE` | `0x1100` | 2000 ms |
| `FAN_STATE` | `0x1106` | 2000 ms |
| `KR` | `0x11a6` | 250 ms |
| `CL_STATE` | `0x1105` | 2000 ms |

Lokalne temperatury, wilgotność i GPS nie są PID-ami ECM ani pozycjami licznika 21 kanałów.

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

Poza menu obrót enkodera w lewo usypia oba ekrany, a w prawo je budzi. Powtórzenie ruchu w tę samą stronę nie przełącza stanu. Naciśnięcie otwiera menu i budzi ekrany. Odstęp ochronny między zaakceptowanymi naciśnięciami wynosi 150 ms, bez blokowania pętli.

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
| 20 | Nasłuch VPW (1 = TX OFF, pozostałe = TX ON) |
| 64–95 | Trwały licznik plików CSV |


Demo uruchamia się z EEPROM tylko dla zapisanej wartości `1`; pusta komórka `0xFF` go nie włącza. Oba tryby widoku nadal odczytują wartość jako `bool`. Pauza ma osobną kontrolę zakresu, która ustawia 75 ms dla wartości nieprawidłowej; usunięcie jej pozycji z menu nie zmienia harmonogramu VPW.

## Granice tej dokumentacji

Testy na stole nie zastępują próby z ECM w samochodzie. Nasłuch zmienia źródło mapowania VPW; skale znanych PID-ów pozostają wspólne. Dokładność rozszerzonych PID dla innych sterowników wymaga osobnego potwierdzenia.

Aktualizacja 2026-11-10: ekran 1 odświeża zmienione pola co 200 ms. Ruchy enkodera są odczytywane również w szybkiej ścieżce LTE i trafiają do kolejki 16 zdarzeń. Obsługa menu i rysowanie odbywają się poza tą ścieżką. Kolejka dodaje 20 B statycznego RAM; nie gwarantuje natychmiastowego odrysowania podczas wysyłania treści POST.


### Licznik restartów modemu (10 listopada 2026)

Na dole drugiego ekranu, pod wilgotnością, mała szara ikona fal radiowych i liczba pokazują liczbę uruchomień procedury PWRKEY przez D12 od startu Arduino. Restart Arduino zeruje licznik. Ponowienie HTTP nie zwiększa licznika; samoczynnych restartów modemu i zaniku zasilania nie wykrywamy. Licznik zatrzymuje się na 65535, zajmuje 2 B RAM i nie zapisuje EEPROM ani SD. W menu i podczas logo wskaźnik pozostaje ukryty; kolor uwzględnia Dark.

Dwukropki In:, Out: i Vent: mają wspólną kolumnę. Jednostka V przy BAT na pierwszym ekranie ma połowę poprzedniego rozmiaru. Zegar i data zachowują dwukolumnowy układ, nie są dynamicznie centrowane razem z każdym dniem tygodnia.

### Weryfikacja wersji z licznikiem

10 listopada 2026: 12 zestawów testów hosta zakończonych powodzeniem. Testy obejmują zwiększanie licznika wyłącznie przy D12, brak zwiększenia przy recovery HTTP, nasycenie na 65535, wyświetlenie wartości 0/9/10/65535 i ukrycie na menu. Kompilacja Mega: 107010 B Flash, 5863 B statycznego RAM (+2 B), 2329 B pozostaje na stos i zmienne lokalne. Firmware wgrano na COM14. W odczycie sprzętowym DPID działał, zapis SD miał 0 błędów, offset potwierdzony przez API rósł, a wolny RAM wynosił 1979 B. Nie wymuszano awarii modemu; wygląd fizycznego ekranu wymaga oceny użytkownika. Otwarcie konsoli powoduje w tym zestawie restart Arduino.

### Nasłuch DPID bez nadawania

Menu `TX: OFF` włącza nasłuch; `ON` przywraca dotychczasowy tryb aktywny. Ustawienie zapisuje się w EEPROM pod adresem 20 (tylko wartość 1 oznacza nasłuch). Przełączenie czyści mapę DPID, ale zachowuje ostatnie wartości kanałów. Nasłuch blokuje konfigurację, zapytania, keepalive i fallback Arduino; drugi adapter musi utrzymywać transmisję ECM.

Na górze drugiego ekranu widnieje np. `19/21`: liczba naszych PID-ów z potwierdzoną definicją i startem strumienia. To stan mapowania, nie licznik świeżych odpowiedzi. Temperatury lokalne i GPS nie należą do tych 21 PID-ów. Po utracie transmisji obowiązują istniejące zasady świeżości i zachowania ostatnich wartości CSV.

Arduino podsłuchuje fizyczne polecenia P59 2C, potwierdzenia 6C i start 2A z potwierdzeniem 7F/23. Obsługuje grupy x4 używane przez P59 Scanner, do czterech identyfikatorów na polecenie startu i sześć bajtów danych na pakiet. Śledzi adres jednego adaptera. Przy zmianie adaptera, stopie lub przełączeniu trybu czyści mapę. Nadpisanie slotu unieważnia dotychczasowe znaczenie; definicja staje się dostępna dopiero po zgodnym ACK. Potwierdzenia starsze niż 2 s są ignorowane. Definicje PID muszą mieć zgodną długość i maskę FFFF; odczyty adresów RAM i inne formaty nie są mapowane na kanały aplikacji.

Należy włączyć nasłuch przed rozpoczęciem konfiguracji drugiego adaptera. Jeśli konfiguracja już się odbyła, trzeba ją powtórzyć na drugim adapterze. Nie zapisujemy mapy do EEPROM. Nieznane PID-y nie zwiększają RAM. Tryb aktywny, kolumny CSV, przeliczenia, SD i LTE zachowują dotychczasowe działanie. Brak odbioru Battery nadal podlega dotychczasowej bramce zapisu, więc brak tego PID-u w planie adaptera może zatrzymać nowe wiersze SD.

Testy hosta obejmują wszystkie 21 kanałów rozłożone na 21 pakietów, różne grupy startu, brakujące, błędne i spóźnione ACK, nadpisanie nieznanym PID-em, wiele pól w jednym pakiecie, blokadę TX, powrót do trybu aktywnego oraz menu/EEPROM. Test na rzeczywistym drugim adapterze wymaga ponownego uruchomienia jego konfiguracji DPID; nie zastępują go testy hosta.

Kolejność menu: Show pictures, TX: ON/OFF, Dark: ON/OFF, Gauge Mode, Clock Mode, Demo: ON/OFF, Restart. Zmiana kolejności nie zmienia adresów EEPROM.


### Wyniki nasłuchu na stanowisku — 10 listopada 2026

Użytkownik potwierdził 21/21 kanałów oraz około 6280 próbek/min w nasłuchu (około 5 Hz/kanał) i 12610 próbek/min po powrocie do aktywnego DPID (około 10 Hz/kanał). Pierwsza konfiguracja aktywna przeszła przez fallback; ponowienie po 30 s uruchomiło DPID.

Odczyt API bez otwierania USB wykazał wszystkie 29 kolumn ECM w 742/742 wierszach, około 4,7 wiersza/s, medianę odstępu 200 ms i pojedynczy odstęp 1,214 s. Puste były tylko cztery kolumny lokalnych czujników i dwie GPS. SHA256 fragmentów, ciągłość offsetów i 36 kolumn CSV były poprawne. Opóźnienie LTE zmieniało się 21 → 65 → 34 s; transfer po spowolnieniu nadrabiał. Nie porównano całej zawartości karty z bazą i nie potwierdzono świeżości każdej wartości: CSV zachowuje ostatni odczyt.

Aktualna kompilacja po uporządkowaniu menu, z diagnostyką MAP: 108784 B Flash, 5930 B statycznego RAM (2262 B na stos i dane lokalne); przyrost względem wersji bez nasłuchu wynosi 67 B. Komenda USB `MAP` wypisuje 21 wierszy z PID, DPID, offsetem od zera, długością i flagą ready. Raport wysyła najwyżej jeden wiersz na wywołanie konsoli, tylko gdy jest miejsce w buforze UART. Otwarcie portu USB w tym zestawie resetuje Arduino i usuwa mapę: konsolę należy otworzyć przed konfiguracją drugiego adaptera.

Nasłuch nie pobiera obecnie kanałów z osobnych odpowiedzi na zapytania PID. Jeśli drugi adapter umieści Battery poza DPID, nie będzie aktualizował bramki zapisu. Zweryfikowany plan 21/21 zawiera wszystkie wymagane PID-y w DPID.
