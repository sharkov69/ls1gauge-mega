# Biblioteki i uruchomienie

[Powrót do README](../README.md)

## Zależności

Wersje w tabeli potwierdzono kompilacją etapu 1 dla `arduino:avr:mega`, z pakietem Arduino AVR Boards 1.8.8. Repozytorium nie zawiera pliku blokującego wersje zależności.

| Biblioteka | Zaobserwowana wersja | Zastosowanie |
| --- | --- | --- |
| `J1850-VPW-WC` | Brak numeru wersji | Własna biblioteka z katalogu `libraries/J1850-VPW-WC` |
| Adafruit GC9A01A | 1.1.1 | Sterowanie TFT |
| Adafruit GFX Library | 1.12.6 | Grafika i tekst |
| Adafruit BusIO | 1.17.4 | Zależność bibliotek Adafruit |
| OneWire | 2.3.8 | Magistrala DS18B20 |
| DallasTemperature | 4.0.6 | Pomiary DS18B20 |
| DHT sensor library | 1.4.7 | DHT22 |
| Adafruit Unified Sensor | 1.1.15 | Zależność biblioteki DHT |
| RTClib | 2.1.4 | DS3231 |
| SdFat | 2.3.0 | Dostęp do SD; lokalnie obecna też wersja Adafruit Fork 2.3.103 |
| SPI, Wire, EEPROM, nagłówki AVR | Z pakietu płytki | Magistrale, pamięć ustawień i watchdog |

Zainstaluj zależności z menedżera bibliotek Arduino IDE. Bibliotekę VPW skopiuj z repozytorium do katalogu `libraries/J1850-VPW-WC` swojego szkicownika Arduino, tak aby pliki `.h` i `.cpp` leżały bezpośrednio w tym katalogu. Nie zastępuj jej inną biblioteką J1850 o podobnej nazwie: szkic używa nagłówka `j1850vpw-wc.h` i zmian tej wersji.

W przypadku SdFat włącz szczegółowy wynik kompilacji i sprawdź, z którego katalogu IDE pobiera `SdFat.h`. Kod używa `SdFat`, `File`, `SPI_FULL_SPEED`, `openNext()` i `getName()`. W lokalnym środowisku są dwa warianty SdFat; kompilacja etapu 1 użyła SdFat 2.3.0. Nie instaluj obu wariantów w celu rozwiązania konfliktu.

## Przygotowanie i wgranie

1. Pobierz repozytorium i otwórz `ls1gauge-mega/ls1gauge-mega.ino`. Zachowaj pliki nagłówkowe w tym samym folderze co szkic.
2. Zainstaluj pakiet **Arduino AVR Boards** i wybierz **Arduino Mega or Mega 2560**, procesor **ATmega2560** oraz port swojej płytki.
3. Zainstaluj biblioteki z tabeli i lokalną bibliotekę VPW.
4. Porównaj sprzęt z [tabelą połączeń](PODLACZENIA.md). Ustaw adresy DS18B20 w `temp.h`, APN w `modem.h` i adres serwera `LOGGER_URL` w `modem.h`.
5. Uruchom weryfikację szkicu w IDE. Zapisz wynik kompilacji wraz z wersją pakietu AVR i wybranymi ścieżkami bibliotek, jeśli chcesz odtworzyć środowisko na innym komputerze.
6. Wgraj szkic. Ustaw czas RTC przez terminal i sprawdź ustawienia demo, widoku oraz pauzy VPW.
7. Sprawdź zegar, odczyty czujników i ekrany. Następnie sprawdź komunikację z pojazdem przez właściwy interfejs VPW, zapis pliku CSV i odpowiedź serwera.

Etap 1 przeszedł kompilację AVR i testy hosta. Nie wgrywano go na urządzenie. Procedurę próby i wyniki pamięci zawiera [ETAP1.md](ETAP1.md).

## Konfiguracja

| Ustawienie | Miejsce | Wartość w kodzie |
| --- | --- | --- |
| Adres DS18B20 wewnętrznego | `temp.h`, `tempIn` | `28 75 F6 BC 00 00 00 D4` |
| Adres DS18B20 zewnętrznego | `temp.h`, `tempOut` | `28 F5 FB 33 00 00 00 91` |
| Typ DHT | `temp.h`, `DHTTYPE` | `DHT22` |
| Interwał czujników | `temp.h`, `SENSORS_INTERVAL` | 2500 ms; komentarz o 5 s jest nieaktualny |
| APN | `modem.h`, `initStep()` | `internet` |
| Wybór trybu sieci modemu | `modem.h`, `initStep()` | `AT+CNMP=13`; znaczenie sprawdź dla własnego modemu |
| Endpoint logów | `modem.h`, `LOGGER_URL` | `http://ls1.cyra.com.pl/logger.php` |
| Odstęp zapytań VPW | EEPROM / menu | 25–125 ms; 75 ms po wykryciu wartości spoza zakresu |
| Logo | Początek `.ino` | `LOGO` włączone |
| Diagnostyka | Początek `.ino` | `DEBUG`, `TXRX`, `DEBUGVPW` wyłączone |

