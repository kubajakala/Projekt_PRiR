#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "wspolne.h"

int main(int argc, char **argv)
{
  parametry_t par;
  zbiory_t z, zq;
  int i, d, e, q, k, ilem, p = 0, *pary = NULL, *ile = NULL;
  unsigned int *a, *b, *szk, *szkq = NULL;
  double *jac = NULL, *mh = NULL, *wyn = NULL;
  kandydat_t *lj = NULL, *lm = NULL;
  long tokeny = 0;
  double t0, t, t_wczyt, t_dupl, t_szk, t_dokl, t_mh, t_zapis, t_calk;

  if(argumenty(argc, argv, &par, 1)) return 1;
  k = par.k;
  t0 = czas();

  /* wczytanie danych */
  t = czas();
  if(wczytaj_zbiory(par.korpus, &z)) return 1;
  if(par.tryb==PARY){
    if(wczytaj_pary(par.drugi, z.n, &p, &pary)) return 1;
  }
  else{
    if(wczytaj_zbiory(par.drugi, &zq)) return 1;
  }
  t_wczyt = czas() - t;

  /* usuwanie duplikatow, dokumenty dla watkow */
  t = czas();
  #pragma omp parallel for schedule(dynamic)
  for(i=0;i<z.n;i++){
    z.rozm[i] = usun_duplikaty(z.elem + z.pocz[i], z.rozm[i]);
  }
  if(par.tryb==TOPK){
    #pragma omp parallel for schedule(dynamic)
    for(i=0;i<zq.n;i++){
      zq.rozm[i] = usun_duplikaty(zq.elem + zq.pocz[i], zq.rozm[i]);
    }
  }
  t_dupl = czas() - t;
  for(i=0;i<z.n;i++) tokeny += z.rozm[i];

  /* budowa szkicow, dokumenty dla watkow */
  t = czas();
  a = malloc(k*sizeof(unsigned int));
  b = malloc(k*sizeof(unsigned int));
  wspolczynniki(k, par.ziarno, a, b);
  szk = malloc((size_t)z.n*k*sizeof(unsigned int));
  #pragma omp parallel for schedule(dynamic)
  for(i=0;i<z.n;i++){
    szkic(z.elem + z.pocz[i], z.rozm[i], k, a, b, szk + (size_t)i*k);
  }
  if(par.tryb==TOPK){
    szkq = malloc((size_t)zq.n*k*sizeof(unsigned int));
    #pragma omp parallel for schedule(dynamic)
    for(i=0;i<zq.n;i++){
      szkic(zq.elem + zq.pocz[i], zq.rozm[i], k, a, b, szkq + (size_t)i*k);
    }
  }
  t_szk = czas() - t;

  if(par.tryb==PARY){
    jac = malloc((p+1)*sizeof(double));
    mh = malloc((p+1)*sizeof(double));

    /* porownania dokladne, pary dla watkow (rozne rozmiary) */
    t = czas();
    #pragma omp parallel for private(d,e) schedule(dynamic,64)
    for(i=0;i<p;i++){
      d = pary[2*i];
      e = pary[2*i+1];
      jac[i] = jaccard(z.elem + z.pocz[d], z.rozm[d], z.elem + z.pocz[e], z.rozm[e]);
    }
    t_dokl = czas() - t;

    /* porownania szkicow, pary dla watkow (stala praca k) */
    t = czas();
    #pragma omp parallel for private(d,e) schedule(static)
    for(i=0;i<p;i++){
      d = pary[2*i];
      e = pary[2*i+1];
      mh[i] = podobienstwo_szkicow(szk + (size_t)d*k, szk + (size_t)e*k, k);
    }
    t_mh = czas() - t;
  }
  else{
    lj = malloc(((size_t)zq.n*par.top+1)*sizeof(kandydat_t));
    lm = malloc(((size_t)zq.n*par.top+1)*sizeof(kandydat_t));
    ile = malloc((zq.n+1)*sizeof(int));
    wyn = malloc((z.n+1)*sizeof(double));

    /* top-k dokladne: podobienstwa rownolegle, wybor listy sekwencyjnie */
    t = czas();
    for(q=0;q<zq.n;q++){
      #pragma omp parallel for schedule(dynamic,64)
      for(d=0;d<z.n;d++){
        wyn[d] = jaccard(zq.elem + zq.pocz[q], zq.rozm[q], z.elem + z.pocz[d], z.rozm[d]);
      }
      ile[q] = 0;
      for(d=0;d<z.n;d++) wstaw(lj + (size_t)q*par.top, &ile[q], par.top, d, wyn[d]);
    }
    t_dokl = czas() - t;

    /* top-k ze szkicow */
    t = czas();
    for(q=0;q<zq.n;q++){
      #pragma omp parallel for schedule(static)
      for(d=0;d<z.n;d++){
        wyn[d] = podobienstwo_szkicow(szkq + (size_t)q*k, szk + (size_t)d*k, k);
      }
      ilem = 0;
      for(d=0;d<z.n;d++) wstaw(lm + (size_t)q*par.top, &ilem, par.top, d, wyn[d]);
    }
    t_mh = czas() - t;
  }

  /* zapis wynikow */
  t = czas();
  if(par.tryb==PARY){
    if(zapisz_pary(par.wynik, p, pary, jac, mh)) return 1;
  }
  else{
    if(zapisz_topk(par.wynik, zq.n, par.top, lj, lm, ile)) return 1;
  }
  t_zapis = czas() - t;
  t_calk = czas() - t0;

  if(par.tryb==PARY){
    printf("tryb: pary, zbiory: %d, pary: %d, k: %d, ziarno: %llu, watki: %d\n",
           z.n, p, k, par.ziarno, omp_get_max_threads());
  }
  else{
    printf("tryb: topk, zbiory: %d, zapytania: %d, top: %d, k: %d, ziarno: %llu, watki: %d\n",
           z.n, zq.n, par.top, k, par.ziarno, omp_get_max_threads());
  }
  printf("pamiec zbiorow: %ld B, pamiec szkicow: %ld B\n",
         tokeny*(long)sizeof(unsigned int), (long)z.n*k*(long)sizeof(unsigned int));
  printf("czas wczytania:            %f s\n", t_wczyt);
  printf("czas usuwania duplikatow:  %f s\n", t_dupl);
  printf("czas budowy szkicow:       %f s\n", t_szk);
  printf("czas porownan dokladnych:  %f s\n", t_dokl);
  printf("czas porownan szkicow:     %f s\n", t_mh);
  printf("czas zapisu:               %f s\n", t_zapis);
  printf("czas calkowity:            %f s\n", t_calk);

  usun_zbiory(&z);
  if(par.tryb==TOPK) usun_zbiory(&zq);
  free(a);
  free(b);
  free(szk);
  free(szkq);
  free(pary);
  free(jac);
  free(mh);
  free(lj);
  free(lm);
  free(ile);
  free(wyn);
  return 0;
}
