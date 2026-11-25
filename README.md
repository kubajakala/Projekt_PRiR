# Podobieństwo zbiorów i szkice danych (Jaccard, MinHash)

Projekt z przedmiotu Programowanie równoległe i rozproszone, rok 2026/2027.
Temat 34, identyfikator D05 (Data Science), zespół dwuosobowy.

Autorzy: Jakub Jąkała, Sylwester Jarosz


## Temat projektu

Treść karty tematu:

> Data Science  
> 34 Podobieństwo zbiorów i szkice danych  
> Identyfikator D05 | Zespół 2 osoby | Poziom łatwy
>
> #### Opis
>
> Projekt porównuje dokumenty lub inne obiekty opisane zbiorami elementów. Należy obliczać dokładne podobieństwo Jaccarda oraz przybliżenie na podstawie szkiców MinHash. W przeciwieństwie do częstości słów reprezentacja zbiorowa uwzględnia obecność tokenu, a nie liczbę jego wystąpień. Celem jest ocena, ile czasu i pamięci można oszczędzić kosztem kontrolowanego błędu.
>
> Program wczytuje zbiory tokenów lub proste teksty, usuwa duplikaty i buduje szkice. Zapisuje podobieństwa dla zadanych par albo ranking względem zapytania. Należy stosować identyczne haszowanie we wszystkich wariantach oraz określić konwencję podobieństwa zbiorów pustych.
>
> #### Zakres implementacji
>
> OpenMP - Przydzielić dokumenty lub pary do wątków. Dla dokładnej referencji porównywać uporządkowane zbiory, a dla szkiców liczyć zgodne pozycje.
>
> MPI - Rozdzielić pary porównań albo fragmenty korpusu i zebrać wyniki. Koszt replikacji szkiców lub przesyłania zapytań uwzględnić oddzielnie.
>
> CUDA lub OpenCL - Równolegle wyznaczać wartości skrótów, minima oraz podobieństwa szkiców. Grupować zadania o podobnej długości, jeśli pozwala to ograniczyć nieaktywną pracę.
>
> #### Dodatkowe wyniki do raportu
>
> Generator powinien kontrolować rozmiary zbiorów i ich część wspólną. Raportować błąd estymacji, zgodność rankingu, pary/s, czas budowy szkiców i pamięć dla kilku długości szkicu.
>
> #### Zakres zespołu
>
> 1 osoba: porównania zadanej listy par.
>
> 2 osoby: wyszukiwanie top k lub strategia grupowania zbiorów o różnych rozmiarach.
>
> #### Rozwinięcie do artykułu lub pracy magisterskiej
>
> Jak długość szkicu wpływa na jakość rankingu i czas dla różnych rozkładów rozmiaru zbiorów?


## Zakres realizacji

- Zespół dwuosobowy, więc oprócz porównań zadanej listy par (tryb `pary`) program
  wyszukuje top-k zbiorów najbardziej podobnych do zapytania (tryb `topk`).
  Z dwóch dodatków podanych w karcie wybraliśmy wyszukiwanie top-k.
- Każdy tryb liczy dokładne podobieństwo Jaccarda i przybliżenie MinHash.
- Wersje: referencyjna sekwencyjna (`jaccard_sekw`), OpenMP (`jaccard_omp`),
  MPI (`jaccard_mpi`). Wersja CUDA jest w przygotowaniu.
- Wszystkie wersje przetwarzają te same dane, używają tego samego haszowania
  i zapisują identyczne pliki wyników.

| Etap | Stan |
|---|---|
| Referencja i testy poprawności | gotowe |
| OpenMP i MPI | gotowe |
| CUDA | w przygotowaniu |
| Eksperymenty i pomiary | w przygotowaniu |
| Raport | w przygotowaniu |


## Podział pracy

| Osoba | Zakres |
|---|---|
| Jakub Jąkała | format danych i generator; wczytywanie, usuwanie duplikatów i dokładny Jaccard (tryb par); wersja OpenMP; CUDA: haszowanie i budowa szkiców na GPU; pomiary OpenMP, wykresy przyspieszenia i efektywności |
| Sylwester Jarosz | MinHash: funkcje skrótu, szkice i porównania szkiców; wyszukiwanie top-k; wersja MPI; CUDA: porównania szkiców i top-k na GPU, grupowanie zadań o podobnej długości; pomiary MPI i GPU, błąd estymacji i zgodność rankingu |
| Wspólnie | testy poprawności, raport, obrona |


## Wymagane oprogramowanie

- Linux, kompilator gcc z OpenMP, GNU make, OpenMPI (`mpicc`, `mpirun`),
  bash i awk (testy).
- Debian/Ubuntu: pakiety `gcc`, `make`, `openmpi-bin`, `libopenmpi-dev`.
- Sprawdzone na:
  - Debian 13, gcc 14.2, OpenMPI 5.0.7 (Docker na Windows 11, WSL2),
  - Ubuntu 22.04, gcc 11.4 (programy bez MPI).

Na Windowsie uruchamiamy wszystko w kontenerze Dockera z gcc i OpenMPI,
z katalogu repozytorium (PowerShell):

