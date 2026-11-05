# Wspólna struktura kanałów — plan i wynik

Cel zatwierdzony przez użytkownika: jedna struktura kanałów, mniej nagłówków,
czytelny kod, identyczne zachowanie ekranów, CSV i LTE. Commit wyłącznie na
codex/vpw-stage1, ZIP zmienionych plików. Usunąć przypadkowy sscom.ini.

## Decyzje

- Cmd/ExtraChannels zastępuje Channel: wartość 16-bit, czas odbioru, poprawność.
  Surowe wartości unsigned odczytujemy przez jawne rzutowanie, bez zmiany bitów.
- cmd.h zawiera kanały, definicje PID, DPID, walidację i odczyt VPW.
  Usuwamy extra_channels.h, vpw_reply.h i vpw_dpid.h.
- display.h zawiera rysowanie pierwszego ekranu i wskaźników (bez gauge_indicators.h).
- Pamięć wyświetlacza i górne słowa GPS są osobne. Definicje zapytań we Flash,
  osobne znaczniki czasu wysłania. Kolejność inicjalizacji bez znaczenia dla I/O.
- SD/LTE nadal wykorzystują 17 bazowych kanałów; LogEntry 42 B, 30 rekordów,
  połowa transmisji 647 B. CSV 45 kolumn, interwały bez zmian.
- Nie naprawiamy przy okazji starych ograniczeń GPS/starych widoków: użytkownik
  wymaga zachowania działania. Formatowanie nie zmienia instrukcji.

## Kolejność

- [x] Zachować bazowy kod i dodać test wspólnej struktury przed implementacją.
- [x] Zintegrować kanały i VPW; dostosować odbiorców, wydzielić cache/GPS.
- [x] Przenieść wskaźniki do display.h i sformatować kod.
- [x] Uruchomić testy, porównać wyjścia CSV/LTE/rysowania z bazą, zbudować Mega.
- [ ] Przegląd końcowy, ZIP z instrukcją usunięcia nagłówków, commit/push bez merge.

## Weryfikacja

Pełne unsigned PW (65535), ujemne temperatury/SA, stare wartości i brak danych,
przepełnienie millis(), cztery DPID i fallback, oba widoki, GPS w pakiecie LTE.
Porównanie treści CSV i bajtów LTE z wersją 87f73bf. Kompilacja Mega określa
statyczny RAM, nie maksymalny stos; testów fizycznego ECU/modemu tutaj nie ma.

## Wynik

- W toku pracy użytkownik doprecyzował usunięcie zdublowanych tablic cmd_*.
  DPID_FIELDS jest jednym katalogiem PID i interwałów. makePidRequest buduje
  zapytania 5/7-bajtowe na stosie, fallback zachowuje wszystkie 21 PID-ów.
- Mega: Flash 90688 B, SRAM 5510 B, pozostaje 2682 B. Baza: 90710/5579 B.
- 6 zestawów hosta, test pola RX i test menu PowerShell przeszły.
- Porównanie z 87f73bf: CSV 1815 B, LTE wraz z komendami 1873 B,
  rysowanie 15538 B, kanały i fallback 63924 B: identyczne wyniki.
- Recenzent nie znalazł regresji produkcyjnej. Poprawiono jego uwagę dotyczącą
  możliwego użycia starych plików wynikowych przez test zgodności: wymagane
  dokładnie jedno trafienie miejsca przechwycenia, stare wyniki są usuwane.
  Próba z celowo błędną kotwicą i starym wynikiem zakończyła się oczekiwanym błędem.
- Ograniczenia czasowe GPS/SD i obcinanie wartości w starym widoku pozostają
  bez zmian zgodnie z zakresem. Nie testowano sprzętu i maksymalnego stosu.
