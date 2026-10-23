# Anexo 2. Bitácora de pruebas y speedups

> Sección de Leonardo. Cada fila sale de un archivo en `resultados/` con el
> comando y la salida completa. Para regenerar todo:
>
> ```bash
> make
> ./bitacora.sh 10                  # pruebas individuales -> resultados/NN_*.txt
> bash resultados/correr_speedups.sh  # tablas de speedup -> resultados/speedup_*.txt
> ```

**Equipo.** Intel Core i9-13900HX (24 núcleos / 32 hilos), WSL 2 Ubuntu,
gcc 15.2 `-O2`, OpenSSL 3.5.5, Open MPI 5.0.10. Mensaje: `datos/mensaje.txt`
("Esta es una prueba de proyecto 2", 32 bytes). Rango: 2^56 claves.

**Cómo se mide.**

- Secuencial: `clock_gettime(CLOCK_MONOTONIC)` alrededor del ciclo de búsqueda.
- Paralela: `MPI_Wtime` desde una `MPI_Barrier` hasta que termina el último
  proceso (`MPI_Reduce` con `MPI_MAX`).
- Tiempo **estimado** (búsquedas cortadas con `-t 10`): intentos que le faltan
  al proceso dueño de la clave ÷ claves por segundo por proceso, medidas en
  esos 10 s.
- Speedup = T_secuencial / T_paralelo. Eficiencia = Speedup / p.

## A2.1 Compilación

`resultados/00_compilacion.txt`: `make -B` compila los tres programas con
`-Wall -Wextra` sin advertencias.

```text
gcc -std=c11 -O2 -Wall -Wextra busqueda_secuencial.c comun.c -o busqueda_secuencial -lcrypto
mpicc -std=c11 -O2 -Wall -Wextra busqueda_mpi.c comun.c -o busqueda_mpi -lcrypto
gcc -std=c11 -O2 -Wall -Wextra aes_cifrado.c comun.c -o aes_cifrado -lcrypto
```

## A2.2 Bitácora de ejecuciones individuales (4 procesos)

| Archivo | Comando | Resultado | Tiempo |
|---|---|---|---|
| `01_secuencial_a` | `./busqueda_secuencial -k 123456` | encontrada | 0.030 s |
| `02_secuencial_b` | `./busqueda_secuencial -k 18014398509481984 -t 10` | corte -t | ≈ 4.20 × 10^9 s (133 años) est. |
| `03_secuencial_c` | `./busqueda_secuencial -k 18014398509481985 -t 10` | corte -t | ≈ 4.21 × 10^9 s (133 años) est. |
| `10_ciclica_a` | `mpirun -np 4 ./busqueda_mpi -k 123456 -s ciclica -t 10` | encontrada | 0.0076 s |
| `10_ciclica_b` | `… -k 18014398509481984 -s ciclica -t 10` | corte -t | ≈ 1.03 × 10^9 s (32.7 años) est. |
| `10_ciclica_c` | `… -k 18014398509481985 -s ciclica -t 10` | corte -t | ≈ 1.04 × 10^9 s (32.9 años) est. |
| `10_ciclica_x1` | `… -k 3377699720527877 -s ciclica -t 10` | corte -t | ≈ 1.92 × 10^8 s (6.1 años) est. |
| `10_ciclica_x2` | `… -k 18014398509481983 -s ciclica -t 10` | corte -t | ≈ 1.04 × 10^9 s (32.8 años) est. |
| `20_bloques_a` | `… -k 123456 -s bloques -t 10` | encontrada | 0.030 s |
| `20_bloques_b` | `… -k 18014398509481984 -s bloques -t 10` | encontrada | 0.0019 s |
| `20_bloques_c` | `… -k 18014398509481985 -s bloques -t 10` | encontrada | 0.0018 s |
| `20_bloques_x1` | `… -k 3377699720527877 -s bloques -t 10` | corte -t | ≈ 7.86 × 10^8 s (24.9 años) est. |
| `20_bloques_x2` | `… -k 18014398509481983 -s bloques -t 10` | corte -t | ≈ 4.17 × 10^9 s (132 años) est. |
| `30_cantor16_a` | `… -k 123456 -s cantor -K 16 -t 10` | encontrada | 0.45 s |
| `30_cantor16_b` | `… -k 18014398509481984 -s cantor -K 16 -t 10` | encontrada | 0.0019 s |
| `30_cantor16_c` | `… -k 18014398509481985 -s cantor -K 16 -t 10` | encontrada | 0.0018 s |
| `30_cantor16_x1` | `… -k 3377699720527877 -s cantor -K 16 -t 10` | encontrada | 0.0011 s |
| `30_cantor16_x2` | `… -k 18014398509481983 -s cantor -K 16 -t 10` | corte -t | ≈ 4.22 × 10^9 s (134 años) est. |
| `40_cantor256_a` | `… -k 123456 -s cantor -K 256 -t 10` | encontrada | 7.30 s |
| `40_cantor256_b` | `… -k 18014398509481984 -s cantor -K 256 -t 10` | encontrada | 0.0020 s |
| `40_cantor256_c` | `… -k 18014398509481985 -s cantor -K 256 -t 10` | encontrada | 0.0019 s |
| `40_cantor256_x1` | `… -k 3377699720527877 -s cantor -K 256 -t 10` | encontrada | 0.0010 s |
| `40_cantor256_x2` | `… -k 18014398509481983 -s cantor -K 256 -t 10` | corte -t | ≈ 4.20 × 10^9 s (133 años) est. |