```
docker run --rm -it -v "${PWD}:/lab" -w /lab devbox-mpi:latest bash
```

`devbox-mpi` to nasz lokalny obraz (Debian 13 z pakietami wymienionymi wyżej).


## Budowanie

```
make                                       # wszystkie programy
make generator jaccard_sekw jaccard_omp    # bez MPI
make clean                                 # usuwa pliki .o
```

Wszystkie wersje są kompilowane z flagą `-O2` (zmienna `OPT` w `Makefile`).


## Uruchomienie

### Generator danych

```
./generator <zbiory> <zapytania> <pary> <rozmiar_min> <rozmiar_max> <udzial_min> <udzial_max> <ziarno> <prefiks>
```

| Parametr | Znaczenie |
|---|---|
| `zbiory` | liczba zbiorów w korpusie |
| `zapytania` | liczba zapytań (wzorców) |
| `pary` | liczba par do porównania |
| `rozmiar_min`, `rozmiar_max` | zakres rozmiaru zbioru |
| `udzial_min`, `udzial_max` | zakres udziału części wspólnej zbioru z jego wzorcem (od 0 do 1) |
| `ziarno` | ziarno generatora liczb losowych |
| `prefiks` | początek nazw plików wynikowych |

Generator zapisuje `prefiks_korpus.txt`, `prefiks_zapytania.txt` i `prefiks_pary.txt`.
Zapytania to wzorce z losowymi tokenami. Zbiór o numerze i ma wzorzec numer
i % zapytania, losowy rozmiar s i część wspólną z wzorcem równą
u * min(s, rozmiar wzorca), gdzie u jest losowane z zakresu udziału. Pozostałe
tokeny są losowe. Pary są losowane spośród zbiorów o tym samym wzorcu.

### Programy

```
./jaccard_sekw pary <korpus> <pary> <k> <ziarno> <wynik>
./jaccard_sekw topk <korpus> <zapytania> <top> <k> <ziarno> <wynik>

OMP_NUM_THREADS=<watki> ./jaccard_omp <te same argumenty>
mpirun -np <procesy> ./jaccard_mpi <te same argumenty>
```

| Parametr | Znaczenie |
|---|---|
| `pary` / `topk` | tryb: porównania listy par albo ranking względem zapytań |
| `korpus` | plik zbiorów |
| `pary` / `zapytania` | plik par albo plik zbiorów-zapytań |
| `top` | długość listy top-k (tylko tryb `topk`) |
| `k` | długość szkicu MinHash (liczba funkcji skrótu) |
| `ziarno` | ziarno współczynników funkcji skrótu |
| `wynik` | plik wyników |

Na komputerze z 6 rdzeniami i 12 wątkami sprzętowymi `mpirun` bez dodatkowych
opcji pozwala na najwyżej 6 procesów. Dla większej liczby trzeba dodać
`--use-hwthread-cpus`.

### Przykład

```
make
./generator 20000 50 200000 100 1000 0 1 11 /tmp/dane
./jaccard_sekw pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_sekw.txt
OMP_NUM_THREADS=6 ./jaccard_omp pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_omp.txt
mpirun -np 6 ./jaccard_mpi pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_mpi.txt
cmp /tmp/pary_sekw.txt /tmp/pary_omp.txt && cmp /tmp/pary_sekw.txt /tmp/pary_mpi.txt && echo identyczne
./jaccard_sekw topk /tmp/dane_korpus.txt /tmp/dane_zapytania.txt 10 128 1 /tmp/topk.txt
```

Plik korpusu z tego przykładu ma 116 MB, dlatego dane trzymamy poza repozytorium.

Program wypisuje parametry, pamięć zbiorów po usunięciu duplikatów i pamięć
szkiców oraz czasy faz. Przykładowy wydruk wersji sekwencyjnej (czasy zależą
od komputera):

```
tryb: pary, zbiory: 20000, pary: 200000, k: 128, ziarno: 1
pamiec zbiorow: 44245316 B, pamiec szkicow: 10240000 B
czas wczytania:            0.824706 s
czas usuwania duplikatow:  0.614415 s
czas budowy szkicow:       2.318214 s
czas porownan dokladnych:  0.864526 s
czas porownan szkicow:     0.058494 s
czas zapisu:               0.079431 s
czas calkowity:            4.759820 s
```

Wersja MPI wypisuje dodatkowo czas rozesłania korpusu, replikacji zbiorów
i szkiców, rozesłania par albo zapytań oraz zebrania wyników. Czasy faz w MPI
są maksimum po procesach. Czas całkowity obejmuje wczytanie i zapis plików.


## Formaty plików

Plik zbiorów (korpus i zapytania): w pierwszej linii liczba zbiorów, potem
w każdej linii rozmiar zbioru i jego tokeny. Token to liczba całkowita
od 0 do 2147483646. Tokeny mogą się powtarzać i być w dowolnej kolejności.

```
3
3 5 3 1
0
2 8 7
```

Plik par: w pierwszej linii liczba par, potem pary numerów zbiorów (od 0).

