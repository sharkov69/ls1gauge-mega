# Połączenia i sprzęt

[Powrót do README](../README.md)

## Zakres opisu

Poniższe przypisania wynikają z aktywnego kodu. Numery oznaczają piny Arduino Mega 2560, a nie numery nóżek ATmega2560. Sprzętowe SPI, I2C i UART opisuje [oficjalna dokumentacja Mega 2560](https://store.arduino.cc/products/arduino-mega-2560-rev3).

Kod nie określa modelu modułu VPW ani modemu, napięć ich zasilania, układu konwersji poziomów i numeracji złącza samochodowego. Ustal te elementy na podstawie schematu działającego urządzenia. Nie łącz przewodu samochodowej magistrali VPW bezpośrednio z GPIO Mega. Potrzebujesz interfejsu elektrycznego VPW zgodnego z polaryzacją biblioteki.

## Zestawienie pinów

| Funkcja | Pin Mega | Połączenie / rola | Źródło |
| --- | --- | --- | --- |
| VPW RX | D3 | Wyjście odbiornika interfejsu VPW → Mega; przerwanie `CHANGE`, `INPUT_PULLUP` | `cmd.h`, biblioteka VPW |
| VPW TX | D5 | Mega → wejście nadajnika interfejsu VPW | `cmd.h` |
| TFT 1 CS | D9 | Wybór pierwszego ekranu | `.ino` |
| TFT 1 DC | D11 | Dane / komenda pierwszego ekranu | `.ino` |
| TFT 1 RST | D7 | Reset pierwszego ekranu | `.ino` |
| TFT 2 CS | D15 | Wybór drugiego ekranu | `.ino` |
| TFT 2 DC | D17 | Dane / komenda drugiego ekranu | `.ino` |
| TFT 2 RST | D13 | Reset drugiego ekranu | `.ino` |
| SPI MOSI / COPI | D51 | Do wejść danych obu TFT i karty SD | Sprzętowe SPI Mega |
| SPI MISO / CIPO | D50 | Z wyjścia danych karty SD | Sprzętowe SPI Mega |
| SPI SCK | D52 | Zegar obu TFT i karty SD | Sprzętowe SPI Mega |
| SD CS | D53 | Wybór karty SD | `sd.h` |
| RTC SDA | D20 | SDA modułu DS3231 | Sprzętowe I2C Mega |
| RTC SCL | D21 | SCL modułu DS3231 | Sprzętowe I2C Mega |
| RTC SQW | D2 | Impuls 1 Hz; przerwanie na zboczu opadającym | `rtc.h`, `.ino` |
| DS18B20 DATA | D47 | Wspólna magistrala OneWire dwóch czujników | `temp.h` |
| DHT22 DATA | D48 | Dane temperatury i wilgotności | `temp.h` |
| Przycisk 1 | D27 | Przycisk między pinem a GND | `.ino` |
| Przycisk 2 | D29 | Przycisk między pinem a GND | `.ino` |
| Przycisk 3 | D31 | Przycisk między pinem a GND | `.ino` |
| Modem RX | D18 / TX1 | Mega TX1 → RX modemu | `Serial1`, `.ino` |
| Modem TX | D19 / RX1 | TX modemu → Mega RX1 | `Serial1`, `.ino` |
| Wyjście o niepotwierdzonej funkcji | D12 | Program ustawia `OUTPUT` i `LOW`; sekwencja resetu modemu jest zakomentowana | `.ino`, `modem.h` |
| USB / terminal | `Serial` | 115200 bit/s; ustawianie czasu i opcjonalne komunikaty | `.ino` |
| Źródło ziarna losowania | A0 | `analogRead(A0)` dla `randomSeed()` | `.ino` |

## Ekrany i karta SD

Oba ekrany korzystają ze sprzętowego SPI oraz osobnych sygnałów CS, DC i RST. TFT 1 prezentuje parametry pojazdu, TFT 2 zegar, czujniki i menu. Kod rysuje interfejs dla ekranów 240 × 240.

Połącz D51 i D52 ze wszystkimi trzema urządzeniami SPI. Każde urządzenie ma własny CS: D9, D15 albo D53. D11 i D13 w tym projekcie pełnią funkcje sterujące TFT; nie są liniami MOSI/SCK jak na Uno. Pinów D15 i D17 nie można równocześnie użyć do odbioru przez Serial3 i Serial2.

Dobierz napięcie zasilania i konwersję poziomów do konkretnych modułów TFT i SD. Sam układ GC9A01A nie określa odporności gotowego modułu na sygnały 5 V. Sterowania podświetleniem BL/LED nie ma w kodzie. Funkcja wygaszania wysyła komendy do kontrolerów ekranów.

## RTC i czujniki

DS3231 wymaga SDA, SCL oraz osobnego przewodu SQW do D2. Program konfiguruje SQW na 1 Hz i wyłącza wyjście 32 kHz. Zapewnij podciąganie linii SQW zgodnie z modułem: w szkicu D2 ma tryb `INPUT`, bez wewnętrznego podciągania. Bez impulsu SQW program nie uruchomi cyklu próbkowania logów ani sekundowych aktualizacji zegara.

Oba DS18B20 podłącz do wspólnej linii DATA na D47, z podciąganiem wymaganym przez OneWire. Wpisz adresy ROM własnych czujników do `tempIn` i `tempOut`. Komentarz o D2 w `temp.h` jest nieaktualny; obowiązuje `ONE_WIRE_BUS 47`.

DHT22 używa D48. Sprawdź, czy jego moduł zawiera rezystor podciągający DATA; dobierz połączenie według dokumentacji użytej wersji czujnika. Z kodu nie wynika fizyczne położenie czujników, jedynie nazwy: wnętrze, zewnętrze i nawiew.

## VPW, modem i zasilanie

Na stronie logicznej interfejsu VPW biblioteka oczekuje stanu spoczynkowego HIGH. Nadaje początek ramki przez ustawienie TX na LOW i obserwuje RX podczas arbitrażu. To opis zachowania GPIO; schemat zewnętrznego układu musi określić jego przełożenie na napięcia magistrali.

Modem łączy się z Mega przez UART `Serial1`, 115200 bit/s. Skrzyżuj TX/RX i zapewnij zgodne poziomy logiczne. Kod używa m.in. `AT+NETOPEN`, `AT+HTTPINIT` i `AT+CGPSINFO`; model i firmware modemu muszą obsługiwać tę sekwencję. Nie przypisuj D12 do PWRKEY lub RESET bez sprawdzenia schematu.

Zapewnij wspólną masę po stronie logiki, zasilanie dostosowane do poboru prądu modemu oraz ochronę zasilania samochodowego. Repozytorium nie daje podstaw do wskazania konkretnej przetwornicy, bezpiecznika ani wartości elementów interfejsu VPW.