En todas las ejecuciones que encontraron la clave, el programa descifró el
mensaje completo ("Esta es una prueba de proyecto 2") y verificó que la clave
coincide con la usada para cifrar. Rendimiento: ≈ 4.3 M claves/s secuencial y
≈ 17.4 M claves/s con 4 procesos.

## A2.3 Speedup con la clave 123456 (prueba a), promedio de 10 ejecuciones

`./medir_speedup.sh -k 123456 -r 10 -s ciclica` (`resultados/speedup_a_ciclica.txt`):

| Procesos | Tiempo prom. (s) | Speedup | Eficiencia |
|---|---|---|---|
| 1 (secuencial) | 0.030061 | 1.00 | 100.0 % |
| 2 | 0.014912 | 2.02 | 100.8 % |
| 3 | 0.010031 | 3.00 | 99.9 % |
| 4 | 0.007707 | **3.90** | **97.5 %** |

Las mismas mediciones con las estrategias alternativas (`speedup_a_*.txt`):

| Procesos | Bloques | Cantor K = 16 | Cantor K = 256 (5 rep.) |
|---|---|---|---|
| 1 (secuencial) | 0.031904 s | 0.030292 s | 0.030251 s |
| 2 | 0.029629 s (S = 1.08) | 0.459262 s (S = 0.066) | 7.36856 s (S = 0.0041) |
| 3 | 0.029773 s (S = 1.07) | 0.458479 s (S = 0.066) | 7.34059 s (S = 0.0041) |
| 4 | 0.029724 s (S = 1.07) | 0.457373 s (S = 0.066) | 7.33432 s (S = 0.0041) |

**Análisis.** La cíclica escala casi linealmente: cada proceso prueba una de
cada `p` claves y 123456 se alcanza en el intento 123456/p. Con bloques no hay
speedup porque 123456 está en el bloque del proceso 0, que la prueba en el mismo
intento que la versión secuencial; los demás procesos trabajan en vano. Con
dovetailing el tiempo no depende de `p` y es `K` veces el de bloques, porque el
proceso 0 solo avanza una posición de su primer sub-bloque cada `K` intentos.

## A2.4 Speedup con las claves b) y c)

