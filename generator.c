#include <stdio.h>
#include <stdlib.h>
#include "wspolne.h"

/* liczba losowa z przedzialu [od, doo] */
int losuj_int(unsigned long long *stan, int od, int doo)
{
  return od + (int)(losuj(stan) % (unsigned long long)(doo - od + 1));
}

/* liczba losowa z przedzialu [0, 1) */
double losuj_ulamek(unsigned long long *stan)
{
  return (losuj(stan) >> 11) * (1.0 / 9007199254740992.0);
}

/* otwarcie pliku prefiks_nazwa.txt */
FILE *otworz(const char *prefiks, const char *nazwa)
{
  char plik[1024];
  FILE *f;

  snprintf(plik, sizeof(plik), "%s_%s.txt", prefiks, nazwa);
  f = fopen(plik, "w");
  if(f==NULL) printf("Nie mozna zapisac pliku %s\n", plik);
  return f;
}

int main(int argc, char **argv)
{
  int n, q, p, smin, smax, i, j, w, s, m, c, r, d, e;
  int *rozm_wz;
  unsigned int **wz, *dok, tmp;
  double umin, umax, u;
  unsigned long long ziarno, stan;
  FILE *f;

  if(argc!=10){
    printf("Uzycie: %s <zbiory> <zapytania> <pary> <rozmiar_min> <rozmiar_max> "
           "<udzial_min> <udzial_max> <ziarno> <prefiks>\n", argv[0]);
    printf("zapisuje prefiks_korpus.txt, prefiks_zapytania.txt, prefiks_pary.txt\n");
    return 1;
  }
  n = atoi(argv[1]);
  q = atoi(argv[2]);
  p = atoi(argv[3]);
  smin = atoi(argv[4]);
  smax = atoi(argv[5]);
  umin = atof(argv[6]);
  umax = atof(argv[7]);
  ziarno = strtoull(argv[8], NULL, 10);
  if(n<0 || q<1 || p<0 || smin<0 || smax<smin || umin<0 || umax<umin || umax>1
     || (p>0 && n==0)){
    printf("Bledne parametry\n");
    return 1;
  }
  stan = ziarno;

  /* wzorce = zapytania, tokeny losowe */
  wz = malloc(q*sizeof(unsigned int *));
  rozm_wz = malloc(q*sizeof(int));
  for(w=0;w<q;w++){
    rozm_wz[w] = losuj_int(&stan, smin, smax);
    wz[w] = malloc((rozm_wz[w]+1)*sizeof(unsigned int));
    for(j=0;j<rozm_wz[w];j++) wz[w][j] = (unsigned int)(losuj(&stan) % P_HASZ);
  }

  /* korpus: zbior i ma wzorzec i%q */
  f = otworz(argv[9], "korpus");
  if(f==NULL) return 1;
  fprintf(f, "%d\n", n);
  dok = malloc((smax+1)*sizeof(unsigned int));
  for(i=0;i<n;i++){
    w = i % q;
    s = losuj_int(&stan, smin, smax);
    u = umin + (umax - umin) * losuj_ulamek(&stan);
    m = s < rozm_wz[w] ? s : rozm_wz[w];
    c = (int)(u * m + 0.5);
    /* czesc wspolna: c roznych elementow wzorca (czesciowe tasowanie) */
    for(j=0;j<c;j++){
      r = losuj_int(&stan, j, rozm_wz[w]-1);
      tmp = wz[w][j];
      wz[w][j] = wz[w][r];
      wz[w][r] = tmp;
      dok[j] = wz[w][j];
    }
    /* reszta: tokeny losowe */
    for(j=c;j<s;j++) dok[j] = (unsigned int)(losuj(&stan) % P_HASZ);
    fprintf(f, "%d", s);
    for(j=0;j<s;j++) fprintf(f, " %u", dok[j]);
    fprintf(f, "\n");
  }
  fclose(f);

  /* zapytania */
  f = otworz(argv[9], "zapytania");
  if(f==NULL) return 1;
  fprintf(f, "%d\n", q);
  for(w=0;w<q;w++){
    fprintf(f, "%d", rozm_wz[w]);
    for(j=0;j<rozm_wz[w];j++) fprintf(f, " %u", wz[w][j]);
    fprintf(f, "\n");
  }
  fclose(f);

  /* pary: zbiory z tym samym wzorcem */
  f = otworz(argv[9], "pary");
  if(f==NULL) return 1;
  fprintf(f, "%d\n", p);
  for(i=0;i<p;i++){
    d = losuj_int(&stan, 0, n-1);
    w = d % q;
    m = (n - 1 - w) / q + 1;
    e = w + q * losuj_int(&stan, 0, m-1);
    fprintf(f, "%d %d\n", d, e);
  }
  fclose(f);

  printf("zbiory: %d, zapytania: %d, pary: %d, rozmiar: %d-%d, udzial: %.2f-%.2f, ziarno: %llu\n",
         n, q, p, smin, smax, umin, umax, ziarno);

  for(w=0;w<q;w++) free(wz[w]);
  free(wz);
  free(rozm_wz);
  free(dok);
  return 0;
}
