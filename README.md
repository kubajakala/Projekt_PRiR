# Podobieństwo zbiorów i szkice danych (Jaccard, MinHash)

Programowanie równoległe i rozproszone 2026/2027, temat 34 (D05).

Autorzy: Jakub Jąkała, Sylwester Jarosz


## Zakres realizacji

- Dokładne podobieństwo Jaccarda i przybliżenie MinHash dla zbiorów tokenów.
- Zespół dwuosobowy: porównania zadanej listy par (tryb `pary`) oraz wyszukiwanie
  top-k względem zapytań (tryb `topk`).
- Wersje: sekwencyjna (referencja), OpenMP, MPI; CUDA w przygotowaniu.
- Wszystkie wersje przetwarzają te same dane i dają identyczne wyniki.

| Etap | Stan |
|---|---|
| Referencja i testy | gotowe |
| OpenMP i MPI | gotowe |
| CUDA | w przygotowaniu |
| Eksperymenty | w przygotowaniu |
| Raport | w przygotowaniu |


## Podział pracy

| Osoba | Zakres |
|---|---|
| Jakub Jąkała | format danych i generator; wczytywanie, usuwanie duplikatów i dokładny Jaccard (tryb par); wersja OpenMP; CUDA: haszowanie i budowa szkiców na GPU; pomiary OpenMP, wykresy przyspieszenia i efektywności |
| Sylwester Jarosz | MinHash: funkcje skrótu, szkice i porównania szkiców; wyszukiwanie top-k; wersja MPI; CUDA: porównania szkiców i top-k na GPU, grupowanie zadań o podobnej długości; pomiary MPI i GPU, błąd estymacji i zgodność rankingu |
| Wspólnie | testy poprawności, raport, obrona |


## Uruchomienie

Wymagane: Linux, gcc z OpenMP, make, OpenMPI. Na Windowsie w kontenerze Dockera
(PowerShell, z katalogu repozytorium):

```
docker run --rm -it -v "${PWD}:/lab" -w /lab devbox-mpi:latest bash
```

Budowanie i testy:

```
make
bash testy/testy.sh
```

Programy:

```
./generator <zbiory> <zapytania> <pary> <rozmiar_min> <rozmiar_max> <udzial_min> <udzial_max> <ziarno> <prefiks>

./jaccard_sekw pary <korpus> <pary> <k> <ziarno> <wynik>
./jaccard_sekw topk <korpus> <zapytania> <top> <k> <ziarno> <wynik>
OMP_NUM_THREADS=<watki> ./jaccard_omp <te same argumenty>
mpirun -np <procesy> ./jaccard_mpi <te same argumenty>
```

`k` to długość szkicu MinHash, `top` to długość listy top-k.

Przykład:

```
./generator 20000 50 200000 100 1000 0 1 11 /tmp/dane
./jaccard_sekw pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_sekw.txt
OMP_NUM_THREADS=6 ./jaccard_omp pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_omp.txt
mpirun -np 6 ./jaccard_mpi pary /tmp/dane_korpus.txt /tmp/dane_pary.txt 128 1 /tmp/pary_mpi.txt
./jaccard_sekw topk /tmp/dane_korpus.txt /tmp/dane_zapytania.txt 10 128 1 /tmp/topk.txt
```


## Struktura repozytorium

| Plik | Zawartość |
|---|---|
| `wspolne.h`, `wspolne.c` | funkcje wspólne: wczytywanie, duplikaty, haszowanie, szkice, Jaccard, top-k |
| `generator.c` | generator danych |
| `jaccard_sekw.c` | wersja sekwencyjna (referencja) |
| `jaccard_omp.c` | wersja OpenMP |
| `jaccard_mpi.c` | wersja MPI |
| `Makefile` | budowanie |
| `testy/` | dane testowe, wyniki oczekiwane, `testy.sh` |
| `NOTATNIK.txt` | notatki zespołu |
