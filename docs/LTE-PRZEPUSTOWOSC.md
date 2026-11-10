# Zaległości wysyłki LTE i zmiana bufora — 10 listopada 2026

## Objaw i diagnoza

Po nocnym teście API było około trzech godzin za zapisem CSV. Arduino nadal odbierało VPW i zapisywało SD, a działająca kontrolka modemu nie oznaczała, że wysyłka nadąża. O 06:29 najnowsza próbka w API pochodziła z około 03:25.

Przed zmianą pomiar bazy wykazał około **667 B/s**. Przy około 139 B na wiersz i nominalnych pięciu wierszach/s logger produkowałby około **696 B/s**. Było za mało zapasu na timeouty i ponowne uruchamianie sesji HTTP. Faktyczny przyrost SD zależy od czasu wykonania pętli; nominalne 5 Hz nie jest gwarantowane.

Każdy POST przenosił do 1024 B CSV, ale wymagał osobnych HTTPDATA, HTTPACTION i HTTPREAD. Po serii 16 żądań klient zamykał sesję i ponownie wykonywał konfigurację HTTP/TLS. Zmierzone oczekiwanie od HTTPACTION do odczytu odpowiedzi wynosiło około sekundy, niezależnie od tego, że wysyłaliśmy tylko 1 kB.

| Mediana odcinka w diagnostyce | ECM odłączony | ECM podłączony |
| --- | ---: | ---: |
| DATA → ACTION | 225 ms | 226 ms |
| ACTION → READ | 1006,5 ms | 1017 ms |
| READ → następne DATA | 130 ms | 131,5 ms |

Porównanie nie wykazało, by priorytet VPW był głównym ograniczeniem cyklu HTTP. Dziesięciominutowy test bez ECM był dodatkowo zakłócony restartem Arduino przy USB i wznowieniem modemu, dlatego nie traktujemy jego całkowitego transferu jako czystego pomiaru wpływu ECM.

Odczyt radia pokazał LTE Band 1 i RSRP około −115,4 dBm. Nie wykonano niezależnego pomiaru przepustowości radia, więc słabego sygnału nie uznano za udowodnioną przyczynę zaległości. Potwierdzone koszty obejmowały małe POST-y i przerwy związane z brakiem odpowiedzi modemu.

## Co zmieniliśmy

| Parametr | Przed zmianą | Po zmianie |
| --- | ---: | ---: |
| Bufor danych CSV w RAM | 1024 B | **2048 B** |
| Maksymalna porcja CSV w jednym POST | 1024 B | **4096 B** |
| Nagłówek protokołu P59C | 58 B | 58 B |
| Maksymalna treść POST, bez nagłówków HTTP/TLS | 1082 B | 4154 B |
| Odpowiedź potwierdzająca P59A | 24 B | 24 B |

„Pakiet 4 kB” oznacza tutaj porcję CSV w żądaniu HTTP, nie fizyczny pakiet sieci LTE. Dwa bloki po 2 kB należą do **jednego POST-u**, a nie do dwóch osobnych żądań.

Arduino nie trzyma całych 4 kB w RAM. Czyta pierwsze 2 kB ze zsynchronizowanego pliku SD, przekazuje je do UART w ograniczonych porcjach, a następnie wykorzystuje ten sam bufor do odczytu i wysłania drugiej części. Mniejszy końcowy fragment nie musi mieć pełnych 4 kB. Nagłówek pozostaje osobno i nie jest nadpisywany podczas ponownego użycia bufora.

Granica odczytu pochodzi z rozmiaru po udanym sync() i jest ustalana przed POST-em. Dopisywanie nowych wierszy nie zmienia długości trwającego żądania. Błąd odczytu drugiej części przerywa próbę, zamiast wysyłać stare dane. Offset przesuwa się dopiero po poprawnym potwierdzeniu API. Po utracie ACK klient pyta serwer o zatwierdzoną pozycję.

Nie zmieniono formatu CSV, nagłówka P59C/P59A ani przeliczników PID. Backend już obsługiwał porcje 4096 B.

## Koszt RAM

Sama zmiana transferu dodała **1026 B statycznego SRAM**: 1024 B dodatkowego bufora i 2 B pozycji aktualnego bloku. Pierwsza kompilacja tej wersji zajmowała 5841 B SRAM i 106456 B Flash.