Estas búsquedas se cortan con `-t 10`; "est." indica tiempo estimado. Los
valores secuenciales varían entre tablas (4.15 a 4.22 × 10^9 s) porque cada una
mide su propio rendimiento en 10 s.

**b) 2^54** (`speedup_b_*.txt`):

| Procesos | Cíclica (1 rep.) | Bloques (3 rep.) | Cantor K = 16 (3 rep.) |
|---|---|---|---|
| 1 (secuencial) | 4.22 × 10^9 s est. | 4.21 × 10^9 s est. | 4.15 × 10^9 s est. |
| 2 | 2.09 × 10^9 s est. (S = 2.02) | 4.19 × 10^9 s est. (S = 1.00) | **0.00101 s** (S ≈ 4.1 × 10^12) |
| 3 | 1.39 × 10^9 s est. (S = 3.04) | 4.19 × 10^9 s est. (S = 1.00) | 5.57 × 10^9 s est. (S = 0.74) |
| 4 | 1.04 × 10^9 s est. (S = **4.06**) | **0.00165 s** (S ≈ 2.6 × 10^12) | **0.00169 s** (S ≈ 2.4 × 10^12) |

**c) 2^54 + 1** (`speedup_c_*.txt`):

| Procesos | Cíclica (1 rep.) | Cantor K = 16 (3 rep.) |
|---|---|---|
| 1 (secuencial) | 4.22 × 10^9 s est. | 4.20 × 10^9 s est. |
| 2 | 2.08 × 10^9 s est. (S = 2.03) | **0.00098 s** (S ≈ 4.3 × 10^12) |
| 3 | 1.38 × 10^9 s est. (S = 3.06) | 5.58 × 10^9 s est. (S = 0.75) |
| 4 | 1.03 × 10^9 s est. (S = **4.08**) | **0.00189 s** (S ≈ 2.2 × 10^12) |

**Análisis.**

- **Cíclica: speedup real ≈ p.** Secuencial y paralela recorren las claves en
  el mismo orden, así que la clave queda en el intento 2^54/p y el speedup
  estimado es 2.02, 3.04 y 4.06. Es la ganancia que da el paralelismo.
- **Bloques y cantor: speedups de 10^12 que no son paralelismo.** Vienen de
  reordenar el espacio: la clave pasa de estar en el intento 2^54 a estar en el
  primero de algún proceso. No se deben leer como eficiencia; la eficiencia
  "real" de esas estrategias es la del inciso A2.3.
- **Dependen de `p`.** Con bloques, 2^54 es el inicio de un bloque solo con
  4 procesos. Con 2 queda a mitad del bloque del proceso 0 y con 3 a tres
  cuartos de ese bloque; en ambos casos el proceso 0 la prueba en el intento
  2^54 + 1, igual que la versión secuencial, así que no hay ganancia.
  Con cantor K = 16 coincide con un inicio de sub-bloque con 2 y 4 procesos,
  pero con 3 el sub-bloque empieza en 2^54 + 2 y la clave queda al final del
  sub-bloque anterior: ≈ 177 años, peor que la versión secuencial.

## A2.5 Capturas pendientes

Hay que tomar las capturas de pantalla de estas mismas ejecuciones con el
comando visible (rúbrica: "sin evidencia no se califica"). Sugerencia: correr
`./bitacora.sh 10` y `bash resultados/correr_speedups.sh` en una terminal amplia
y capturar cada bloque `$ comando` + salida.

- [ ] Compilación (`00_compilacion`).
- [ ] Pruebas a), b) y c) con cíclica (`10_ciclica_a|b|c`).
- [ ] Pruebas a), b) y c) con bloques y cantor K = 16 / 256 (`20_*`, `30_*`, `40_*`).
- [ ] Tablas de speedup (`speedup_a_ciclica`, `speedup_a_cantor16`,
      `speedup_b_ciclica`, `speedup_b_cantor16`).
