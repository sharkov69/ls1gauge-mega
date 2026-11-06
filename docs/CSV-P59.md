# CSV i cztery pakiety DPID

Aktualizacja 2026-11-06. Zmiany dotyczą VPW, ekranów i SD. Modem, API, format binarny, bufor 30 rekordów i harmonogram LTE pozostają bez zmian. `CMD_SIZE=17`, rekord LTE ma 42 bajty; nowe kanały nie trafiają do API.

## Pakiety i źródła

| DPID | Pola (adres VPW, długość) | Bajty |
|---|---|---:|
| FE | RPM 000C/2, TPS 0011/1, MAP 000B/1, Speed 000D/1, Oil 115C/1 | 6 |
| FD | AFR1 114B/1, AFR2 114E/1, Battery 1141/1, ECT 0005/1, IAT 000F/1, TFT 1940/1 | 6 |
| FC | Injector B1 125A/2, B2 125B/2, SA 000E/1, PRNDL 1113/1 | 6 |
| FB | TCC 1102/1, Shift Mode 1973/1, A/C 1100/1, Fans 1106/1, Knock Retard 11A6/1, Closed Loop 1105/1 | 6 |

21 surowych pól, 24 bajty. Dwa starty: `2A 14 FE FD FC FB` oraz `2A 24 FE FD FC FB`. Każdy wymaga własnego potwierdzenia z echem i kodem 23. Po potwierdzeniu obu grup odbiornik wymaga wszystkich czterech pakietów. Cel: około 100 ms; rzeczywista częstotliwość wymaga testu w aucie. RX liczy odczytane pola (5+6+4+6), a nie dodatkowe kolumny wyciągane z tych samych bitów.

SA: `raw/2-64` stopnie. Knock Retard: `raw/11.375` stopnie. Injector B1/B2: `raw/65.535` ms. PRNDL: 1=P, 2=R, 4=N, 8=D4, 16=D3, 32=D2, 64=D1, 128=Error. Jest to status selektora, nie osobny kanał biegu aktualnie załączonego.

Kontrolka paliwa: oba poprawne czasy wtrysku <0,3 ms, czyli raw <=19. To wnioskowanie o odcięciu, nie odczyt dedykowanej flagi DFCO. S: Trans Shift Mode 7 lub 8. Kontrolki wymagają danych nie starszych niż 500 ms; w awaryjnym odpytywaniu pojedynczym zwykłe stany mają limit 3 s, bo pełny obieg trwa co najmniej 2,5 s. Kontrolka odcięcia zachowuje ostrzejszy limit 500 ms dla obu banków. Po braku danych kontrolki są szare/ukryte. PRNDL bez danych pokazuje `?`, SA `--`.

## CSV

Pozostaje plik `l-YYMMDD.csv`, średnik i polski przecinek dziesiętny. Pierwsza kolumna: `Datetime`, np. `2026-11-05 14:32:10,200`. SD zapisuje najnowszy stan co 200 ms, synchronizuje co 1 s. To migawki 5 Hz, nie zapis każdej ramki VPW. Wolna karta nie wywołuje serii zaległych kopii.

CSV ma 37 kolumn z czasem. Jego kolejność jest niezależna od niezmienionej kolejności kanałów w LTE. Po `Datetime`: napięcie, RPM, prędkość, TPS, IAT, ECT, TFT, MAP, olej, SA, AFR1, AFR2. Następnie czujniki lokalne i GPS, PRNDL, TCC, tryb skrzyni, czasy wtrysku, pięć pól A/C, Fan1 i Fan2, KR, Closed Loop oraz cztery pozostałe flagi. Dokładny nagłówek i mapowanie znajdują się obok siebie w `sd.h`. Nazwy kanałów ECU odpowiadają katalogowi P59 Scanner, a `AFR 1` / `AFR 2` jego P59.Maths.xml.

Jednostki nie wynikają z nazwy kolumny. Dla późniejszego importera należy użyć tej tabeli, a nie domyślnych jednostek amerykańskich z katalogu:

| Kolumna / grupa | Jednostka lub kodowanie |
|---|---|
| Intake Air Temp (SAE), Engine Coolant Temp (SAE), Trans Fluid Temp | °C |
| Intake Manifold Absolute Pressure (SAE), Engine Oil Pressure | bar |
| Throttle Position (SAE), Vent Humidity(%) | % |
| Control Module Voltage | V |
| AFR 1, AFR 2 | AFR; dotychczasowa średnia z 4 odczytów |
| Vehicle Speed (SAE) | km/h |
| Engine RPM (SAE) | rpm |
| Vent Temp, Outside Temp, Inside Temp | °C |
| Latitude, Longitude | stopnie dziesiętne, 6 miejsc |
| Timing Advance (SAE), Knock Retard | stopnie; 1 i 2 miejsca dziesiętne |
| Injector Pulse Width Avg. Bank 1 / Bank 2 | ms, 3 miejsca |
| PRNDL Status, Trans Shift Mode | surowy kod wyliczenia z katalogu |
| Pozostałe nowe kolumny | logiczne 0/1, po zastosowaniu inwersji katalogowej |

Wszystkie kolumny CSV zapisują ostatnią odebraną wartość bez limitu wieku. Pole pozostaje puste tylko do pierwszego odczytu od uruchomienia; prawidłowe zero zapisuje się jako zero. Dotyczy to również czujników lokalnych i GPS. Restart zeruje tę pamięć. Reguła CSV nie zmienia limitów świeżości kontrolek ani danych LTE. Poprawiono zapis Engine Oil Pressure: wartość wewnętrzna oznacza dziesiąte części bara, więc 33 zapisujemy jako `3,3`, nie `0,33`. Nie zmienia to bufora ani API LTE.

Wspólne bajty i bity (numery od 0):

- 1100: AC Requested 0, AC Active 1, AC Clutch Engaged 3, AC Disable (Pressure) 4, AC Disable (WOT) 6.
- 1106: Fan 1 0, Fan 2 1.
- 1102: Reverse Inhibited 4 (odwrócony), TCC Applied 7.
- 1105: Closed Loop Active 0, O2 B1S1 Ready 1, O2 B2S1 Ready 3, Fuel Trim Learn 5.

Closed Loop Active to nazwa stanu dostępnego w katalogu, odpowiadającego żądanemu wskaźnikowi pracy w closed loop. Nie zmienia nastaw ECU.

Jeżeli dzisiejszy plik ma stary nagłówek, program zachowuje go jako `.bak`. Jeśli taka kopia już istnieje, wybiera `.b01` … `.b99`, bez nadpisywania. Następnie tworzy nowy `.csv`. Po zajęciu wszystkich nazw zgłasza błąd SD zamiast usuwać dane. Dotychczasowa data modyfikacji FAT nadal pochodzi z RTC.

Zgodne nazwy ułatwiają przyszły import, ale aktualizacja nie dodaje importera tego formatu do P59 Scanner. Ten CSV nadal wymaga obsługi separatora, czasu, jednostek i kodów wyliczeń po stronie importera.

Źródła: [katalog](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/web/src/catalog/p59-1281057.json), [DPID](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/docs/reference/MPVI4-DPID.md), [warunek odcięcia](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/web/src/gaugeIndicators.ts), [nazwy AFR](https://github.com/sharkov69/codex-p59-scanner/blob/master/p59-session-fix/src/Scanner.Backend/defaults/P59.Maths.xml).
