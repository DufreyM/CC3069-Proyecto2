#!/bin/bash
# Speedups del Anexo 2: guarda cada tabla de medir_speedup.sh en resultados/.
cd "$(dirname "$0")/.." || exit 1

medir() {
    local nombre=$1
    shift
    { echo "\$ ./medir_speedup.sh $*"; ./medir_speedup.sh "$@"; } \
        | tee "resultados/$nombre.txt"
}

medir speedup_a_ciclica   -k 123456 -r 10 -s ciclica
medir speedup_a_bloques   -k 123456 -r 10 -s bloques
medir speedup_a_cantor16  -k 123456 -r 10 -s cantor -K 16
medir speedup_a_cantor256 -k 123456 -r 5  -s cantor -K 256
medir speedup_b_ciclica   -k 18014398509481984 -r 1 -s ciclica -t 10
medir speedup_b_cantor16  -k 18014398509481984 -r 3 -s cantor -K 16 -t 10
medir speedup_b_bloques   -k 18014398509481984 -r 3 -s bloques -t 10
medir speedup_c_ciclica   -k 18014398509481985 -r 1 -s ciclica -t 10
medir speedup_c_cantor16  -k 18014398509481985 -r 3 -s cantor -K 16 -t 10
