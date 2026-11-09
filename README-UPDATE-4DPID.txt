LS1Gauge Mega — aktualizacja 4 DPID, 2026-11-05

1. Zamknij Arduino IDE i zachowaj kopię obecnego szkicu.
2. Skopiuj zawartość folderu ls1gauge-mega z ZIP-a do swojego folderu
   szkicu ls1gauge-mega. Nadpisz istniejące pliki i dodaj dwa nowe nagłówki:
   extra_channels.h i gauge_indicators.h. ZIP nie zawiera całego projektu.
3. Biblioteka J1850-VPW-WC pozostaje w wersji z poprzedniej aktualizacji.
4. Otwórz ls1gauge-mega.ino, wybierz Arduino Mega 2560 i wgraj szkic.
5. Nowy układ dotyczy pełnego widoku pierwszego ekranu i zegara z datą
   na drugim. Pozostałe tryby nadal są dostępne dotychczasowymi przyciskami.

LTE: kod modemu, binarny format API, lista wysyłanych danych, częstotliwość,
bufor 30 rekordów oraz ustawienia DEBUG_MODEM/EEPROM pozostają bez zmian.
Nowe kanały trafiają na wyświetlacze i do CSV, nie do bazy przez LTE.

SD: nadal l-YYMMDD.csv, migawka co 200 ms. Nowy plik ma 45 kolumn.
Pierwsze 17 kanałów zachowuje kolejność; nazwy ECU dostosowano do P59.
Nowe kolumny są na końcu. Pełna lista źródeł, jednostki i dodatkowe bity:
docs/CSV-P59.md. Import tego formatu do skanera wymaga jeszcze obsługi po
stronie importera; zgodność nazw nie zastępuje informacji o jednostkach.

Jeśli na karcie jest dzisiejszy plik ze starym nagłówkiem, zostanie zachowany
jako .bak (lub .b01 ... .b99, gdy kopia już istnieje), a nowy CSV powstanie
pod dotychczasową nazwą. Program nie nadpisuje poprzednich logów.

Podgląd: otwórz preview/ls1gauge-preview.html w Firefoxie. Działa offline,
ma oba ekrany i przełączniki. Nie komunikuje się z Arduino ani z API.

Sprawdzone lokalnie: kompilacja Mega (90 710 B flash; 5579 B RAM,
2613 B wolne na stos/zmienne lokalne; wzrost RAM o 116 B względem poprzedniej
wersji), testy VPW/DPID, CSV i kopii plików, niezmienionego LTE oraz debug USB.
Nie wykonano testu na fizycznym Arduino/P59. Po wgraniu sprawdź przy aucie:
pełne dane obu ekranów, ikony, przyrost RX, nowy CSV i dotychczasową wysyłkę LTE.
Rzeczywista częstotliwość ramek i opóźnienia karty SD zależą od sprzętu.
