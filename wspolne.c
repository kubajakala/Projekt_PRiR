#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wspolne.h"

/* czas zegara monotonicznego [s] */
double czas(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* generator splitmix64 */
unsigned long long losuj(unsigned long long *stan)
{
  unsigned long long z;

  *stan += 0x9E3779B97F4A7C15ULL;
  z = *stan;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

/* parametry z wiersza polecen */
int argumenty(int argc, char **argv, parametry_t *par, int wypisz)
{
  int ok = 0;

  if(argc==7 && strcmp(argv[1],"pary")==0){
    par->tryb = PARY;
    par->korpus = argv[2];
    par->drugi = argv[3];
    par->top = 0;
    par->k = atoi(argv[4]);
    par->ziarno = strtoull(argv[5], NULL, 10);
    par->wynik = argv[6];
    ok = par->k > 0;
  }
  else if(argc==8 && strcmp(argv[1],"topk")==0){
    par->tryb = TOPK;
    par->korpus = argv[2];
    par->drugi = argv[3];
    par->top = atoi(argv[4]);
    par->k = atoi(argv[5]);
    par->ziarno = strtoull(argv[6], NULL, 10);
    par->wynik = argv[7];
    ok = par->k > 0 && par->top > 0;
  }
  if(!ok && wypisz){
    printf("Uzycie:\n");
    printf("  %s pary <korpus> <pary> <k> <ziarno> <wynik>\n", argv[0]);
    printf("  %s topk <korpus> <zapytania> <top> <k> <ziarno> <wynik>\n", argv[0]);
    printf("k - dlugosc szkicu (k >= 1), top - dlugosc listy (top >= 1)\n");
  }
  return !ok;
}

/* plik: liczba zbiorow, potem w kazdej linii rozmiar i tokeny */
int wczytaj_zbiory(const char *nazwa, zbiory_t *z)
{
  FILE *f;
  int i, j, n, r, pojemnosc;
  long long t;

  f = fopen(nazwa, "r");
  if(f==NULL){
    printf("Nie mozna otworzyc pliku %s\n", nazwa);
    return 1;
  }
  if(fscanf(f, "%d", &n)!=1 || n<0){
    printf("Bledna liczba zbiorow w pliku %s\n", nazwa);
    fclose(f);
    return 1;
  }
  z->n = n;
  z->rozm = malloc((n+1)*sizeof(int));
  z->pocz = malloc((n+1)*sizeof(int));
  pojemnosc = 1024;
  z->elem = malloc(pojemnosc*sizeof(unsigned int));
  z->pocz[0] = 0;
  for(i=0;i<n;i++){
    if(fscanf(f, "%d", &r)!=1 || r<0){
      printf("Bledny rozmiar zbioru %d w pliku %s\n", i, nazwa);
      fclose(f);
      usun_zbiory(z);
      return 1;
    }
    /* powiekszenie tablicy elementow */
    if(z->pocz[i]+r > pojemnosc){
      while(z->pocz[i]+r > pojemnosc) pojemnosc *= 2;
      z->elem = realloc(z->elem, pojemnosc*sizeof(unsigned int));
    }
    for(j=0;j<r;j++){
      if(fscanf(f, "%lld", &t)!=1 || t<0 || t>=P_HASZ){
        printf("Bledny token w zbiorze %d w pliku %s\n", i, nazwa);
        fclose(f);
        usun_zbiory(z);
        return 1;
      }
      z->elem[z->pocz[i]+j] = (unsigned int)t;
    }
    z->rozm[i] = r;
    z->pocz[i+1] = z->pocz[i] + r;
  }
  fclose(f);
  return 0;
}

/* plik: liczba par, potem w kazdej linii dwa numery zbiorow */
int wczytaj_pary(const char *nazwa, int n, int *p, int **pary)
{
  FILE *f;
  int i;

  f = fopen(nazwa, "r");
  if(f==NULL){
    printf("Nie mozna otworzyc pliku %s\n", nazwa);
    return 1;
  }
  if(fscanf(f, "%d", p)!=1 || *p<0){
    printf("Bledna liczba par w pliku %s\n", nazwa);
    fclose(f);
    return 1;
  }
  *pary = malloc((2*(*p)+1)*sizeof(int));
  for(i=0;i<2*(*p);i++){
    if(fscanf(f, "%d", &(*pary)[i])!=1 || (*pary)[i]<0 || (*pary)[i]>=n){
      printf("Bledna para %d w pliku %s\n", i/2, nazwa);
      fclose(f);
      free(*pary);
      return 1;
    }
  }
  fclose(f);
  return 0;
}

/* zwolnienie pamieci */
void usun_zbiory(zbiory_t *z)
{
  free(z->rozm);
  free(z->pocz);
  free(z->elem);
}

/* porownanie dla qsort */
static int porownaj(const void *a, const void *b)
{
  unsigned int x = *(const unsigned int *)a;
  unsigned int y = *(const unsigned int *)b;

  return (x > y) - (x < y);
}

/* sortowanie i usuniecie powtorzen, zwraca nowy rozmiar */
int usun_duplikaty(unsigned int *x, int n)
{
  int i, m;

  if(n==0) return 0;
  qsort(x, n, sizeof(unsigned int), porownaj);
  m = 1;
  for(i=1;i<n;i++){
    if(x[i]!=x[m-1]) x[m++] = x[i];
  }
  return m;
}

/* wspolczynniki funkcji h_i(x) = (a_i*x + b_i) mod P_HASZ */
void wspolczynniki(int k, unsigned long long ziarno, unsigned int *a, unsigned int *b)
{
  int i;
  unsigned long long stan = ziarno;

  for(i=0;i<k;i++){
    a[i] = 1 + (unsigned int)(losuj(&stan) % (P_HASZ-1));
    b[i] = (unsigned int)(losuj(&stan) % P_HASZ);
  }
}

/* szkic MinHash: minimum kazdej funkcji skrotu po elementach */
void szkic(const unsigned int *x, int n, int k, const unsigned int *a,
           const unsigned int *b, unsigned int *s)
{
  int i, j;
  unsigned int h;

  for(i=0;i<k;i++) s[i] = PUSTY;
  for(j=0;j<n;j++){
    for(i=0;i<k;i++){
      h = (unsigned int)(((unsigned long long)a[i]*x[j] + b[i]) % P_HASZ);
      if(h < s[i]) s[i] = h;
    }
  }
}

/* dokladne podobienstwo Jaccarda zbiorow posortowanych */
double jaccard(const unsigned int *x, int nx, const unsigned int *y, int ny)
{
  int i = 0, j = 0, wsp = 0;

  /* umowa: dwa zbiory puste */
  if(nx==0 && ny==0) return 1.0;
  while(i<nx && j<ny){
    if(x[i]<y[j]) i++;
    else if(x[i]>y[j]) j++;
    else{
      wsp++;
      i++;
      j++;
    }
  }
  return (double)wsp / (nx + ny - wsp);
}

/* udzial zgodnych pozycji szkicow */
double podobienstwo_szkicow(const unsigned int *s, const unsigned int *t, int k)
{
  int i, zgodne = 0;

  for(i=0;i<k;i++){
    if(s[i]==t[i]) zgodne++;
  }
  return (double)zgodne / k;
}

/* kolejnosc listy: wieksze podobienstwo, przy rownym mniejszy numer */
static int lepszy(int dok, double podob, kandydat_t c)
{
  return podob > c.podob || (podob == c.podob && dok < c.dok);
}

/* wstawienie do posortowanej listy top-k */
void wstaw(kandydat_t *lista, int *ile, int top, int dok, double podob)
{
  int i;

  if(*ile==top && !lepszy(dok, podob, lista[top-1])) return;
  if(*ile<top) (*ile)++;
  i = *ile - 1;
  while(i>0 && lepszy(dok, podob, lista[i-1])){
    lista[i] = lista[i-1];
    i--;
  }
  lista[i].dok = dok;
  lista[i].podob = podob;
}

/* linia: zbior1 zbior2 jaccard minhash */
int zapisz_pary(const char *nazwa, int p, const int *pary,
                const double *jac, const double *mh)
{
  FILE *f;
  int i;

  f = fopen(nazwa, "w");
  if(f==NULL){
    printf("Nie mozna zapisac pliku %s\n", nazwa);
    return 1;
  }
  for(i=0;i<p;i++){
    fprintf(f, "%d %d %.6f %.6f\n", pary[2*i], pary[2*i+1], jac[i], mh[i]);
  }
  fclose(f);
  return 0;
}

/* linia: zapytanie miejsce dok_jaccard jaccard dok_minhash minhash */
int zapisz_topk(const char *nazwa, int q, int top, const kandydat_t *lj,
                const kandydat_t *lm, const int *ile)
{
  FILE *f;
  int i, r;
  const kandydat_t *a, *b;

  f = fopen(nazwa, "w");
  if(f==NULL){
    printf("Nie mozna zapisac pliku %s\n", nazwa);
    return 1;
  }
  for(i=0;i<q;i++){
    for(r=0;r<ile[i];r++){
      a = lj + (size_t)i*top + r;
      b = lm + (size_t)i*top + r;
      fprintf(f, "%d %d %d %.6f %d %.6f\n", i, r+1, a->dok, a->podob, b->dok, b->podob);
    }
  }
  fclose(f);
  return 0;
}
