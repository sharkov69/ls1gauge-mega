# Etap 1: VPW, pętla i pamięć

Zmiany dotyczą Mega 2560 i tego samego P59, który obsługiwał bazowy wsad. Zachowano piny, PID-y, przeliczniki, format CSV i binarny POST. Nie włączono DPID ani szybszego trybu magistrali. Na prośbę autora średnia ruchoma AFR obu sond szerokopasmowych obejmuje teraz 4 zamiast 10 próbek. Pierwsza próbka nadal wypełnia cały filtr, a wartości spoza 1029–1911 zastępuje 1475.

## Pamięć

Pomiar: Arduino AVR Boards 1.8.8, `arduino:avr:mega`, AVR GCC 7.3.0, biblioteki z [URUCHOMIENIE.md](URUCHOMIENIE.md), diagnostyka DEBUG wyłączona. Baza: commit `c89592a`.

| Zasób | Baza | Etap 1 | Zmiana |
| --- | ---: | ---: | ---: |
| Flash | 78 642 B | 79 290 B | +648 B |
| Statyczny SRAM | 6532 B | 5176 B | −1356 B |
| SRAM poza danymi statycznymi | 1660 B | 3016 B | +1356 B |

Ostatni wiersz to miejsce na stos i stertę, a nie zmierzony minimalny zapas podczas pracy. Stały bufor BMP 723 B zastąpił bufor 48 B na stosie. Opisy pól TFT i komendy AT trafiły do Flash. Usunięto dynamiczne `String` z modemu, terminala i formatowania czasu/obrotów. Mniejsze filtry AFR oszczędzają dodatkowo 24 B.

Największe pozostałe bufory to log 1260 B, paczka HTTP 647 B i obiekt SD około 630 B. Nie dodano drugiego bufora całego logu ani większych buforów UART. Przy zmianie bibliotek lub definicji DEBUG wynik może się zmienić.

## Obsługa w pętli

- VPW: domknięcie krótkiej ramki po ciszy, kontrola granic 8 bajtów, osobne sprawdzanie CRC każdej ramki, atomowe pobieranie z kolejki i licznik utraconych ramek. Nieudana próba nadawania na zajętej magistrali zachowuje odbierany pakiet.
- Szkic odbiera ramki przed nadawaniem i ponownie pod koniec pętli. Usunięto puste oczekiwanie 5 ms. Nadawanie zwraca wynik, a oczekiwanie na wolną magistralę trwa najwyżej około 1 ms; sama transmisja nadal zajmuje kilka ms.
- Jeden automat zarządza UART modemu. Przetwarza maksymalnie 24 bajty na wywołanie, sprawdza miejsce w TX i nie używa `flush()` ani pętli oczekującej na odpowiedź.
- Długa odpowiedź GPS nie mieści się w domyślnym buforze RX Mega. Podczas jej odbioru pętla daje pierwszeństwo modemowi i VPW RX, odkładając VPW TX, UI, czujniki, terminal i SD. Trwa to do odpowiedzi lub timeoutu 1 s. Nie zwiększa to SRAM, lecz może opóźnić pozostałe zadania.
- HTTP korzysta z niezmiennej kopii jednej połowy logu. Przed skopiowaniem drugiej sprawdza, czy nowa akwizycja jej nie nadpisała. Błąd HTTP kończy paczkę bez ponawiania niepewnego POST. Program opróżnia UART i czeka na 5,5 s ciszy przed inicjalizacją, aby nie wysłać komendy AT jako danych w oknie `HTTPDATA=...,5000`.
- Zapis SD obejmuje jeden rekord na iterację, a rysowanie BMP najwyżej 16 pikseli. Usypianie i budzenie TFT używa zegara zamiast `delay()`.

Cisza 5,5 s zabezpiecza okno danych HTTP, ale nie stanowi dowodu zakończenia dowolnie opóźnionej operacji sieciowej. Inicjalizacja wykonuje `HTTPTERM`; pełna gwarancja rozdzielenia bardzo spóźnionych odpowiedzi wymaga potwierdzenia zachowania konkretnego modemu lub resetu sprzętowego.

