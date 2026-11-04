# CSV w polskim Excelu

Od poprawki z 2026-11-04 logger zapisuje `l-YYMMDD.csv` z jedną kolumną czasu. Każdy wiersz ma 18 pól: `Datetime` oraz 17 kanałów. Separator pól to średnik, a separator dziesiętny to przecinek. Nie ma kolumny `Uptime_ms`.

Przykład skrócony do trzech kolumn:

```csv
Datetime;Engine RPM;AFR Bank 1
2026-11-04 14:32:10,000;850;14,75
2026-11-04 14:32:10,200;875;14,72
```

Zapis pozostaje nominalnie co 200 ms. Licznik RX mierzy wartości odebrane z VPW; wiersz CSV zawiera aktualny zestaw kanałów, nie pojedynczą ramkę. Wynik około 6500 RX/min nie oznacza 6500 wierszy CSV/min.

## Import i wykres

1. W Excelu wybierz **Dane → Z tekstu/pliku CSV** i wskaż plik.
2. Ustaw separator **średnik**. Wybierz **Przekształć dane**, jeśli Excel błędnie rozpozna typy.
3. Dla `Datetime` wybierz typ **Data/godzina**, używając ustawień regionalnych **Polski (Polska)**. Dla pomiarów wybierz liczby dziesiętne z tymi samymi ustawieniami. W Power Query służy do tego **Zmień typ → Użyj ustawień regionalnych**.
4. Po załadowaniu ustaw dla kolumny czasu format niestandardowy `rrrr-mm-dd gg:mm:ss,000` (lub `gg:mm:ss,000`, jeśli chcesz wyświetlać sam czas).
5. Wstaw wykres **punktowy XY z liniami**. Jako X wybierz `Datetime`, jako Y np. `Engine RPM`. Wykres XY zachowa rzeczywiste przerwy między zapisami.

Bezpośrednie otwarcie dwuklikiem zależy od ustawień regionalnych Windows/Excela. W razie jednej kolumny lub dat jako tekstu użyj importu powyżej. [Microsoft opisuje ustawienia separatorów i import CSV](https://support.microsoft.com/en-us/excel/get-started/import-or-export-text-txt-or-csv-files). Nie wykonywano testu importu w aplikacji Excel; sprawdzono format w testach loggera.

## Czas próbek i data pliku

RTC dostarcza datę i pełne sekundy. Przy pierwszym zapisie program kotwiczy w nich licznik czasu, następnie dodaje rzeczywiście upływające milisekundy Arduino. Obsługuje zawinięcie `millis()`. Trzy cyfry ułamka służą do przedstawienia odstępów próbek; nie oznaczają synchronizacji absolutnej z dokładnością 1 ms. Niepewność początkowej fazy względem RTC jest mniejsza niż sekunda. Różnica zegarów większa niż sekunda, np. po ręcznej zmianie RTC, ponownie kotwiczy licznik. Restart lub korekta zegara może przerwać ciągłość osi czasu.

SdFat pobiera datę utworzenia/modyfikacji z RTC przy tworzeniu i synchronizacji pliku. Wcześniej kod nie rejestrował tej funkcji, więc biblioteka używała domyślnej daty FAT. Czas modyfikacji FAT ma rozdzielczość 2 s; milisekundy znajdziesz w CSV. Poprawka nie przepisuje dat wszystkich historycznych plików.

## Istniejące logi

Jeżeli `l-YYMMDD.csv` ma ten sam nagłówek, logger dopisuje kolejne wiersze. Jeżeli ma stary układ, program zachowuje go jako `l-YYMMDD.bak` i tworzy nowy CSV. Kopię można otworzyć w Excelu przez import. Jeśli plik `.bak` już istnieje, program niczego nie nadpisuje: zgłasza błąd SD i ponawia próbę po 5 s. W takim przypadku skopiuj oba stare pliki z karty i zwolnij nazwę docelowego CSV. Poprzednie pliki `f-*.csv` pozostają bez zmian.

Liczby zapisujemy bez pośredniego bufora tekstu: napięcie i dodatkowe temperatury mają jedno miejsce po przecinku, MAP/AFR/olej dwa, GPS sześć. Dzielniki i przeliczniki pozostają dotychczasowe, w tym znana rozbieżność skali oleju wobec ekranu. Format binarny LTE pozostaje bez zmian.

## Weryfikacja

Pięć zestawów testów hosta przeszło. Testy obejmują metadane FAT, 18 kolumn, polskie liczby i ujemne temperatury, GPS, backup starego pliku, konflikt kopii, ponowne otwarcie, północ, korektę RTC oraz zawinięcie czasu. Kompilacja Mega: Flash 85 332 B, statyczny SRAM 5423 B, pozostało 2769 B na stos/stertę. Względem poprzedniego commitu SRAM wzrósł o 21 B. Nie zmierzono maksymalnego stosu ani czasu operacji na fizycznej karcie.
