Aktualizacja: wspólne kanały i porządki w kodzie, 2026-11-05

1. Zrób kopię swojego folderu szkicu.
2. Nadpisz pliki z katalogu ls1gauge-mega w tej paczce.
3. Usuń z folderu szkicu poniższe stare pliki (ich kod jest teraz w cmd.h/display.h):
   extra_channels.h
   vpw_reply.h
   vpw_dpid.h
   gauge_indicators.h
   Usuń również sscom.ini, jeśli nadal tam jest. To ustawienia terminala, nie firmware.
4. Skompiluj ls1gauge-mega.ino dla Arduino Mega 2560 i wgraj jak dotychczas.

Paczka zawiera tylko dodane/zmienione pliki względem commita 87f73bf.
Nie zastępuje całego projektu. Biblioteka J1850-VPW-WC pozostaje bez zmian.
Nie trzeba zmieniać EEPROM, konfiguracji API ani plików na SD.

27 kanałów ma wspólną strukturę Channel. Usunięto cztery dodatkowe nagłówki.
Wskaźniki, przeliczenia, interwały, CSV (45 kolumn) i format LTE (42 B/rekord,
30 rekordów, po 647 B na połowę transmisji) zachowują poprzednie działanie.
SPARK nazwano OIL, ponieważ ten indeks przechowuje ciśnienie oleju.

Kompilacja Mega: RAM 5510 B, wolne 2682 B; Flash 90688 B.
To o 69 B mniej RAM niż przed porządkami. Zapytania pojedyncze i cztery DPID
korzystają z jednego katalogu we Flash; nie ma już tablic cmd_air/cmd_rpm itd.
To wynik statyczny, nie pomiar maksymalnego zużycia stosu na urządzeniu.
Testy hosta i porównanie z poprzednią wersją nie zastępują próby z ECU, SD i modemem.
