#!/bin/bash
#----------------------------------------------------------------------
# CC3069 - Proyecto 02
# Mide el Speedup de la busqueda de clave AES-128-CBC: compara la version
# secuencial contra la paralela (Open MPI) con 2, 3 y 4 procesos.
#
#   Speedup    = T_secuencial / T_paralelo
#   Eficiencia = Speedup / numero de procesos
#
# Cada tiempo es el promedio de REPS ejecuciones. Si una ejecucion llega al
# limite -t sin encontrar la clave, se usa el tiempo estimado hasta la clave
# que reporta el programa (columna "Tipo" = estimado).
#
# Uso: ./medir_speedup.sh [-k clave] [-b bits] [-r reps]
#                         [-s ciclica|bloques|cantor] [-K sub_bloques] [-t seg]
#   ./medir_speedup.sh -k 123456 -r 10                     # prueba a), ciclica
#   ./medir_speedup.sh -k 123456 -r 10 -s cantor -K 16
#   ./medir_speedup.sh -k 18014398509481984 -r 1 -s cantor -t 10
#----------------------------------------------------------------------
KEY=123456
BITS=56
REPS=10
ESTRATEGIA=ciclica
SUBBLOQUES=16
LIMITE=
SEQ=./busqueda_secuencial
PAR=./busqueda_mpi

uso() {
    echo "Uso: $0 [-k clave] [-b bits] [-r reps] [-s ciclica|bloques|cantor]" \
         "[-K sub_bloques] [-t seg]" >&2
    exit 1
}

while getopts "k:b:r:s:K:t:" opcion; do
    case $opcion in
        k) KEY=$OPTARG ;;
        b) BITS=$OPTARG ;;
        r) REPS=$OPTARG ;;
        s) ESTRATEGIA=$OPTARG ;;
        K) SUBBLOQUES=$OPTARG ;;
        t) LIMITE=$OPTARG ;;
        *) uso ;;
    esac
done

if [ ! -x "$SEQ" ] || [ ! -x "$PAR" ]; then
    echo "Faltan los binarios. Ejecuta 'make' primero." >&2
    exit 1
fi

ARGS=(-k "$KEY" -b "$BITS")
[ -n "$LIMITE" ] && ARGS+=(-t "$LIMITE")
ARGS_PAR=("${ARGS[@]}" -s "$ESTRATEGIA" -K "$SUBBLOQUES")

# Devuelve "segundos tipo": el tiempo real si encontro la clave o el
# estimado hasta la clave si se detuvo por el limite -t.
tiempo() {
    awk '/^Resultado: ENCONTRADA/ { ok = 1 }
         /^Tiempo:/ { t = $2 }
         /^Tiempo estimado hasta la clave \(s\):/ { e = $NF }
         END {
             if (ok) print t, "medido"
             else if (e != "") print e, "estimado"
             else print "nan", "sin_dato"
         }'
}

# Extrae la clave encontrada y el texto recuperado (para la verificacion).
resultado() { awk '/^Clave encontrada:/ {print $3} /^Texto recuperado:/ {print}'; }

# Ejecuta con n procesos (n = 1 => secuencial).
ejecutar() {
    if [ "$1" -eq 1 ]; then
        $SEQ "${ARGS[@]}"
    else
        mpirun -np "$1" $PAR "${ARGS_PAR[@]}"
    fi
}

echo "Escenario: clave $KEY, 2^$BITS candidatas, estrategia $ESTRATEGIA" \
     "(K = $SUBBLOQUES), $REPS repeticiones${LIMITE:+, limite $LIMITE s}"
echo

echo "Verificacion de resultados:"
seq_res=$(ejecutar 1 | resultado)
par_res=$(ejecutar 4 | resultado)
echo "  Secuencial: $(echo $seq_res)"
echo "  MPI (4):    $(echo $par_res)"
if [ -z "$seq_res" ] || [ -z "$par_res" ]; then
    echo "  Al menos una version no termino; se comparan tiempos estimados."
elif [ "$seq_res" = "$par_res" ]; then
    echo "  Ambas versiones recuperan la misma clave y mensaje."
else
    echo "  ERROR: los resultados no coinciden."
fi
echo

# Acumula los tiempos de cada configuracion sobre REPS repeticiones.
declare -A suma tipo
for ((r = 1; r <= REPS; r++)); do
    printf "Repeticion %d/%d\r" "$r" "$REPS"
    for n in 1 2 3 4; do
        read -r t k < <(ejecutar "$n" | tiempo)
        tipo[$n]=$k
        suma[$n]=$(awk -v a="${suma[$n]:-0}" -v b="$t" 'BEGIN { printf "%.6f", a + b }')
    done
done
echo

TS=$(awk -v s="${suma[1]}" -v r="$REPS" 'BEGIN { printf "%.6f", s / r }')

printf "| %-10s | %-16s | %-8s | %-9s | %-10s |\n" \
    "Procesos" "Tiempo prom (s)" "Tipo" "Speedup" "Eficiencia"
printf "|%s|%s|%s|%s|%s|\n" "------------" "------------------" "----------" \
    "-----------" "------------"
printf "| %-10s | %16.6g | %-8s | %9s | %10s |\n" "1 (sec.)" "$TS" "${tipo[1]}" "1.00" "100.0%"

for n in 2 3 4; do
    awk -v n="$n" -v ts="$TS" -v s="${suma[$n]}" -v r="$REPS" -v k="${tipo[$n]}" 'BEGIN {
        tp = s / r
        sp = ts / tp
        printf "| %-10s | %16.6g | %-8s | %9.4g | %9.1f%% |\n", n, tp, k, sp, 100 * sp / n
    }'
done
