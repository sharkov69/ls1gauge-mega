# DEBUG_MODEM

Uzgodniony cel: opcjonalny log USB 115200 do analizy startu modemu, bez blokowania i bez rosnących buforów. Domyślnie wyłączony, konfiguracja w modem_debug_config.h. Komendy, odpowiedzi (również odrzucane przy opróżnianiu UART), wynik/timeout, etap i próba, okresowy RAM/licznik pominięć. Nie logujemy binarnego POST. Nie obiecujemy detekcji przepełnienia sprzętowego UART, której Arduino HardwareSerial nie udostępnia.

Wpis do 63 bajtów na stosie, sprawdzenie dostępnego miejsca przed całym zapisem. Bufor fragmentu RX 25 bajtów, liczniki o stałym rozmiarze, brak kolejki i String. Po resetach zapis surowego MCUSR z początku setup z zastrzeżeniem ingerencji bootloadera. Nie zmieniamy protokołu modemu ani timeoutów.

- [x] Testy pełnego USB, ograniczenia długości, RX/timeout, startu i obu POST z włączoną diagnostyką.
- [x] Implementacja i skrypt PowerShell do portu COM, bez instalacji zależności.
- [x] Kompilacja obu wariantów, SRAM, dokumentacja, publikacja i ZIP zmian.
