#!/bin/bash
#----------------------------------------------------------------------
# CC3069 - Proyecto 02
# Bitacora de pruebas (incisos 2 y 3, Anexo 2). Compila y ejecuta cada
# prueba con 4 procesos, mostrando el comando antes de su salida, y guarda
# todo en resultados/ para el informe y las capturas.
#
# Las busquedas que tardarian anos se cortan con -t LIMITE; el programa
# estima el tiempo hasta la clave a partir de su posicion en el recorrido.
#
# Uso: ./bitacora.sh [limite_segundos]      (por defecto 10)
#----------------------------------------------------------------------
LIMITE=${1:-10}
SALIDA=resultados
CLAVE_A=123456
CLAVE_B=18014398509481984     # 2^56 / 4
CLAVE_C=18014398509481985     # 2^56 / 4 + 1
CLAVE_X1=3377699720527877     # 3 * 2^50 + 5: a mitad de un bloque
CLAVE_X2=18014398509481983    # 2^54 - 1: ultima clave del proceso 0

mkdir -p "$SALIDA"

# Muestra el comando, lo ejecuta y guarda ambos en resultados/<nombre>.txt.
correr() {
    local nombre=$1
    shift
    {
        echo "\$ $*"
        "$@" 2>&1
        echo
    } | tee "$SALIDA/$nombre.txt"
}

correr 00_compilacion make -B

# Inciso 2: estrategia base (ciclica) con 4 procesos y version secuencial.
correr 01_secuencial_a ./busqueda_secuencial -k $CLAVE_A
correr 02_secuencial_b ./busqueda_secuencial -k $CLAVE_B -t "$LIMITE"
correr 03_secuencial_c ./busqueda_secuencial -k $CLAVE_C -t "$LIMITE"

for clave in A B C X1 X2; do
    var=CLAVE_$clave
    valor=${!var}
    sufijo=$(echo "$clave" | tr 'A-Z' 'a-z')

    correr "10_ciclica_$sufijo" \
        mpirun -np 4 ./busqueda_mpi -k "$valor" -s ciclica -t "$LIMITE"

    # Inciso 3: estrategias alternativas con las mismas claves.
    correr "20_bloques_$sufijo" \
        mpirun -np 4 ./busqueda_mpi -k "$valor" -s bloques -t "$LIMITE"
    correr "30_cantor16_$sufijo" \
        mpirun -np 4 ./busqueda_mpi -k "$valor" -s cantor -K 16 -t "$LIMITE"
    correr "40_cantor256_$sufijo" \
        mpirun -np 4 ./busqueda_mpi -k "$valor" -s cantor -K 256 -t "$LIMITE"
done

# Resumen: tiempo real o estimado de cada prueba.
{
    printf "| %-22s | %-14s | %-10s | %-16s |\n" "Prueba" "Resultado" "Tiempo (s)" "Estimado (s)"
    printf "|%s|%s|%s|%s|\n" "------------------------" "----------------" \
        "------------" "------------------"
    for f in "$SALIDA"/[0-4]*.txt; do
        nombre=$(basename "$f" .txt)
        [ "$nombre" = "00_compilacion" ] && continue
        awk -v n="$nombre" '
            /^Resultado: ENCONTRADA/ { r = "encontrada" }
            /^Resultado: (LIMITE|no se encontro)/ { r = "limite -t" }
            /^Tiempo:/ { t = $2 }
            /^Tiempo estimado hasta la clave \(s\):/ { e = $NF }
            END { printf "| %-22s | %-14s | %10s | %16s |\n", n, r, t, (e == "" ? "-" : e) }
        ' "$f"
    done
} | tee "$SALIDA/resumen.md"