### Czas RTC

Otwórz monitor portu szeregowego z prędkością **115200 bit/s** i wyślij linię zakończoną LF lub CR:

```text
SETTIME=2026-10-27 12:00:00
```

Zastąp datę i godzinę czasem lokalnym, który chcesz wyświetlać. Parser wymaga układu `SETTIME=YYYY-MM-DD HH:MM:SS`; sprawdza zakresy oraz poprawność daty przez RTClib. Program zapisuje również flagę czasu letniego. Korektę lato/zima wykonuje przy starcie według daty, bez uwzględnienia godziny przejścia. Synchronizacja zegara z GPS oraz funkcja pobierania czasu z HTTP są zakomentowane.

### Karta SD i obrazy

Logi trafiają do katalogu głównego karty. Program nie formatuje karty; przygotuj ją w systemie plików obsługiwanym przez wybraną konfigurację SdFat, np. FAT32.

Opcja `Show pictures` wyszukuje do 12 plików `.bmp` w katalogu głównym. Przygotuj nieskompresowane obrazy BMP 24-bit, z nagłówkiem DIB 40 bajtów, dodatnią wysokością i wymiarami do 240 × 240. Użyj krótkich nazw 8.3, np. `foto01.bmp`; bufor nazwy ma tylko 14 bajtów wraz ze ścieżką i zakończeniem. Kod sprawdza wymiary przed konwersją do `uint8_t`; odrzuca większe obrazy i przerywa odczyt uciętego pliku.

## Przyciski i menu

| Przycisk | Poza menu | W menu | W regulacji `VPW pause` |
| --- | --- | --- | --- |
| 1 / D27 | Usypia lub budzi oba ekrany | Poprzednia pozycja | +10 ms |
| 2 / D29 | Otwiera menu i budzi ekrany | Wykonuje wybraną opcję | Powrót do menu |
| 3 / D31 | Przełącza demo i zapisuje wybór | Następna pozycja | −10 ms |

Menu wyświetla się na TFT 2. Program stosuje 300 ms odstępu między reakcjami i zamyka menu po 5 s bezczynności.

| Pozycja | Działanie |
| --- | --- |
| `Show pictures` | Kolejny obraz z SD; powrót do wskaźników po około 5 s |
| `Gauge View` | Zmiana układu pierwszego ekranu |
| `Clock View` | Zmiana układu zegara |
| Tekst aktualnego tokenu | Wygenerowanie i zapis nowego tokenu |
| `VPW pause` | Regulacja minimalnego odstępu zapytań |
| `Restart` | Wywołanie funkcji pod adresem 0; nie odcina zasilania modułów |

**Demo nie jest trybem izolowanym.** Program dalej wysyła zapytania VPW, a wygenerowane wartości mogą trafić do SD i na serwer. Przed rejestracją rzeczywistej jazdy sprawdź ustawienie demo. Uśpienie ekranów także nie zatrzymuje rejestracji.

## Diagnostyka

Włącz `DEBUG`, aby zobaczyć komunikaty na `Serial`. `TXRX` dodatkowo włącza wypisywanie ramek, lecz korzysta z makr `DEBUG`. Dla `DEBUGVPW` definicja musi być spójna w szkicu i oddzielnie kompilowanej bibliotece, np. przez włączenie jej w nagłówku `j1850vpw-wc.h` i szkicu. Sama definicja w `.ino` nie obejmuje pliku `.cpp` biblioteki.

| Objaw | Co sprawdzić |
| --- | --- |
| Brak startu ekranów | Komunikację I2C z RTC: przy błędzie `rtc.begin()` program zatrzymuje się przed inicjalizacją TFT |
| Zegar stoi, brak logów | SQW → D2, podciąganie i impulsy 1 Hz |
| Brak danych VPW | Interfejs elektryczny, RX D3/TX D5, zgodność PID ze sterownikiem i ustawienie demo |
| Błędne temperatury DS18B20 | D47, adresy ROM, połączenie OneWire i podciąganie |
| Brak CSV | CS D53, wspólne SPI, kartę i wybrany wariant SdFat; pierwszy zapis następuje nominalnie po około minucie |
| Modem powtarza inicjalizację | Zasilanie, UART, SIM, APN, obsługę komend AT; test startowy oczekuje HTTP 403 |
| Brak wysyłki mimo danych na SD | Zakończenie inicjalizacji modemu, endpoint i odpowiedź HTTP 200 na POST |
| Błędna skala oleju w CSV | Rozbieżność dzielnika opisana w [formacie danych](DZIALANIE.md) |