## Pozostałe ograniczenia

DHT22 nadal blokuje przerwania podczas części pomiaru i może powodować utratę zboczy VPW. OneWire, operacje SD i rysowanie TFT pozostają synchroniczne. Podział pracy skraca pojedyncze zadania, ale nie zapewnia maksymalnego czasu zapisu karty. Flaga SQW jest nadal pojedynczym `bool`.

Odstęp `pause` pozostaje bez zmian. Dla 75 ms teoretyczna granica to około 13,3 zapytania/s, a nominalne interwały tabeli PID wymagają około 17,2/s. Etap 1 nie daje więc gwarancji osiągnięcia wszystkich częstotliwości. Wymaga to osobnego strojenia harmonogramu lub DPID po pomiarach.

## Weryfikacja

Trzy programy testowe hosta przechodzą: VPW, modem i SD/BMP. Wykonują kod projektu z atrapami GPIO, zegara, UART, SD i TFT. Obejmują uszkodzone CRC przed poprawną ramką, ramki 7/8 bajtów, przepełnienie kolejki, granice nadawania, zachowanie RX przy BUS_BUSY, walidację odpowiedzi, przepełnienie czasu, pełny UART TX, brak odpowiedzi, długie linie, inicjalizację, GPS, obie paczki 647 B z CRC, nadpisanie logu, spóźnione DOWNLOAD oraz skończony bufor RX z transmisją w czasie. Test SD sprawdza pojedynczy zapis, nadpisanie i błąd I/O; BMP sprawdza części wiersza, padding, wymiary i ucięty plik.

Uruchomienie na Windows z MSVC:

```powershell
./tests/run-host-tests.ps1
# Gdy Visual Studio leży gdzie indziej:
./tests/run-host-tests.ps1 -VsDevCmd 'C:/sciezka/VC/Auxiliary/Build/vcvars64.bat'
```

Kompilacja firmware z Arduino CLI i zainstalowanymi zależnościami:

```powershell
arduino-cli compile --fqbn arduino:avr:mega --library ./libraries/J1850-VPW-WC --warnings all ./ls1gauge-mega
```

Wynik AVR: sukces; pozostają ostrzeżenia o nieużywanym parametrze `h` w `images.h` i porównaniu typów w `rtc.h`. Testy hosta nie mierzą czasów GPIO AVR ani stosu na urządzeniu. Nie wykonano wgrywania ani próby z P59.

## Próba na urządzeniu

1. Zachowaj działający wsad i ustawienia. Wyłącz demo. Zacznij od dotychczasowej pauzy VPW.
2. W terminalu 115200 wyślij `MEM` z zakończeniem linii. Odpowiedź zawiera `ram`, `min`, `loop` w µs, `crc` i `drop`. `min` to minimum próbek wolnego RAM na wejściu do pętli, nie pomiar najgłębszego stosu. `loop` obejmuje najdłuższą zmierzoną pełną lub priorytetową iterację; wcześniejsze powroty menu nie trafiają do tego maksimum. Przy pełnym TX terminala odpowiedź może zostać pominięta.
3. Sprawdź oba AFR po zmianie średniej: szybsza reakcja, ten sam przelicznik. Porównaj RX/TX, CRC i przepełnienia kolejki przy odświeżaniu ekranów, GPS oraz podczas zapisu i wysyłki logu.
4. Sprawdź brak zasięgu/GPS, odłączenie odpowiedzi modemu, brak SD i niepoprawny BMP. Zweryfikuj dwa POST-y po 647 B oraz kompletność CSV na serwerze/karcie.
5. Zmierz analizatorem czasy impulsów VPW i opóźnienia pętli, szczególnie podczas DHT i SD. Dopiero te pomiary uzasadnią dalsze skracanie `pause`.