Późniejsze zmiany dodały kolejkę enkodera, licznik restartów modemu, nasłuch DPID i diagnostykę MAP. Aktualna wersja po porządkowaniu zajmuje **5930 B statycznego SRAM**, pozostawiając 2262 B na stos i dane lokalne. Nie należy porównywać tej sumy z pierwotnym firmware tak, jakby cały przyrost pochodził wyłącznie z bufora LTE.

W teście pierwszej wersji 4 kB konsola raportowała stabilne 2001 B wolnego RAM. Jest to pomiar podczas pracy, a nie gwarantowany najgorszy przypadek użycia stosu.

## Zmierzone wyniki

W 90-sekundowym odcinku stabilnej pracy po zmianie:

| Wielkość | Przyrost | Średnie tempo |
| --- | ---: | ---: |
| Zapis SD | 43 250 B | **480,6 B/s** |
| Dane potwierdzone przez API | 196 608 B | **2184,5 B/s** |

W tym odcinku LTE przesyłało około **4,55 razy więcej bajtów niż przybywało na SD**, zmniejszając kolejkę o około 1704 B/s. Nie jest to gwarantowana wydajność przez cały dzień. Porównanie do porannego pomiaru 667 B/s daje około 3,3-krotny wzrost, ale pomiary wykonano w różnym czasie i przy różnych przerwach modemu.

| Kontrola 10 listopada, czas Warszawy | Opóźnienie najnowszej próbki API |
| --- | ---: |
| 08:02 | około 2 godz. 1 min; wybrany plik SD miał około 3,84 MB zaległości |
| 08:41 | 1 godz. 4 min |
| 09:17 | 39,5 s |
| 18:15, przed otwarciem USB | 13 s |

Sprawdzono też starszy plik o długości 2 040 766 B: dotarł w całości. Wcześniejsza pozorna luka przed północą wynikała z niedosłanej końcówki starszego pliku. Sam najnowszy czas w API nie wystarcza do potwierdzenia przesłania wszystkich plików.

Podczas późniejszej próby nasłuchu o 22:21–22:24 opóźnienie zmieniło się 21 → 65 → 34 s: wysyłka chwilowo zwolniła, następnie nadrabiała. Wszystkie 29 kolumn ECM były wypełnione w sprawdzonych 742 wierszach; puste pozostały kolumny lokalnych czujników i GPS. Odebrane fragmenty miały zgodne SHA256, ciągłe offsety i prawidłowe 36 kolumn CSV.

## Wpływ na SD i wyświetlacze

Dłuższy POST zajmuje UART dłużej. Przy 115200 baud, 8N1 same 4154 B wymagają co najmniej około 361 ms. Szybka ścieżka obsługi modemu nadal odbiera VPW, ale odkłada wolniejsze zadania SD i rysowania. W intensywnym transferze zmierzono około **4 wierszy CSV/s zamiast nominalnych 5**; w późniejszym nasłuchu około 4,7/s. Nie zapisujemy każdej odebranej próbki VPW: CSV zapisuje bieżący zestaw wartości.

Osobna poprawka ustawiła ekran 1 na 200 ms i przechwytuje ruchy enkodera także podczas szybkiej obsługi LTE. Zdarzenia czekają w kolejce 16 pozycji, a rysowanie wykonuje się później. Użytkownik potwierdził, że ruchy nie giną. Kolejka nie gwarantuje natychmiastowego odrysowania podczas wysyłania treści POST.

## Testy i pozostałe ograniczenia

Testy hosta sprawdzają granice 2047/2048/2049 i 4095/4096/4097 B, zgodność całej treści żądania, mały bufor UART, błąd odczytu drugiego bloku, utratę ACK i wznowienie od offsetu API. Cały zestaw 12 testów regresji przeszedł. Test sprzętowy potwierdził dalszy odbiór DPID, zerowy licznik błędów SD w obserwowanym odcinku i rosnące potwierdzone offsety API.

Zmiana **nie usuwa przyczyny okresowego braku odpowiedzi modemu**. Automatyczne recovery i D12 potrafią wznowić transfer, ale takie przerwy nadal mogą chwilowo zwiększać zaległość. Nie wykazano pełnej zgodności całej karty z API ani zachowania wszystkich pojedynczych próbek VPW. Logi nie są automatycznie kasowane z SD po wysłaniu.

Kod: [csv_upload.h](../ls1gauge-mega/csv_upload.h), [modem-csv.h](../ls1gauge-mega/modem-csv.h), [odczyt SD](../ls1gauge-mega/csv_upload_sd.h). Opis konfiguracji i protokołu: [CSV_UPLOAD.md](CSV_UPLOAD.md).