```
2
0 2
1 1
```

Wynik trybu `pary`, jedna linia na parę:
`zbior1 zbior2 jaccard minhash`

Wynik trybu `topk`, jedna linia na miejsce w rankingu:
`zapytanie miejsce dok_jaccard jaccard dok_minhash minhash`
(ranking dokładny i ranking ze szkiców obok siebie).


## Testy poprawności

```
make
bash testy/testy.sh
```

Skrypt uruchamia się z katalogu głównego repozytorium. Potrzebuje `mpirun`.
Uruchamia do 12 wątków i 12 procesów MPI z opcją `--oversubscribe`, więc działa
także na komputerze z mniejszą liczbą rdzeni (sprawdzone na 4 CPU). Kończy się wierszem
`Liczba bledow: 0` i kodem 0, gdy wszystkie testy przechodzą. Sprawdza:

1. referencję w trybie par na małych danych z wynikami policzonymi ręcznie
   (`testy/korpus.txt`, `testy/pary.txt`, `testy/pary_oczekiwane.txt`): zbiory
   puste, powtórzenia, kolejność tokenów, zbiory identyczne i rozłączne,
   największy dopuszczalny token,
2. referencję w trybie top-k (`testy/zapytania.txt`, `testy/topk_oczekiwane.txt`):
   remisy, top większe od liczby zbiorów, ranking ze szkiców,
3. błąd MinHash na danych z generatora: co najmniej 99% par spełnia
   |MinHash - J| <= 4 * sqrt(J(1-J)/k) przy k = 256,
4. OpenMP (1, 2, 3, 4, 12 wątków) i MPI (1, 2, 3, 4, 12 procesów): pliki wyników
   identyczne z referencją, także przy liczbie zbiorów mniejszej niż liczba
   procesów i przy pustym korpusie,
5. błędne dane (za duży token, ucięty plik, zły numer pary, brak pliku, k = 0,
   top = 0): każda wersja kończy się kodem błędu.


## Metoda

- Usuwanie duplikatów: sortowanie zbioru (qsort) i usunięcie powtórzeń.
- Dokładny Jaccard: scalanie dwóch posortowanych zbiorów,
  J = |A∩B| / (|A| + |B| - |A∩B|).
- MinHash: k funkcji h_i(x) = (a_i * x + b_i) mod (2^31 - 1). Szkic zbioru to
  minima tych funkcji po elementach, przybliżenie J to udział zgodnych pozycji
  dwóch szkiców. Współczynniki a_i, b_i daje generator splitmix64 z podanego
  ziarna (funkcja `wspolczynniki` w `wspolne.c`), wspólny dla wszystkich wersji.
- Zbiory puste: J(pusty, pusty) = 1, J(pusty, niepusty) = 0. MinHash daje to
  samo, bo szkic zbioru pustego ma na każdej pozycji 0xFFFFFFFF, a skróty są
  mniejsze od 2^31 - 1.
- Top-k: malejąco po podobieństwie, przy remisie mniejszy numer zbioru.
- Tolerancja: wyniki są ilorazami liczb całkowitych liczonymi tak samo
  w każdej wersji, więc wersje równoległe porównujemy z referencją bez
  tolerancji. Błąd MinHash względem dokładnego J sprawdza test 3.
- OpenMP: dokumenty przydzielane wątkom przy usuwaniu duplikatów i budowie
  szkiców (`schedule(dynamic)`, bo zbiory mają różne rozmiary), pary przydzielane
  wątkom przy porównaniach (dokładne `dynamic,64`, szkice `static`). W top-k
  podobieństwa zapytania do wszystkich dokumentów liczone są równolegle,
  a listę wybiera jeden wątek.
- MPI: proces 0 wczytuje dane i rozsyła fragmenty korpusu (`MPI_Scatterv`).
  Każdy proces usuwa duplikaty i buduje szkice swojego fragmentu.
  - tryb par: replikacja zbiorów i szkiców (`MPI_Allgatherv`, osobne czasy),
    podział listy par (`MPI_Scatterv`), zebranie wyników (`MPI_Gatherv`),
  - tryb top-k: rozesłanie zapytań (`MPI_Bcast`, osobny czas), top-k na
    fragmencie, zebranie list (`MPI_Gather`) i scalenie w procesie 0.
- Wyniki MPI z jednego komputera są wynikami lokalnymi, a nie pomiarem
  skalowalności klastra.


## Struktura repozytorium

| Plik | Zawartość |
|---|---|
| `wspolne.h`, `wspolne.c` | wczytywanie i zapis plików, usuwanie duplikatów, funkcje skrótu, szkice, Jaccard, wybór top-k, pomiar czasu |
| `generator.c` | generator danych |
| `jaccard_sekw.c` | wersja referencyjna (sekwencyjna) |
| `jaccard_omp.c` | wersja OpenMP |
| `jaccard_mpi.c` | wersja MPI |
| `Makefile` | budowanie |
| `testy/` | dane testowe, wyniki oczekiwane, `testy.sh` |
| `NOTATNIK.txt` | notatki zespołu: ustalenia i kolejne kroki |
