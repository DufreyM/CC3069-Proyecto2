#!/bin/bash
#----------------------------------------------------------------------
# CC3069 - Proyecto 02
# Mide el Speedup de la busqueda de clave AES-128-CBC: compara la version
# secuencial contra la paralela (Open MPI) con 2, 3 y 4 procesos.
#
#   Speedup    = T_secuencial / T_paralelo
#   Eficiencia = Speedup / numero de procesos
#
# Cada tiempo es el promedio de REPS ejecuciones. Primero verifica que
# ambas versiones recuperen la misma clave y el mismo mensaje.
#
# Uso: ./medir_speedup.sh [clave] [bits] [repeticiones]
#   ./medir_speedup.sh 123456 56 10    # prueba a) del enunciado
#   ./medir_speedup.sh 123456 24 10    # rango reducido para pruebas rapidas
#----------------------------------------------------------------------
KEY=${1:-123456}
BITS=${2:-56}
REPS=${3:-10}
SEQ=./busqueda_secuencial
PAR=./busqueda_mpi

if [ ! -x "$SEQ" ] || [ ! -x "$PAR" ]; then
    echo "Faltan los binarios. Ejecuta 'make' primero." >&2
    exit 1
fi

# Extrae el valor numerico de la linea "Tiempo:" de la salida.
tiempo() { awk '/^Tiempo:/ {print $2}'; }

# Extrae la clave encontrada y el texto recuperado (para la verificacion).
resultado() { awk '/^Clave encontrada:/ {print $3} /^Texto recuperado:/ {print}'; }

# Ejecuta con n procesos (n = 1 => secuencial) y devuelve su tiempo.
ejecutar() {
    if [ "$1" -eq 1 ]; then
        $SEQ -k "$KEY" -b "$BITS" | tiempo
    else
        mpirun -np "$1" $PAR -k "$KEY" -b "$BITS" | tiempo
    fi
}

echo "Escenario: clave $KEY, 2^$BITS candidatas, $REPS repeticiones"
echo

echo "Verificacion de resultados:"
seq_res=$($SEQ -k "$KEY" -b "$BITS" | resultado)
par_res=$(mpirun -np 4 $PAR -k "$KEY" -b "$BITS" | resultado)
echo "  Secuencial: $(echo $seq_res)"
echo "  MPI (4):    $(echo $par_res)"
if [ "$seq_res" = "$par_res" ]; then
    echo "  Ambas versiones recuperan la misma clave y mensaje."
else
    echo "  ERROR: los resultados no coinciden."
fi
echo

# Acumula los tiempos de cada configuracion sobre REPS repeticiones.
declare -A suma
for ((r = 1; r <= REPS; r++)); do
    printf "Repeticion %d/%d\r" "$r" "$REPS"
    for n in 1 2 3 4; do
        t=$(ejecutar "$n")
        suma[$n]=$(awk -v a="${suma[$n]:-0}" -v b="$t" 'BEGIN { printf "%.6f", a + b }')
    done
done
echo

TS=$(awk -v s="${suma[1]}" -v r="$REPS" 'BEGIN { printf "%.6f", s / r }')

printf "| %-10s | %-15s | %-7s | %-10s |\n" \
    "Procesos" "Tiempo prom (s)" "Speedup" "Eficiencia"
printf "|%s|%s|%s|%s|\n" "------------" "-----------------" "---------" "------------"
printf "| %-10s | %15s | %7s | %10s |\n" "1 (sec.)" "$TS" "1.00" "100.0%"

for n in 2 3 4; do
    awk -v n="$n" -v ts="$TS" -v s="${suma[$n]}" -v r="$REPS" 'BEGIN {
        tp = s / r
        sp = ts / tp
        printf "| %-10s | %15.6f | %7.2f | %9.1f%% |\n", n, tp, sp, 100 * sp / n
    }'
done
