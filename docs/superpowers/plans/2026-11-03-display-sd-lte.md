# Szybszy ekran, SD i LTE

Dyspozycje autora: usunąć pomiar i wyświetlanie TX; zwiększyć częstotliwość TFT1 oraz SD; zachować zgodność LTE z API i wysyłać około dwa razy częściej.

Decyzje: TFT1 co 100 ms (wcześniej 400), szybki CSV co 200 ms, LTE 30 rekordów co 1 s zamiast co 2 s. Nadal dwa POST-y po 15 rekordów/647 B. SD zapisuje osobny plik f-YYMMDD.csv z dodatkową kolumną czasu monotonicznego w ms, nie korzysta z bufora HTTP. Bez dodatkowej pełnej kopii 30 rekordów. Dotychczasowe pliki l- pozostają historyczne.

- [x] Usunąć licznik TX i pole z obu widoków, zachować RX.
- [x] Zapis SD pojedynczych aktualnych próbek co 200 ms, sync co 1 s, ponowienie po błędzie nie częściej niż co 5 s. Bez nadganiania opóźnień kopiami jednej próbki.
- [x] LTE: średnia z jednego odczytu 1 Hz, bufor/format bez zmian, brak równoległych POST.
- [x] Ekran co 100 ms, proste pola odmalowywać po zmianie. Ograniczanie/zaokrąglanie RPM wykonywać na wartości do prezentacji, nie na danych loggera.
- [x] Testy SD: czas w ms, nowy nagłówek, interwał i opóźnienie, błędy I/O; istniejący test modemu obu POST z CRC. Kompilacja Mega/SRAM, przegląd, dokumentacja i commit.

Ograniczenia: są to docelowe interwały, nie pomiar sprzętowy. Odbiór GPS, DHT i operacje SD/TFT mogą je opóźniać. Szybki CSV przechowuje aktualny stan co 200 ms, a nie każdą ramkę DPID. RTC zapewnia datę do sekundy, millis identyfikuje odstępy między wierszami; millis zawija się po około 49,7 dnia.
