# Klasyfikator 1-NN (1-Najbliższy Sąsiad)

Prosta implementacja algorytmu "uczenia leniwego" (lazy learning) wykorzystująca metodę **1-Najbliższego Sąsiada (1-NN)** do klasyfikacji danych wektorowych.

## 📄 Opis Zadania

Celem programu jest przeprowadzenie klasyfikacji danych testowych na podstawie zbioru treningowego. Program nie buduje modelu w fazie uczenia (jedynie wczytuje dane), a cała praca obliczeniowa wykonywana jest w momencie klasyfikacji (faza testowania).

### Dane wejściowe
Do zadania dołączone są dwa pliki CSV:
1.  **`train.csv`** – zbiór treningowy (wzorce).
2.  **`test.csv`** – zbiór testowy (dane do sklasyfikowania).

### Format danych
Każdy rekord w plikach składa się z trzech cech (współrzędnych wektora) oraz etykiety klasy:
- **Cechy:** $x, y, z$ (liczby rzeczywiste)
- **Etykieta klasy:** $label \in \{1, 2\}$

Przykładowa struktura wiersza:
```csv
x, y, z, label
```

### 💾Wczytywanie danych
Wczytywanie danych realizuje sie za pomocą funkcji Wektor *wczyt, która zwarca nam wskaźnik na nowo zaalokowane dane - czyli adres na pierwszy element do tych danych, w których znajduje się tablica struktur. 
Algorytm wczytywania oparty jest na dynamicznym alokowaniu pamięci za pomocą funkcji $realloc()$ - za każdym razem, gdy rozmiar naszej tablicy struktur jest mniejszy od pola $.capacity$ to zwiększamy zmienną $.capacity$ dwukrotnie i przydzielamy świeżo zaalokowaną pamięć dla naszej tablicy struktur Wektor.

Czytamy z pliku za pomocą funkcji $fgets()$, która zapisuje w zmiennej $buffer$ o rozmiarze $BUF-SIZE$, dokładnie jeden wiersz z pliku. Następnie wartości będące w zmiennej $buffer$ przypisujemy za pomocą funkcji $sscanf()$ do konkretnych pól tablicy struktury Wektor. 

Zmienna $buffer$ jest nadpisywana w każdej iteracji, dlatego przed wywołaniem pętli, która kończy się tylko gdy w pliku zabraknie wierszy, wywołujemy funkcje $fgets()$, która zapisze pierwszy wiersz(nagłówek) do zmiennej $buffer$, ponieważ w nagłówku znajdują się śmieci, których nie potrzebujemy. 
Zatem, gdy wejdziemy do pętli ten nagłówek nam się automatycznie nadpisze i w zmiennej $buffer$ zostaną ważne dla nas zmienne ($x, y, z, label$).

Na koniec, jeśli zaalokowaliśmy zbyt wiele pamięci to za pomocą funkcji $realloc()$ alokujemy pamięć na dokładną liczbę elementów w naszej tablicy.

Funkcja ta zwraca nam wskaźnik(adres) na zaalokowane elementy - w tym przypadku jest to tablica struktur Wektor.


### 🧮Algorytm i Matematyka
Do określeia podobieństa między wektorami wykorzystywana jest odległość euklidesowa. 
Dla wektora testowego $t_{1}$ = ($x_{1},y_{1},z_{1}$) i wektora treningowego $t_{2}$ = ($x_{2},y_{2},z_{2}$), odległość $d$ wyraża się wzorem:

$$
d(t_{1},t_{2}) = (x_{2}-x_{1})^{2} + (y_{2}-y_{1})^{2} + (z_{2}-z_{1})^{2}
$$

Pierwiastek został pominięty w celu usprawnienia obliczeń oraz nie był on potrzebny do znalezienia najbliższego sąsiada (1-NN).

### 💡Logika działania
Algorytm działa wobec następującego schematu:
  1. Obliczamy odległość wektora testowego do każdego punktu ze zbioru treningowego,
  2. Znajdujemy najbliższego sąsiada za pomoca prostej instrukcji $if$ - jeśli odległość do danego wektora w danej iteracji była mniejsze od poprzedniej to do zmiennej $min-dist$ przypisujemy wartośc obliczonej w tej iteracji odległości.
  3. Jeśli instrukcja warunkowa zostanie spełniona to również do zmiennej $type$ przypisujemy pole struktury $.type$ aktualnie oblicznego wektora treningowego,
  4. Gdy obliczymy już odległości dla wszystkich punktów ze zbioru treningowego i znajdziemy najbliższego sąsiada to wychodząc z pętli $j$ i wkraczając do pętli $i$ (pętla iterująca przez wektor testowy) sprawdzamy w instrukcji $if$ czy typ naszego najbliższego sąsiada zgadza się z polem $.type$ aktualnego wektora testowego,
  5. Jeśli tak zwiększamy zmienną $local-hits$ w przeciwym wypadku zwiększamy zmienną $local-missesd$. Natomiast po każdej iteracji pętli $i$ zwiększamy zmienną $local-processed$.

Zastosowano niezbędne środki, aby nie dochodziło do tzw. racing conditions poprzez zastosowanie mutexów, barier oraz zmiennych warunkowych.
Podczas analizy w czasie rzeczywistym możliwe są następujące czynności:
  1. [P] Wyświetl rezulaty
  2. [W] Wstrzymaj/Wznów działanie
  3. [R] Reset
  4. [Z] Zakończ program

Wątki obliczeniowe modyfikują wspólne zmienne tylko co 20 iteracji pętli $i$.
Po zakończeniu pęlti $i$, czyli po przeanalizowaniu wszystkcih wektorów ze zbioru testowego, aktualizujemy statystyki, które mogły się nie załapać przez aktualizację statystyk globalnych co 20 iteracji pętli $i$.

W celu przyspieszenia obliczania danych zastosowaliśmy paralelizację w taki sposób, że każdy wątek posiada własne ID. Od tego ID wątki zaczynają pętle z wektorami testowymi. 
Ta pętla ma krok $NUM-THREAD$ dzięki czemu każdy wątek będzie obliczał inny wektor testowy co zoptymalizuje pracę i każdy wątek będzie miał do oblicznia taką wartość: $dane-watku->size-test/5$, zamiast przeiterowania po całym zbiorze testowym. 
Dzieki temu wątki nie wykonują "podwójnej pracy" i nie obliczają wartości odległości dla tych samych wektorów ze zbioru testowego.
Takie działanie znacznie zmniejsza czas analizy danych.

### 📊Ocena klasyfikacji
Wzór na obliczenia dokładności naszych danych:

$$
Accuracy = \frac{HITS}{HITS+MISSES} 
$$

### 📊Przykładowy wynik działania

$$
\begin{array}{l}
\textbf{OSTATECZNE DANE} \\
\text{POSTĘP: } 20000/20000 [100.0]\\
\text{TRAFIENIA: } 13600 \\
\text{PUDŁA: } 6400 \\
\text{DOKŁADNOŚĆ: } 0.680 \\
\text{STAN OBLICZEŃ: ZAKOŃCZONY}
\end{array}
$$

### Licencja
Ten projekt jest objęty licencją MIT - zobacz plik [LICENSE](LICENSE) po więcej szczegółów.
