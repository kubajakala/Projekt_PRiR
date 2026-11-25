#ifndef WSPOLNE_H
#define WSPOLNE_H

/* liczba pierwsza 2^31-1, tokeny i skroty sa mniejsze */
#define P_HASZ 2147483647u

/* wartosc szkicu zbioru pustego (wieksza od kazdego skrotu) */
#define PUSTY 0xFFFFFFFFu

/* tryby programu */
#define PARY 1
#define TOPK 2

/* zbiory zapisane po kolei w jednej tablicy */
typedef struct {
  int n;               /* liczba zbiorow */
  int *rozm;           /* rozmiary zbiorow */
  int *pocz;           /* poczatki zbiorow w elem (n+1 pozycji) */
  unsigned int *elem;  /* elementy wszystkich zbiorow */
} zbiory_t;

/* pozycja na liscie top-k */
typedef struct {
  int dok;
  double podob;
} kandydat_t;

/* parametry z wiersza polecen */
typedef struct {
  int tryb;
  char *korpus;
  char *drugi;         /* plik par albo zapytan */
  int top;
  int k;
  unsigned long long ziarno;
  char *wynik;
} parametry_t;

double czas(void);
unsigned long long losuj(unsigned long long *stan);
int argumenty(int argc, char **argv, parametry_t *par, int wypisz);
int wczytaj_zbiory(const char *nazwa, zbiory_t *z);
int wczytaj_pary(const char *nazwa, int n, int *p, int **pary);
void usun_zbiory(zbiory_t *z);
int usun_duplikaty(unsigned int *x, int n);
void wspolczynniki(int k, unsigned long long ziarno, unsigned int *a, unsigned int *b);
void szkic(const unsigned int *x, int n, int k, const unsigned int *a,
           const unsigned int *b, unsigned int *s);
double jaccard(const unsigned int *x, int nx, const unsigned int *y, int ny);
double podobienstwo_szkicow(const unsigned int *s, const unsigned int *t, int k);
void wstaw(kandydat_t *lista, int *ile, int top, int dok, double podob);
int zapisz_pary(const char *nazwa, int p, const int *pary,
                const double *jac, const double *mh);
int zapisz_topk(const char *nazwa, int q, int top, const kandydat_t *lj,
                const kandydat_t *lm, const int *ile);

#endif
