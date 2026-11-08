# Test PWRKEY, NTP i wysyłki CSV — 2026-11-08

Sprzęt: Arduino Mega 2560, SIM7600E-L1C, firmware LE20CB03SIM7600M11_OTA.
D12 prowadzi do PWRKEY; użytkownik potwierdził usunięcie R104.
ECM i czujniki temperatury nie były podłączone podczas próby na stole.

## Zmiany

- D12 używa OUTPUT LOW / INPUT bez podciągania. Nie podaje 5 V na PWRKEY.
- Powtarzający się brak odpowiedzi modemu uruchamia nieblokującą sekwencję
  wyłączenia, sprawdzenia AT i ewentualnego włączenia. Pierwszy cykl nie zaczyna
  się przed 30 s od startu Arduino; następne nie częściej niż co 120 s.
- CNTP i CCLK korzystają z tego samego właściciela UART co pozostałe operacje.
  RTC przechowuje czas Warszawy i jest korygowany tylko dla różnicy >30 s.
- Usunięto stary transport, jego endpoint, token, bufor 30 rekordów i przełącznik
  trybów. DEBUG_MODEM i jego sterowanie przez USB/EEPROM pozostają.
- Usunięto zakomentowany błędny wariant read32. Obecny format CSV, VPW i rysowanie
  nie zmieniły się podczas tej poprawki.

## Testy komputerowe

12 zestawów `tests/run-host-tests.ps1`: wszystkie przeszły. Obejmują parser AT,
brak miejsca w UART/USB, DEBUG_MODEM i EEPROM, PWRKEY i ograniczenie powtórzeń,
NTP (sukces/błąd/cisza), dalszy transfer CSV po błędzie NTP, różnicę RTC
-31/-30/0/30/31 s, błędne daty, strefy oraz granice czasu letniego.
Pozostałe testy sprawdzają VPW/DPID/fallback, SD, kolejkę, ACK i interfejs.
`tests/provision-tests.ps1` również przeszedł na sztucznej konfiguracji.

Kompilacja Mega: 103076 B Flash, 4800 B statycznego SRAM (+19 B względem wersji
przed PWRKEY/NTP), 3392 B pozostaje na stos i dane lokalne.

## Próba na urządzeniu

Wgrano firmware przez COM14. Modem początkowo nie odpowiadał na AT.
Automatyczny cykl rozpoczął się przy uptime 36524 ms; pojawiły się RDY,
CPIN READY i odpowiedzi AT. Inicjalizacja zakończyła się przy 91813 ms,
czyli około 55,3 s po rozpoczęciu cyklu.

CNTP zwrócił `+CNTP: 0`. Odczyt CCLK dał UTC 1794174288; wpis `RK` potwierdził,
że RTC mieścił się w tolerancji, więc nie przestawiono zegara. Sam próg korekty
sprawdzono testami hosta, bez celowego fałszowania daty logów na SD.

Heartbeat przychodził co 5 s podczas PWRKEY, NTP i transferów; wskazywał 3180 B
wolnego RAM. Użytkownik potwierdził działanie zegara, przycisków i menu.
W końcowej dwuminutowej obserwacji SD wzrosło z 30 do 534 wierszy, `errors=0`;
odczyty CSV pokazywały `cfg=1 tls=1 err=0` i rosnący offset.
Nie zarejestrowano dodatkowego resetu Arduino podczas tych prób.

Stan API przed próbą: 1281024 B / 1251 fragmentów. Weryfikacja po próbie:
1565696 B / 1529 fragmentów. Sprawdzenie potwierdziło ciągłość offsetów,
SHA-256 fragmentów i 37 kolumn we wszystkich kompletnych wierszach.
Nie jest to porównanie całej karty SD z serwerem.

Końcowe usunięcie wyłączonego kodu starego transportu nie zmieniło pliku HEX:
SHA-256 `F2A35F5F9EA9E9821E477A7DDC61566E300B79F1F4072A54B0AEEB7C96AF1E26`.
Ten obraz został wcześniej wgrany i poddany końcowej obserwacji.

## Ograniczenia

PWRKEY nie zastępuje RESET ani odcięcia zasilania, jeśli moduł przestanie reagować
także na ten pin. Brak przewodu STATUS oznacza brak sprzętowego potwierdzenia
wyłączenia. Próba na stole nie potwierdza zachowania pod obciążeniem VPW w aucie.
Wcześniejszy pojedynczy watchdog na etapie zapisu SD pozostaje osobnym problemem;
brak resetu w tej próbie nie dowodzi usunięcia jego przyczyny.
