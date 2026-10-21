#----------------------------------------------------------------------
# CC3069 - Proyecto 02
# Compila la version secuencial y la paralela (Open MPI) de la busqueda
# de claves AES-128-CBC. Ambas comparten comun.c.
#----------------------------------------------------------------------
CC = gcc
MPICC = mpicc
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDLIBS = -lcrypto

PROGRAMAS = busqueda_secuencial busqueda_mpi

all: $(PROGRAMAS)

busqueda_secuencial: busqueda_secuencial.c comun.c comun.h
	$(CC) $(CFLAGS) busqueda_secuencial.c comun.c -o $@ $(LDLIBS)

busqueda_mpi: busqueda_mpi.c comun.c comun.h
	$(MPICC) $(CFLAGS) busqueda_mpi.c comun.c -o $@ $(LDLIBS)

datos/mensaje.txt:
	mkdir -p datos
	printf 'Esta es una prueba de proyecto 2' > datos/mensaje.txt

datos: datos/mensaje.txt

clean:
	rm -f $(PROGRAMAS)

.PHONY: all clean datos
