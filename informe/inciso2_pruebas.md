# Inciso 2. Pruebas con la estrategia base (cíclica, 4 procesos)

> Sección de Leonardo. Salidas completas en `resultados/` (generadas con
> `./bitacora.sh 10`); las capturas se toman de esas mismas ejecuciones.

**Equipo de pruebas.** Intel Core i9-13900HX (24 núcleos / 32 hilos), WSL 2 con
Ubuntu, gcc 15.2 con `-O2`, OpenSSL 3.5.5 (interfaz EVP) y Open MPI 5.0.10. Todas las ejecuciones
paralelas usan `mpirun -np 4` y el mensaje `datos/mensaje.txt` =
"Esta es una prueba de proyecto 2" (32 bytes, sin salto de línea). Cada
candidata se descifra con AES-128-CBC y se acepta si el texto contiene
"es una prueba de".

El tiempo se mide con `MPI_Wtime` desde la barrera inicial hasta que termina el
último proceso (`MPI_Reduce` con `MPI_MAX`). Las búsquedas que tardarían años se
cortan con `-t 10`: el programa mide su rendimiento en esos 10 s y estima el
tiempo hasta la clave a partir del intento en que su proceso la probaría.

## Resultados

| Prueba | Clave | Comando | Resultado | Tiempo |
|---|---|---|---|---|
| a) | 123456 | `mpirun -np 4 ./busqueda_mpi -k 123456` | encontrada | **0.0076 s** (medido) |
| b) | 2^56/4 = 18014398509481984 | `mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -t 10` | corte a los 10 s | **≈ 32.7 años** (estimado) |
| c) | 2^56/4 + 1 = 18014398509481985 | `mpirun -np 4 ./busqueda_mpi -k 18014398509481985 -t 10` | corte a los 10 s | **≈ 32.9 años** (estimado) |

Rendimiento medido con 4 procesos: 17.0 a 17.6 millones de claves por segundo
(≈ 4.37 M por proceso). La versión secuencial alcanza 4.1 a 4.3 M claves/s.

Como referencia, la versión secuencial tarda 0.030 s en a) y se estima en
**≈ 133 años** para b) y c), porque tendría que probar 2^54 claves antes de
llegar a ellas.

## d) ¿Por qué el tiempo depende de la clave?

El programa no sabe dónde está la clave, así que el tiempo para encontrarla es
el número de intentos que necesita **el proceso al que le toca**, dividido entre
la velocidad de ese proceso:

  T(c) ≈ intento(c) / (claves por segundo de un proceso)

Con la distribución cíclica el proceso `c mod 4` prueba la clave `c` en su
intento `c div 4 + 1`:

| Clave | Proceso | Intento | Tiempo |
|---|---|---|---|
| 123456 | 0 | 30 865 | 30 865 / 4.37 M/s ≈ 7 ms |
| 2^54 | 0 | 2^52 + 1 ≈ 4.5 × 10^15 | ≈ 1.03 × 10^9 s ≈ 32.7 años |
| 2^54 + 1 | 1 | 2^52 + 1 ≈ 4.5 × 10^15 | ≈ 32.7 años |

1. **El tiempo crece linealmente con el valor de la clave.** La cíclica avanza
   las cuatro secuencias al mismo ritmo, así que todas las claves menores que
   `c` se prueban antes que `c`. 2^54 es 1.46 × 10^11 veces mayor que 123456, y
   el tiempo crece en la misma proporción: de milisegundos a décadas.
2. **b) y c) cuestan lo mismo.** Las dos claves están en el mismo intento, una
   en el proceso 0 y la otra en el 1. La diferencia entre 32.65 y 32.9 años no
   viene de la clave: es la variación de rendimiento entre las dos mediciones
   de 10 s, que se multiplica por 4.5 × 10^15 intentos.
3. **Ser "un cuarto del espacio" no ayuda con la cíclica.** 2^56/4 sería el
   inicio exacto del bloque del proceso 1 si el espacio se repartiera en
   bloques contiguos, pero con la cíclica queda en la cuarta parte del
   recorrido de cada proceso. Esto motiva la estrategia del inciso 3.
4. **Variación entre ejecuciones.** Con la misma clave el tiempo también cambia
   un poco por la planificación del sistema operativo: el i9-13900HX mezcla
   núcleos de rendimiento y de eficiencia, y WSL comparte la CPU con Windows.
   Además, el aviso de fin se revisa cada 4096 claves, así que siempre hay
   hasta ~1 ms de latencia. Por eso se promedian 10 repeticiones (Anexo 2).

## e) Capturas

Una captura por prueba, con el comando visible (la salida de `bitacora.sh`
muestra `$ comando` antes de cada resultado):

- [ ] `resultados/00_compilacion.txt`: `make -B` sin advertencias.
- [ ] `resultados/10_ciclica_a.txt`: prueba a).
- [ ] `resultados/10_ciclica_b.txt`: prueba b).
- [ ] `resultados/10_ciclica_c.txt`: prueba c).
- [ ] `resultados/01_secuencial_a.txt` a `03_secuencial_c.txt`: referencia secuencial.
