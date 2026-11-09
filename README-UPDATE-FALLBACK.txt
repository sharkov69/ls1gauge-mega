Aktualizacja harmonogramu awaryjnego VPW — 2026-11-05

Na wersji d34e287 wystarczy podmienić ls1gauge-mega/cmd.h i ponownie wgrać
program do Arduino Mega. Pozostałe pliki w ZIP to testy i ta instrukcja.

send() obsługuje wszystkie 21 PID tym samym algorytmem co wcześniej:
wybiera najbardziej zaległy odczyt (czas od wysłania minus interwał PID).
Usunięto osobną kolejkę dodatkowych PID, która ignorowała te interwały.

Interwały odczytu awaryjnego:
250 ms: RPM, TPS, MAP, SPEED, O2, O22, SA, KR.
1000 ms: INJ1, INJ2, PRNDL, TCC, SHIFT_MODE.
2000 ms: AC_STATE, FAN_STATE, CL_STATE.
5000 ms: OIL, BATT, WATER, AIR, TRANS.

Zachowano przerwę między zapytaniami, ochronę po odbiorze i aktualizowanie
czasu odczytu dopiero po udanej transmisji. Interwały określają moment
gotowości zapytania; obciążenie magistrali i przerwy mogą opóźnić wysłanie.
Zmiana dotyczy wyłącznie odczytu awaryjnego. Pakiety i działanie DPID,
wyświetlacze, format CSV oraz wysyłka LTE pozostają bez zmian.

Weryfikacja:
- 7 zestawów testów hosta: bez błędów.
- Test wszystkich 21 interwałów, priorytetu zaległych odczytów, błędu TX,
  przerw ochronnych i przepełnienia licznika czasu: bez błędów.
- Porównanie CSV, LTE, poleceń rysowania i dekodowania DPID z wersją
  87f73bf: identyczne wyniki dla tych samych danych wejściowych.
- Kompilacja Mega: Flash 90478 B; zmienne globalne RAM 5525 B (+15 B),
  pozostałe 2667 B na stos i dane robocze.

Nie przeprowadzono testu na fizycznym Arduino ani ECM. Wynik kompilacji
nie jest pomiarem maksymalnego wykorzystania stosu podczas pracy.
