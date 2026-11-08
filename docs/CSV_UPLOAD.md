# CSV z SD przez SIM7600E-L1C

Stan 2026-11-08: CSV/HTTPS jest jedynym transportem. Testy na Mega z SIM7600E-L1C potwierdziły wznowienie przez PWRKEY, synchronizację NTP i dalszy transfer CSV. Backend działa pod https://camaro.it-tiger.pl; import w P59 Scanner pozostaje osobnym etapem.

Brak P59.CFG lub P59.TLS blokuje wysyłkę, ale nie zapis CSV. Usunięto stary transport binarny, token i przełącznik trybów.

## Działanie

Arduino skanuje logi na SD, odczytuje pozycję zatwierdzoną przez serwer i wysyła porcje do 1024 B. Odczytuje tylko bajty po udanym `sync()`. Po utracie ACK pyta o pozycję ponownie. Sesja HTTP obsługuje do 16 żądań, potem oddaje modem pozostałym zadaniom. Buforowanie UART i binarny ACK nie blokują pętli oczekiwaniem sieciowym. Nie ma przejścia na nieszyfrowany HTTP.

RPM=0 przez ponad 10 s zatrzymuje zapis; pierwszy odczyt RPM>1 wznawia go. W tym czasie wysyłanie zaległości trwa, dopóki jest zasilanie. Nie gwarantujemy przesłania całego dnia w 10 minut: trzeba zmierzyć przepustowość SIM7600 i wpływ na VPW.

ID pliku składa się z 8-bajtowej przestrzeni instalacji oraz licznika. Licznik zajmuje EEPROM 64–95 (dwie kopie z CRC). Metadane SD `.u00`–`.u64` zachowują ID po restarcie i zmianie nazwy archiwum. Nie kasuj ich podczas zwykłego kopiowania logów. Skrócenie pliku nadaje nowe ID. Ręczna podmiana CSV na inny o tej samej lub większej długości wymaga usunięcia jego metadanych, aby nadać nowe ID. Po wyzerowaniu EEPROM wygeneruj nowy P59.CFG z nową przestrzenią instalacji. Zachowaj kopię poprzedniej karty.

Uszkodzone metadane blokują dany plik; napraw je po zabezpieczeniu kopii. Przerwany pierwszy zapis metadanych może wymagać ich usunięcia. Błąd zapisu SD blokuje wysyłkę do restartu po naprawie karty. Nie usuwamy CSV po odebraniu potwierdzenia. Plik nieczytelny lub z pozycją serwera poza lokalnym końcem jest pomijany po zamknięciu HTTP i sprawdzany ponownie przy kolejnym skanowaniu.

## Przygotowanie karty

