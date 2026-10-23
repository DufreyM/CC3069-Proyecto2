# Inciso 3. Estrategia alternativa y comparación

> Sección de Cindy. El funcionamiento detallado, el pseudocódigo y los diagramas
> están en el inciso 4 (`inciso4_estrategia_alternativa.md`).

## 3.1 Propuesta

Con la distribución cíclica las cuatro secuencias avanzan juntas desde 0, así
que el tiempo para encontrar una clave crece con su valor (inciso 2d). Para
cambiar **qué** claves se encuentran rápido implementamos, en el mismo
`busqueda_mpi.c`, dos estrategias que se eligen con `-s`:

- **Bloques** (`-s bloques`): el espacio de 2^56 claves se divide en 4 bloques
  contiguos de 2^54 y cada proceso recorre el suyo en orden. Cada proceso tiene
  un punto de arranque: 0, 2^54, 2^55 y 3·2^54.
- **Bloques intercalados / dovetailing** (`-s cantor -K <K>`): cada bloque se
  parte en `K` sub-bloques y el proceso prueba una clave de cada sub-bloque por
  turnos. Con `K = 16` hay 4·16 = 64 puntos de arranque repartidos por todo el
  espacio; con `K = 256`, 1 024.

Además, la terminación con `MPI_Allreduce` por lote se cambió por avisos
asíncronos (`MPI_Isend` / `MPI_Irecv` / `MPI_Test`), de modo que ningún proceso
se detiene a sincronizar mientras busca.

## 3.2 Resultados con las claves del inciso 2 (4 procesos)

Comando: `mpirun -np 4 ./busqueda_mpi -k <clave> -s <estrategia> [-K <K>] -t 10`.
"est." = estimado a partir del rendimiento medido en 10 s y del intento en que
el proceso dueño prueba la clave.

| Clave | Cíclica (base) | Bloques | Cantor K = 16 | Cantor K = 256 |
|---|---|---|---|---|
| a) 123456 | **0.0076 s** | 0.030 s | 0.45 s | 7.30 s |
| b) 2^54 | ≈ 32.7 años (est.) | **0.0019 s** | **0.0019 s** | **0.0020 s** |
| c) 2^54 + 1 | ≈ 32.9 años (est.) | **0.0018 s** | **0.0018 s** | **0.0019 s** |
| extra: 3·2^50 + 5 | ≈ 6.1 años (est.) | ≈ 24.9 años (est.) | **0.0011 s** | **0.0010 s** |
| extra: 2^54 − 1 | ≈ 32.8 años (est.) | ≈ 132 años (est.) | ≈ 134 años (est.) | ≈ 133 años (est.) |

La clave 3·2^50 + 5 está a mitad del bloque del proceso 0 (justo después del
inicio de su sub-bloque 3 con K = 16) y 2^54 − 1 es la última clave del bloque
del proceso 0: el peor caso de las estrategias por bloques.

Las tres estrategias prueban unas 4.37 M claves por segundo por proceso, así que
las diferencias vienen solo del **orden** en que se recorren las claves:

| Clave | Intento en su proceso: cíclica | bloques | K = 16 | K = 256 |
|---|---|---|---|---|
| 123456 | 30 865 | 123 457 | 1 975 297 | 31 604 737 |
| 2^54 | 4.5 × 10^15 | 1 | 1 | 1 |
| 2^54 + 1 | 4.5 × 10^15 | 2 | 17 | 257 |

## 3.3 Análisis

**b) y c) pasan de décadas a milisegundos.** 2^56/4 es exactamente el inicio
del bloque del proceso 1, así que las estrategias por bloques la prueban en su
primer intento, y 2^54 + 1 en el segundo (bloques) o en el `K + 1` (cantor).
Los ~2 ms que se miden son casi todo latencia de terminación: el proceso 0 se
entera del hallazgo al terminar su lote de 4096 claves.

**a) se vuelve más lenta.** 123456 está en el bloque del proceso 0, que es el
único que trabaja en algo útil: con bloques el tiempo es el de la versión
secuencial (0.030 s), y con dovetailing es `K` veces mayor, porque el proceso
solo avanza una posición en cada sub-bloque cada `K` intentos (16 × 123 457 =
1.97 M intentos; 256 × 123 457 = 31.6 M).

**Ninguna estrategia reduce el trabajo total ni el tiempo promedio.** Las tres
prueban las mismas 2^56 claves sin repetir ninguna. Para una clave al azar el
tiempo esperado es el mismo (la mitad del recorrido de un proceso: 2^53
intentos, ≈ 65 años con 4 procesos); lo que cambia es *qué* claves caen cerca del principio. La
cíclica favorece las claves pequeñas; los bloques, las que están al inicio de
cada cuarto del espacio; el dovetailing, las que están al inicio de cada uno de
sus `4·K` sub-bloques. La clave 2^54 − 1 muestra que el peor caso sigue
existiendo para todas.

**Los puntos de arranque dependen del número de procesos.** Con 3 procesos y
K = 16, el sub-bloque 12 del proceso 0 empieza en 2^54 + 2, así que 2^54 queda
como la *última* clave del sub-bloque 11 y su búsqueda se estima en ≈ 177 años
(Anexo 2). Con 2 y 4 procesos 2^54 coincide con un inicio de sub-bloque y sale
en milisegundos. El dovetailing es bueno para las claves del enunciado, pero no
es una mejora general.

## 3.4 Ventajas y limitaciones

| Estrategia | Ventajas | Limitaciones |
|---|---|---|
| Cíclica (base) | La más rápida con claves pequeñas; reparto perfectamente equilibrado; speedup casi lineal (3.90 con 4 procesos en a)) | b) y c) tardan décadas; el tiempo crece linealmente con la clave |
| Bloques | b) y c) instantáneas; la más simple | Solo 4 puntos de arranque; claves a mitad de bloque tardan años; en a) no hay speedup (solo trabaja el proceso 0) |
| Cantor K = 16 | a), b), c) y la clave a mitad de bloque en menos de un segundo; 64 puntos de arranque sin trabajo extra | Claves pequeñas K veces más lentas que con bloques; el resultado depende de `p` y `K`; el peor caso no cambia |
| Cantor K = 256 | 1 024 puntos de arranque | 123456 tarda 7.3 s: el costo para claves pequeñas crece con `K` |
| Terminación asíncrona | Nadie se detiene a sincronizar; el aviso llega en ~1 ms | Más código: hay que contar reportes, cancelar el buzón pendiente y esperar los envíos antes de `MPI_Finalize` |

**Conclusión del inciso.** Para las claves del inciso 2 la mejor opción es el
dovetailing con un `K` moderado (16): resuelve a), b) y c) en menos de medio
segundo, cuando la cíclica tardaría décadas en b) y c). El costo es hacer más
lenta la clave pequeña (0.45 s contra 0.0076 s) y depender de dónde caen los
puntos de arranque.
