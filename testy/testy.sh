#!/bin/bash
# testy poprawnosci, uruchomienie z katalogu projektu po make:
#   bash testy/testy.sh

T=testy
TMP=$(mktemp -d)
MPIRUN="mpirun --use-hwthread-cpus"
WATKI="1 2 3 4 12"
PROCESY="1 2 3 4 12"
BLEDY=0

# wynik testu
wynik() {
  if [ "$1" -eq 0 ]; then
    echo "OK    $2"
  else
    echo "BLAD  $2"
    BLEDY=$((BLEDY+1))
  fi
}

# minhash: przy J=0 i J=1 dokladnie, inaczej |MH-J| <= tol (male zbiory kolejnych liczb)
MH_MALE='{ j=$3+0; m=$4+0; if(j==0 || j==1){ if(m!=j) z=1 } else { d=m-j; if(d<0) d=-d; if(d>tol) z=1 } } END{ exit z }'

echo "== 1. referencja, tryb par (wyniki policzone recznie)"
for k in 1 1000; do
  ./jaccard_sekw pary $T/korpus.txt $T/pary.txt $k 1 $TMP/p.txt > /dev/null
  awk '{ print $1, $2, $3 }' $TMP/p.txt | cmp -s - $T/pary_oczekiwane.txt
  wynik $? "k=$k: dokladny Jaccard"
done
./jaccard_sekw pary $T/korpus.txt $T/pary.txt 1 1 $TMP/p.txt > /dev/null
awk -v tol=1 "$MH_MALE" $TMP/p.txt
wynik $? "k=1: MinHash (J=0 i J=1 dokladnie)"
./jaccard_sekw pary $T/korpus.txt $T/pary.txt 1000 1 $TMP/p.txt > /dev/null
awk -v tol=0.1 "$MH_MALE" $TMP/p.txt
wynik $? "k=1000: MinHash (J=0 i J=1 dokladnie, inne +-0.1)"

echo "== 2. referencja, tryb top-k (ranking policzony recznie)"
./jaccard_sekw topk $T/korpus.txt $T/zapytania.txt 10 1000 1 $TMP/t10.txt > /dev/null
awk '{ print $1, $2, $3, $4 }' $TMP/t10.txt | cmp -s - $T/topk_oczekiwane.txt
wynik $? "top=10 (wiecej niz zbiorow): ranking dokladny"
./jaccard_sekw topk $T/korpus.txt $T/zapytania.txt 3 1000 1 $TMP/t3.txt > /dev/null
awk '$2<=3' $TMP/t10.txt | cmp -s - $TMP/t3.txt
wynik $? "top=3: poczatek pelnej listy"
# lista MinHash: kazdy zbior raz, malejaco (remis: mniejszy numer), wartosci jak w MH_MALE
awk 'NR==FNR { J[$1" "$3]=$4+0; next }
     { key=$1" "$5; m=$6+0
       if(!(key in J) || seen[key]++) z=1
       j=J[key]
       if(j==0 || j==1){ if(m!=j) z=1 } else { d=m-j; if(d<0) d=-d; if(d>0.1) z=1 }
       if($2>1 && (m>pm || (m==pm && $5+0<pd))) z=1
       pm=m; pd=$5+0 }
     END { exit z }' $T/topk_oczekiwane.txt $TMP/t10.txt
wynik $? "top=10: ranking MinHash"

echo "== 3. referencja, blad MinHash na danych z generatora"
./generator 2000 20 5000 0 300 0 1 7 $TMP/g > /dev/null
./jaccard_sekw pary $TMP/g_korpus.txt $TMP/g_pary.txt 256 1 $TMP/gp.txt > /dev/null
awk '{ j=$3+0; d=$4-j; if(d<0) d=-d; s=4*sqrt(j*(1-j)/256); n++; if(d<=s+1e-9) w++ }
     END { printf "      %d z %d par w granicy 4 sigma\n", w, n; exit !(w>=0.99*n) }' $TMP/gp.txt
wynik $? "k=256: co najmniej 99% par z |MH-J| <= 4*sqrt(J(1-J)/k)"

echo "== 4. OpenMP i MPI kontra referencja (pliki wynikow identyczne)"
PRZYPADKI=(
  "pary $T/korpus.txt $T/pary.txt 1000 1"
  "topk $T/korpus.txt $T/zapytania.txt 10 1000 1"
  "topk $T/korpus.txt $T/zapytania.txt 3 1 1"
  "pary $TMP/g_korpus.txt $TMP/g_pary.txt 1 3"
  "pary $TMP/g_korpus.txt $TMP/g_pary.txt 64 3"
  "topk $TMP/g_korpus.txt $TMP/g_zapytania.txt 5 64 3"
  "topk $TMP/g_korpus.txt $TMP/g_zapytania.txt 2005 16 3"
  "pary $T/pusty.txt $T/pusty.txt 8 1"
  "topk $T/pusty.txt $T/zapytania.txt 5 8 1"
)
for c in "${PRZYPADKI[@]}"; do
  ./jaccard_sekw $c $TMP/ref.txt > /dev/null
  z=0
  for w in $WATKI; do
    OMP_NUM_THREADS=$w ./jaccard_omp $c $TMP/omp.txt > /dev/null
    cmp -s $TMP/ref.txt $TMP/omp.txt || z=1
  done
  wynik $z "omp ($WATKI watkow): ${c//$TMP\//}"
  z=0
  for np in $PROCESY; do
    $MPIRUN -np $np ./jaccard_mpi $c $TMP/mpi.txt > /dev/null
    cmp -s $TMP/ref.txt $TMP/mpi.txt || z=1
  done
  wynik $z "mpi ($PROCESY procesow): ${c//$TMP\//}"
done

echo "== 5. bledne dane (program konczy sie kodem bledu)"
printf '1\n2 5 2147483647\n' > $TMP/zly_token.txt
printf '2\n3 1 2\n' > $TMP/uciety.txt
printf '1\n0 9\n' > $TMP/zla_para.txt
ZLE=(
  "pary $TMP/zly_token.txt $T/pusty.txt 8 1"
  "pary $TMP/uciety.txt $T/pusty.txt 8 1"
  "pary $T/korpus.txt $TMP/zla_para.txt 8 1"
  "pary $TMP/brak.txt $T/pusty.txt 8 1"
  "topk $T/korpus.txt $T/zapytania.txt 0 8 1"
  "pary $T/korpus.txt $T/pary.txt 0 1"
)
for c in "${ZLE[@]}"; do
  z=0
  ./jaccard_sekw $c $TMP/x.txt > /dev/null 2>&1 && z=1
  ./jaccard_omp $c $TMP/x.txt > /dev/null 2>&1 && z=1
  $MPIRUN -np 2 ./jaccard_mpi $c $TMP/x.txt > /dev/null 2>&1 && z=1
  wynik $z "sekw, omp, mpi: ${c//$TMP\//}"
done

rm -rf $TMP
echo "Liczba bledow: $BLEDY"
[ $BLEDY -eq 0 ]