1. Na VM utwórz urządzenie: w `/opt/p59-log-server` uruchom `sudo docker compose exec api python -m receiver provision camaro-mega`. Zachowaj wyświetlony token urządzenia (64 znaki hex) w bezpiecznym miejscu. To nie jest token Cloudflare ani token administratora API.
2. Na Windows, z katalogu projektu, uruchom `./tools/New-P59SdConfig.ps1 -CardPath E:\` (podstaw literę karty). Wklej token do ukrytego pola. Skrypt zapisze P59.CFG: 32 bajty sekretu + 8 losowych bajtów przestrzeni instalacji. Nie nadpisuje istniejącego pliku.
3. Na razie nie twórz P59.TLS. Przechowuj kartę jak nośnik z hasłem; plik nie jest szyfrowany na SD. Nie dołączaj go do ZIP ani Git.

## Test TLS po podłączeniu USB

### Konfiguracja SD przez Arduino USB

Nie trzeba wyjmować karty. Konsola 115200 obsługuje `CFG NN HHHHHHHHHHHHHHHH`:
`NN` to dziesiętny offset 00, 08, 16, 24 albo 32, a 16 małych znaków hex koduje
osiem bajtów konfiguracji. Pierwsze 32 bajty to sekret urządzenia, ostatnie osiem
to losowa przestrzeń instalacji. Po każdej porcji wymagane jest `CFG OK`.
`CFG COMMIT` sprawdza 40 bajtów i zatwierdza plik tymczasowy jako P59.CFG.
Istniejącego P59.CFG nie można nadpisać tymi komendami. Nie zapisuj sekretu
w transkrypcji terminala ani w repozytorium.

Po wykonaniu poniższych testów TLS operator może wysłać `TLS PASS` i oczekiwać
`TLS OK`. Ta komenda zapisuje marker kwalifikacji; sama nie przeprowadza testów.
`CSV` pokazuje stan konfiguracji, kwalifikacji, transferu, błędów i offsetu,
a `MEM` raportuje wolną pamięć. Polecenia nie wyłączają debugowania.

Firmware LE20CB03SIM7600M11_OTA odpowiada nagłówkiem `+HTTPREAD: DATA,24`.
Parser obsługuje go oraz starszy format `+HTTPREAD: 24`, bez nowego bufora RAM.

Testuj bez aktywnego Arduino na tym samym interfejsie AT. Zapisz model, odpowiedzi `ATI`, `AT+CGMR`, stan SIM `AT+CPIN?`, rejestrację `AT+CEREG?`, sygnał `AT+CSQ`, czas `AT+CCLK?`. Zegar musi odpowiadać rzeczywistemu czasowi i strefie modemu. Zegar lokalny wyświetlacza nie jest automatycznie źródłem czasu TLS. Zweryfikuj APN `internet`; jeśli operator wymaga innego, zmień CGDCONT w modem.h.

1. Ustal bieżący łańcuch certyfikatu publicznego `camaro.it-tiger.pl`. Za proxy CF modem widzi certyfikat Cloudflare, nie certyfikat Let's Encrypt na VM. Pobierz właściwy certyfikat CA z oficjalnego repozytorium jego wystawcy, sprawdź fingerprint i ważność. Nie używaj certyfikatu końcowego jako trwałego CA.
2. Wgraj plik PEM do modemu jako `p59-ca.pem` poleceniem `AT+CCERTDOWN="p59-ca.pem",N`, gdzie N to dokładna długość pliku w bajtach. Po znaku gotowości prześlij dokładne bajty, bez dodatkowego CR/LF. Użyj terminala obsługującego wysłanie pliku binarnego; potwierdź OK i obecność przez `AT+CCERTLIST`.
3. Uruchom HTTPINIT i konfigurację poniżej. Każda pozycja musi zwrócić OK; HTTPINIT może trwać do 120 s.

```text
AT+HTTPINIT
AT+HTTPPARA="CID",1
AT+CSSLCFG="sslversion",0,3
AT+CSSLCFG="authmode",0,1
AT+CSSLCFG="ignorelocaltime",0,0
AT+CSSLCFG="enableSNI",0,1
AT+CSSLCFG="cacert",0,"p59-ca.pem"
AT+HTTPPARA="SSLCFG",0
AT+HTTPPARA="CONNECTTO",20
AT+HTTPPARA="RECVTO",10
AT+CSSLCFG="negotiatetime",0,20
AT+HTTPPARA="URL","https://camaro.it-tiger.pl/healthz"
AT+HTTPACTION=0
```

Oczekuj `+HTTPACTION: 0,200,...`, odczytaj ciało przez HTTPREAD; `/healthz` ma odpowiedzieć `{"status":"ready"}`. Zakończ HTTPTERM. Nie wysyłaj sekretu urządzenia podczas testów negatywnych.

4. Na kontrolowanych endpointach TLS powtórz GET przy: niezaufanym CA, wygasłym certyfikacie oraz prawidłowym łańcuchu CA z certyfikatem wystawionym dla innej nazwy hosta. Dla ostatniego testu oba hosty muszą korzystać z tego samego zaufanego CA, aby wykluczyć odmowę wyłącznie z powodu CA. Wszystkie trzy próby muszą zakończyć się błędem TLS, bez obsługi żądania HTTP przez serwer. Samo SNI i authmode=1 nie dowodzą sprawdzania nazwy hosta. Endpointy testowe przygotujemy przed sesją ze sprzętem.
5. Jeśli modem ignoruje nazwę hosta, datę albo zwraca ERROR dla wymaganych poleceń, pozostaw wysyłkę wyłączoną. Potrzebna będzie aktualizacja firmware zgodna z SIM7600E-L1C lub osobny zweryfikowany transport. Nie stosuj authmode=0 ani ignorelocaltime=1.
6. Dopiero po przejściu testów na docelowym firmware uruchom `./tools/New-P59SdConfig.ps1 -CardPath E:\ -TlsTestsPassed`. Powstanie marker P59.TLS (TLS1 + 8 bajtów przestrzeni instalacji). To zapis wyniku ręcznej kwalifikacji, nie automatyczny test. Po zmianie firmware lub konfiguracji zaufania usuń marker i powtórz testy.

## Test całego toru

- Wgraj bieżący firmware; rozpocznij od krótkiego logu. Pobierz plik z API i porównaj bajty lub SHA-256 z zamkniętym plikiem SD. API administracyjne: GET /v1/files oraz GET /v1/files/{device_id}/{file_id}, nagłówek Authorization: Bearer z tokenem administratora. Nie zapisuj tokenu w URL.
- Przerwij sieć przed ACK i uruchom ponownie zasilanie. Backend ma zachować jeden ciąg bajtów, bez duplikatów i luk. Po błędzie Arduino odczekuje 5,5 s ciszy na UART, chroniąc okno wejściowe `HTTPDATA`, następnie wymaga `+CPAS: 0` i `OK`. Zamyka poprzednią sesję przez `HTTPTERM` i potwierdza nową przez `HTTPINIT`, zanim wróci do inicjalizacji i wysyłki. Samo spóźnione `OK` nie wystarcza do potwierdzenia gotowości. Nie ma stałej przerwy 130 s; timeout poleceń HTTP wynosi do 120 s, ale poprawna odpowiedź kończy oczekiwanie wcześniej. Brak odpowiedzi powoduje ponowienie sprawdzenia po okresie ciszy, bez blokowania pętli.
- Sprawdź zaległe pliki, zmianę dnia, archiwizację oraz rosnący dzienny CSV. Nie wyjmuj karty podczas normalnej pracy; próby awarii zasilania rób na kopii karty testowej.
- RPM=0 przez 11 s: plik przestaje rosnąć, zaległości nadal się wysyłają. RPM>1: zapis wraca bez resetu urządzenia. Sprawdź również krótki postój 2–5 min.
- Zmierz ilość wysłanych bajtów przez 10 min, przerwy VPW, płynność wyświetlacza, GPS i wolny stos. Statyczny raport kompilatora nie mierzy stosu.

## Wyniki lokalne

Wszystkie 11 zestawów testów C++ przeszło, w tym odcięcia zapisu licznika, błędne metadane, granice sync, provisioning, ramki 1024 B, binarny ACK, timeouty i spóźnione URC. Porównanie wyjść starego LTE, ekranów i kanałów z 87f73bf: identyczne bajty.

| Tryb Mega | Flash | Statyczny SRAM |
|---|---:|---:|
| 1 CSV | 98 980 B | 4903 B |
| 2 stare API | 90 614 B | 5533 B |
| 0 bez wysyłania | 88 494 B | 3524 B |

Powyższa tabela dotyczy wcześniejszej kompilacji. Test sprzętowy 2026-11-08 potwierdził TLS oraz odbiór 138240 B CSV przez API. Wystąpiły jednak zawieszenia: stabilność i odzyskiwanie po awarii pozostają niepotwierdzone. Szczegóły w `outputs/arduino-sd-api-test-20261108.txt`.

## Diagnostyka zawieszeń

Przy `DEBUG_MODEM=1` znaczniki zajmują dodatkowo 14 B SRAM. Nie zapisują EEPROM ani SD.
`BOOT raw=0 valid=1 wdt=1 stage=21 owner=5 phase=2` oznacza, że poprzednio przerwanie
watchdoga zastało aplikację w etapie 21. `valid=0` oznacza brak wiarygodnego śladu.
`raw` to MCUSR dostępny aplikacji; standardowy bootloader Mega zeruje ten rejestr,
więc zero nie rozstrzyga przyczyny resetu. `wdt=0` nie wyklucza resetu watchdogiem,
jeżeli przerwania były wyłączone. Po utracie zasilania ślad w `.noinit` może zniknąć.

Watchdog po około 4 s zapisuje etap w przerwaniu. Nie drukuje ani nie wykonuje I/O.
Następnie ustawia sprzętowy reset po około 15 ms i pozostaje w ISR do resetu,
aby inne funkcje nie nakarmiły watchdoga ani nie nadpisały etapu awarii.
Ta krótka pętla występuje tylko po timeoutcie; normalna diagnostyka nie czeka.
W setup watchdog pozostaje wyłączony
jak wcześniej; komunikaty `BS` pokazują postęp inicjalizacji.

| Etap | Miejsce |
|---|---|
| 1–9 | Start: 2 czujniki, 3 RTC, 4 SD, 5 BMP, 6 konfiguracja CSV, 7 TFT, 8 VPW, 9 koniec setup |
| 10–14 | Początek pętli/zasilanie ekranów, odbiór VPW, szybka obsługa modemu, wysłanie VPW, rysowanie |
| 15–20 | Menu, BMP, czujniki, odczyt RTC, zegar/ekran, drugi odbiór VPW |
| 21 | Zapis SD, w tym callback daty korzystający z RTC |
| 22–26 | Kolejka SD, harmonogram CSV, konsola, modem, koniec pętli |
| 40–48 | CSV: idle, setup, prepare/odczyt SD, data, body, body OK, action, read, term |

`owner` i `phase` to właściciel UART i faza transakcji AT z momentu timeoutu.
Kod wyświetlania i VPW pozostaje bez zmian; znaczniki otaczają ich wywołania.

Zapis od samego otwarcia portu, bez oczekiwania na odpowiedź konsoli:

```powershell
.\tools\capture-modem.ps1 -Port COM14 -CaptureOnly -Seconds 180 -OutputFile .\outputs\freeze.log
```

Plik musi mieć nową nazwę. `-CaptureOnly` nie wysyła poleceń i nie zmienia flagi DEBUG.
DEBUG musi być wcześniej włączony. Bez tej opcji pozostaje dotychczasowe menu 1/2/3.
Zakończenie zapisu nie wyłącza DEBUG. Otwarcie portu może resetować Mega; podczas
próby trzymaj jedno połączenie otwarte, nie uruchamiaj równocześnie drugiej konsoli.
Po zawieszeniu odczekaj co najmniej 10 s, by zapisać ewentualny restart watchdogiem.

## Protokół v1 i dokumentacja modemu

### Wznawianie przez D12 i czas RTC

Płytka BK-SIM7600: D12 jest połączony z PWRKEY, R104 został usunięty.
Program używa wyłącznie OUTPUT LOW oraz INPUT bez podciągania. Nie podaje 5 V na PWRKEY.
Po starcie przez 30 s nie wykonuje cyklu zasilania. Pięć nieudanych prób AT podczas
inicjalizacji lub trzy kolejne próby bez odpowiedzi w recovery uruchamiają cykl,
nie częściej niż raz na 120 s. Odpowiedź o braku rejestracji w sieci nie uruchamia cyklu.

Cykl: LOW 3 s, zwolnienie 30 s, próba AT. Jeśli modem odpowie, program przechodzi
do inicjalizacji; inaczej podaje LOW 500 ms, zwalnia pin i daje 15 s na start.
Oczekiwania korzystają z millis(), bez delay() i dodatkowych buforów. Zapis SD,
VPW i ekrany nadal działają. Kolejka CSV zachowuje pliki i po wznowieniu pyta API
o zatwierdzony offset. Bez podłączonego STATUS nie ma sprzętowego potwierdzenia
wyłączenia; jeśli zawieszony moduł ignoruje PWRKEY, potrzebny jest RESET lub odcięcie zasilania.

Po uzyskaniu połączenia program synchronizuje modem z pool.ntp.org przez CNTP,
czeka na wynik, następnie odczytuje CCLK. Czas przelicza na Europe/Warsaw
z uwzględnieniem ostatniej niedzieli marca/października o 01:00 UTC.
RTC zmienia wyłącznie przy bezwzględnej różnicy większej niż 30 s.
Udany odczyt powtarza co 6 godzin, nieudany co 5 minut. NTP ma limit 30 s;
niepowodzenie zwalnia UART dla LTE i nie zatrzymuje zapisu SD.
Synchronizacja zaczyna się wyłącznie między transferami CSV.

Debug: `PW 0/1/3/4` oznacza etapy PWRKEY, `RA` korektę RTC,
`RK` czas zgodny z tolerancją, `RE` nieudaną synchronizację.

- [SIMCom Hardware Design V1.09](https://mm.digikey.com/Volume0/opasdata/d220001/medias/docus/4998/SIM7600_Series_Hardware_Design.pdf)
- [Instrukcja producenta BK-SIM7600](https://pdfcoffee.com/2018-07-24-122343-bk-sim7600-board-user-manual-v10-pdf-free.html)
- [SIMCom AT V3.00: CNTP i CCLK](https://files.waveshare.com/wiki/SIM7600G-H/SIM7500_SIM7600_Series_AT_Command_Manual_V3.00.pdf)

POST /v1/chunks: `P59C` (4 B), sekret (32 B), ID (16 B), offset uint32 LE, długość uint16 LE, dokładne bajty CSV. Arduino: do 1024 B danych, backend: do 4096 B. Offset 0xffffffff i długość 0 pytają o postęp. HTTP200 musi zawierać dokładnie 24 B: `P59A`, ID, zatwierdzony offset uint32 LE. Potwierdzenie następuje po transakcji bazy.

Parser obsługuje udokumentowane HTTPREAD: początkowe OK, +HTTPREAD:24, 24 bajty, +HTTPREAD:0; wariant z końcowym OK dopuszcza tylko bez początkowego OK. Bufor z tokenem nie trafia do debugowania.

- [SIMCom HTTP AT V1.00](https://simcom.ee/documents/SIM7X00/SIM7500_SIM7600_SIM7800%20Series_HTTP_AT%20Command%20Manual_V1.00.pdf)
- [SIMCom SSL AT V1.00](https://simcom.ee/documents/SIM7X00/SIM7500_SIM7600_SIM7800%20Series_SSL_AT%20Command%20Manual_V1.00.pdf)
