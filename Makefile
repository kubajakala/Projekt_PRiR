# kompilator c
CCOMP = gcc

# konsolidator
LOADER = gcc

# kompilator i konsolidator MPI
MPICOMP = mpicc

# opcje optymalizacji (te same dla wszystkich wersji)
OPT = -O2

# wszystkie programy
all: generator jaccard_sekw jaccard_omp jaccard_mpi

# zaleznosci i komendy
generator: generator.o wspolne.o
	$(LOADER) $(OPT) generator.o wspolne.o -o generator

generator.o: generator.c wspolne.h
	$(CCOMP) -c $(OPT) generator.c

wspolne.o: wspolne.c wspolne.h
	$(CCOMP) -c $(OPT) wspolne.c

jaccard_sekw: jaccard_sekw.o wspolne.o
	$(LOADER) $(OPT) jaccard_sekw.o wspolne.o -o jaccard_sekw

jaccard_sekw.o: jaccard_sekw.c wspolne.h
	$(CCOMP) -c $(OPT) jaccard_sekw.c

jaccard_omp: jaccard_omp.o wspolne.o
	$(LOADER) $(OPT) -fopenmp jaccard_omp.o wspolne.o -o jaccard_omp

jaccard_omp.o: jaccard_omp.c wspolne.h
	$(CCOMP) -c $(OPT) -fopenmp jaccard_omp.c

jaccard_mpi: jaccard_mpi.o wspolne.o
	$(MPICOMP) $(OPT) jaccard_mpi.o wspolne.o -o jaccard_mpi

jaccard_mpi.o: jaccard_mpi.c wspolne.h
	$(MPICOMP) -c $(OPT) jaccard_mpi.c

clean:
	rm -f *.o
