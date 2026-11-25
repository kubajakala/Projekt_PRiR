#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
#include "wspolne.h"

/* fazy pomiaru czasu */
#define T_WCZYT 0
#define T_KORPUS 1
#define T_DUPL 2
#define T_SZK 3
#define T_REPL_ZB 4
#define T_REPL_SZK 5
#define T_ROZESL 6
#define T_DOKL 7
#define T_MH 8
#define T_ZBIER 9
#define T_ZAPIS 10
#define FAZY 11

int main(int argc, char **argv)
{
  parametry_t par;
  zbiory_t z = {0, NULL, NULL, NULL}, zq = {0, NULL, NULL, NULL}, lok, w;
  int rank, size, r, i, j, d, e, q, k, n, m, lp, blad, ilem;
  int p = 0, nq = 0, tq = 0;
  int *pary = NULL, *pary_lok = NULL, *od, *pod = NULL, *ile_r, *ile_x, *przes_x, *ile = NULL;
  unsigned int *a, *b, *szk_lok, *szk = NULL, *szkq = NULL;
  double *jac = NULL, *mh = NULL, *jac_lok = NULL, *mh_lok = NULL;
  kandydat_t *lj_lok = NULL, *lm_lok = NULL, *wsz_j = NULL, *wsz_m = NULL;
  kandydat_t *lj = NULL, *lm = NULL, c;
  long tokeny_lok = 0, tokeny = 0;
  double tf[FAZY], tmax[FAZY], t0, t, t_calk = 0.0;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if(argumenty(argc, argv, &par, rank==0)){
    MPI_Finalize();
    return 1;
  }
  k = par.k;
  for(i=0;i<FAZY;i++) tf[i] = 0.0;
  MPI_Barrier(MPI_COMM_WORLD);
  t0 = czas();

  /* wczytanie danych (proces 0) */
  blad = 0;
  if(rank==0){
    t = czas();
    blad = wczytaj_zbiory(par.korpus, &z);
    if(!blad && par.tryb==PARY) blad = wczytaj_pary(par.drugi, z.n, &p, &pary);
    if(!blad && par.tryb==TOPK) blad = wczytaj_zbiory(par.drugi, &zq);
    tf[T_WCZYT] = czas() - t;
  }
  MPI_Bcast(&blad, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if(blad){
    MPI_Finalize();
    return 1;
  }

  /* rozeslanie fragmentow korpusu: dokumenty od[r]..od[r+1]-1 dla procesu r */
  t = czas();
  n = z.n;
  MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
  od = malloc((size+1)*sizeof(int));
  ile_r = malloc(size*sizeof(int));
  ile_x = malloc(size*sizeof(int));
  przes_x = malloc(size*sizeof(int));
  for(r=0;r<=size;r++) od[r] = (int)((long long)n*r/size);
  for(r=0;r<size;r++) ile_r[r] = od[r+1] - od[r];
  lok.n = ile_r[rank];
  lok.rozm = malloc((lok.n+1)*sizeof(int));
  lok.pocz = malloc((lok.n+1)*sizeof(int));
  MPI_Scatterv(z.rozm, ile_r, od, MPI_INT, lok.rozm, lok.n, MPI_INT, 0, MPI_COMM_WORLD);
  lok.pocz[0] = 0;
  for(i=0;i<lok.n;i++) lok.pocz[i+1] = lok.pocz[i] + lok.rozm[i];
  lok.elem = malloc((lok.pocz[lok.n]+1)*sizeof(unsigned int));
  if(rank==0){
    for(r=0;r<size;r++){
      ile_x[r] = z.pocz[od[r+1]] - z.pocz[od[r]];
      przes_x[r] = z.pocz[od[r]];
    }
  }
  MPI_Scatterv(z.elem, ile_x, przes_x, MPI_UNSIGNED, lok.elem, lok.pocz[lok.n],
               MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  if(rank==0) usun_zbiory(&z);
  tf[T_KORPUS] = czas() - t;

  /* usuwanie duplikatow we fragmencie */
  t = czas();
  for(i=0;i<lok.n;i++){
    lok.rozm[i] = usun_duplikaty(lok.elem + lok.pocz[i], lok.rozm[i]);
  }
  tf[T_DUPL] = czas() - t;
  for(i=0;i<lok.n;i++) tokeny_lok += lok.rozm[i];

  /* szkice fragmentu */
  t = czas();
  a = malloc(k*sizeof(unsigned int));
  b = malloc(k*sizeof(unsigned int));
  wspolczynniki(k, par.ziarno, a, b);
  szk_lok = malloc(((size_t)lok.n*k+1)*sizeof(unsigned int));
  for(i=0;i<lok.n;i++){
    szkic(lok.elem + lok.pocz[i], lok.rozm[i], k, a, b, szk_lok + (size_t)i*k);
  }
  tf[T_SZK] = czas() - t;

  if(par.tryb==PARY){
    /* replikacja zbiorow po usunieciu duplikatow */
    t = czas();
    w.n = n;
    w.rozm = malloc((n+1)*sizeof(int));
    w.pocz = malloc((n+1)*sizeof(int));
    MPI_Allgatherv(lok.rozm, lok.n, MPI_INT, w.rozm, ile_r, od, MPI_INT, MPI_COMM_WORLD);
    w.pocz[0] = 0;
    for(i=0;i<n;i++) w.pocz[i+1] = w.pocz[i] + w.rozm[i];
    /* spakowanie fragmentu bez luk po duplikatach */
    m = 0;
    for(i=0;i<lok.n;i++){
      memmove(lok.elem + m, lok.elem + lok.pocz[i], lok.rozm[i]*sizeof(unsigned int));
      m += lok.rozm[i];
    }
    for(r=0;r<size;r++){
      ile_x[r] = w.pocz[od[r+1]] - w.pocz[od[r]];
      przes_x[r] = w.pocz[od[r]];
    }
    w.elem = malloc((w.pocz[n]+1)*sizeof(unsigned int));
    MPI_Allgatherv(lok.elem, m, MPI_UNSIGNED, w.elem, ile_x, przes_x, MPI_UNSIGNED,
                   MPI_COMM_WORLD);
    tf[T_REPL_ZB] = czas() - t;

    /* replikacja szkicow */
    t = czas();
    for(r=0;r<size;r++){
      ile_x[r] = ile_r[r]*k;
      przes_x[r] = od[r]*k;
    }
    szk = malloc(((size_t)n*k+1)*sizeof(unsigned int));
    MPI_Allgatherv(szk_lok, lok.n*k, MPI_UNSIGNED, szk, ile_x, przes_x, MPI_UNSIGNED,
                   MPI_COMM_WORLD);
    tf[T_REPL_SZK] = czas() - t;

    /* rozeslanie par: pary pod[r]..pod[r+1]-1 dla procesu r */
    t = czas();
    MPI_Bcast(&p, 1, MPI_INT, 0, MPI_COMM_WORLD);
    pod = malloc((size+1)*sizeof(int));
    for(r=0;r<=size;r++) pod[r] = (int)((long long)p*r/size);
    for(r=0;r<size;r++){
      ile_x[r] = 2*(pod[r+1] - pod[r]);
      przes_x[r] = 2*pod[r];
    }
    lp = pod[rank+1] - pod[rank];
    pary_lok = malloc((2*lp+1)*sizeof(int));
    MPI_Scatterv(pary, ile_x, przes_x, MPI_INT, pary_lok, 2*lp, MPI_INT, 0, MPI_COMM_WORLD);
    tf[T_ROZESL] = czas() - t;

    jac_lok = malloc((lp+1)*sizeof(double));
    mh_lok = malloc((lp+1)*sizeof(double));

    /* porownania dokladne */
    t = czas();
    for(i=0;i<lp;i++){
      d = pary_lok[2*i];
      e = pary_lok[2*i+1];
      jac_lok[i] = jaccard(w.elem + w.pocz[d], w.rozm[d], w.elem + w.pocz[e], w.rozm[e]);
    }
    tf[T_DOKL] = czas() - t;

    /* porownania szkicow */
    t = czas();
    for(i=0;i<lp;i++){
      d = pary_lok[2*i];
      e = pary_lok[2*i+1];
      mh_lok[i] = podobienstwo_szkicow(szk + (size_t)d*k, szk + (size_t)e*k, k);
    }
    tf[T_MH] = czas() - t;

    /* zebranie wynikow w procesie 0 */
    t = czas();
    for(r=0;r<size;r++){
      ile_x[r] = pod[r+1] - pod[r];
      przes_x[r] = pod[r];
    }
    if(rank==0){
      jac = malloc((p+1)*sizeof(double));
      mh = malloc((p+1)*sizeof(double));
    }
    MPI_Gatherv(jac_lok, lp, MPI_DOUBLE, jac, ile_x, przes_x, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gatherv(mh_lok, lp, MPI_DOUBLE, mh, ile_x, przes_x, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    tf[T_ZBIER] = czas() - t;
    usun_zbiory(&w);
  }
  else{
    /* rozeslanie zapytan */
    t = czas();
    if(rank==0){
      nq = zq.n;
      tq = zq.pocz[nq];
    }
    MPI_Bcast(&nq, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&tq, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(rank!=0){
      zq.n = nq;
      zq.rozm = malloc((nq+1)*sizeof(int));
      zq.pocz = malloc((nq+1)*sizeof(int));
      zq.elem = malloc((tq+1)*sizeof(unsigned int));
    }
    MPI_Bcast(zq.rozm, nq, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(zq.elem, tq, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    zq.pocz[0] = 0;
    for(i=0;i<nq;i++) zq.pocz[i+1] = zq.pocz[i] + zq.rozm[i];
    tf[T_ROZESL] = czas() - t;

    /* zapytania: duplikaty i szkice w kazdym procesie */
    t = czas();
    for(i=0;i<nq;i++){
      zq.rozm[i] = usun_duplikaty(zq.elem + zq.pocz[i], zq.rozm[i]);
    }
    tf[T_DUPL] += czas() - t;
    t = czas();
    szkq = malloc(((size_t)nq*k+1)*sizeof(unsigned int));
    for(i=0;i<nq;i++){
      szkic(zq.elem + zq.pocz[i], zq.rozm[i], k, a, b, szkq + (size_t)i*k);
    }
    tf[T_SZK] += czas() - t;

    /* lokalne listy top-k, puste pozycje maja dok = -1 */
    lj_lok = malloc(((size_t)nq*par.top+1)*sizeof(kandydat_t));
    lm_lok = malloc(((size_t)nq*par.top+1)*sizeof(kandydat_t));
    for(i=0;i<nq*par.top;i++){
      lj_lok[i].dok = -1;
      lj_lok[i].podob = 0.0;
      lm_lok[i] = lj_lok[i];
    }

    /* top-k dokladne na fragmencie */
    t = czas();
    for(q=0;q<nq;q++){
      ilem = 0;
      for(d=0;d<lok.n;d++){
        wstaw(lj_lok + (size_t)q*par.top, &ilem, par.top, od[rank]+d,
              jaccard(zq.elem + zq.pocz[q], zq.rozm[q], lok.elem + lok.pocz[d], lok.rozm[d]));
      }
    }
    tf[T_DOKL] = czas() - t;

    /* top-k ze szkicow na fragmencie */
    t = czas();
    for(q=0;q<nq;q++){
      ilem = 0;
      for(d=0;d<lok.n;d++){
        wstaw(lm_lok + (size_t)q*par.top, &ilem, par.top, od[rank]+d,
              podobienstwo_szkicow(szkq + (size_t)q*k, szk_lok + (size_t)d*k, k));
      }
    }
    tf[T_MH] = czas() - t;

    /* zebranie list w procesie 0 i scalenie */
    t = czas();
    m = nq*par.top*sizeof(kandydat_t);
    if(rank==0){
      wsz_j = malloc((size_t)size*m+1);
      wsz_m = malloc((size_t)size*m+1);
    }
    MPI_Gather(lj_lok, m, MPI_BYTE, wsz_j, m, MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Gather(lm_lok, m, MPI_BYTE, wsz_m, m, MPI_BYTE, 0, MPI_COMM_WORLD);
    if(rank==0){
      lj = malloc(((size_t)nq*par.top+1)*sizeof(kandydat_t));
      lm = malloc(((size_t)nq*par.top+1)*sizeof(kandydat_t));
      ile = malloc((nq+1)*sizeof(int));
      for(q=0;q<nq;q++){
        ile[q] = 0;
        ilem = 0;
        for(r=0;r<size;r++){
          for(j=0;j<par.top;j++){
            c = wsz_j[((size_t)r*nq + q)*par.top + j];
            if(c.dok>=0) wstaw(lj + (size_t)q*par.top, &ile[q], par.top, c.dok, c.podob);
            c = wsz_m[((size_t)r*nq + q)*par.top + j];
            if(c.dok>=0) wstaw(lm + (size_t)q*par.top, &ilem, par.top, c.dok, c.podob);
          }
        }
      }
    }
    tf[T_ZBIER] = czas() - t;
  }

  /* zapis wynikow (proces 0) */
  if(rank==0){
    t = czas();
    if(par.tryb==PARY) blad = zapisz_pary(par.wynik, p, pary, jac, mh);
    else blad = zapisz_topk(par.wynik, nq, par.top, lj, lm, ile);
    tf[T_ZAPIS] = czas() - t;
    t_calk = czas() - t0;
  }

  /* czasy faz: maksimum po procesach */
  MPI_Reduce(tf, tmax, FAZY, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  MPI_Reduce(&tokeny_lok, &tokeny, 1, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

  if(rank==0 && !blad){
    if(par.tryb==PARY){
      printf("tryb: pary, zbiory: %d, pary: %d, k: %d, ziarno: %llu, procesy: %d\n",
             n, p, k, par.ziarno, size);
    }
    else{
      printf("tryb: topk, zbiory: %d, zapytania: %d, top: %d, k: %d, ziarno: %llu, procesy: %d\n",
             n, nq, par.top, k, par.ziarno, size);
    }
    printf("pamiec zbiorow: %ld B, pamiec szkicow: %ld B\n",
           tokeny*(long)sizeof(unsigned int), (long)n*k*(long)sizeof(unsigned int));
    printf("czas wczytania:            %f s\n", tmax[T_WCZYT]);
    printf("czas rozeslania korpusu:   %f s\n", tmax[T_KORPUS]);
    printf("czas usuwania duplikatow:  %f s\n", tmax[T_DUPL]);
    printf("czas budowy szkicow:       %f s\n", tmax[T_SZK]);
    if(par.tryb==PARY){
      printf("czas replikacji zbiorow:   %f s\n", tmax[T_REPL_ZB]);
      printf("czas replikacji szkicow:   %f s\n", tmax[T_REPL_SZK]);
      printf("czas rozeslania par:       %f s\n", tmax[T_ROZESL]);
    }
    else{
      printf("czas rozeslania zapytan:   %f s\n", tmax[T_ROZESL]);
    }
    printf("czas porownan dokladnych:  %f s\n", tmax[T_DOKL]);
    printf("czas porownan szkicow:     %f s\n", tmax[T_MH]);
    printf("czas zebrania wynikow:     %f s\n", tmax[T_ZBIER]);
    printf("czas zapisu:               %f s\n", tmax[T_ZAPIS]);
    printf("czas calkowity:            %f s\n", t_calk);
    printf("(czasy faz: maksimum po procesach)\n");
  }

  usun_zbiory(&lok);
  if(par.tryb==TOPK) usun_zbiory(&zq);
  free(od);
  free(pod);
  free(ile_r);
  free(ile_x);
  free(przes_x);
  free(a);
  free(b);
  free(szk_lok);
  free(szk);
  free(szkq);
  free(pary);
  free(pary_lok);
  free(jac);
  free(mh);
  free(jac_lok);
  free(mh_lok);
  free(lj_lok);
  free(lm_lok);
  free(wsz_j);
  free(wsz_m);
  free(lj);
  free(lm);
  free(ile);
  MPI_Finalize();
  return blad;
}
