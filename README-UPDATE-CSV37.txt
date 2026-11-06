CSV37 — aktualizacja 2026-11-06

Podmień ls1gauge-mega/sd.h i ls1gauge-mega/cmd.h na wersji a085d7f,
a następnie skompiluj i wgraj program do Mega. W cmd.h zmienił się tylko komentarz.
Pozostałe pliki paczki to dokumentacja i testy.

CSV ma 37 kolumn. Usunięto dotychczasowe numery 34,35,36,37,38,42,44,45.
Po dacie: napięcie, RPM, prędkość, TPS, IAT, ECT, TFT, MAP, olej, SA, AFR1, AFR2.
Fan2 jest po Fan1, a pięć pól A/C tworzy wspólną grupę.

Każde pole zachowuje ostatnią odebraną wartość aż do następnego pomiaru.
Puste pole oznacza brak jakiegokolwiek odczytu od uruchomienia, także dla
starych kanałów, czujników lokalnych i GPS. Prawidłowe zero nie jest brakiem danych.
Przy restarcie historia wartości zaczyna się od nowa.

Nie zmieniono pakietów DPID, 21 PID, harmonogramu fallback, ekranów ani LTE.
Usunięte flagi nie są już interpretowane przez CSV. Ich wspólne bajty nadal
są odczytywane, ponieważ zawierają też pozostawione flagi.
Zmiana CSV nie zmienia limitów świeżości kontrolek na ekranach.

Plik o starym nagłówku zostanie zachowany jako .bak lub .b01–.b99;
program utworzy CSV z nowym nagłówkiem. Nie miesza dwóch formatów w jednym pliku.

Testy: siedem zestawów hosta bez błędów; LTE, ekrany i dekodowanie DPID
zgodne z wcześniejszą wersją. Testy SD obejmują dokładną kolejność 37 kolumn,
zera, brak pierwszego pomiaru, ostatnie wartości po 10 s oraz GPS i raw=32768.
Kompilacja Mega: Flash 90262 B; RAM 5525 B, bez wzrostu (2667 B pozostaje).
Nie wykonano testu na fizycznym Arduino.

Zaktualizowany wzorzec do importera jest w osobnym ZIP-ie symulacji v2.
