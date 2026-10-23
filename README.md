# CC3069 Proyecto 02: búsqueda paralela de claves AES-128 con Open MPI

Universidad del Valle de Guatemala, Computación Paralela y Distribuida, sección 10.

**Integrantes:** María José Girón Isidro, Cindy Gualim Perez y Leonardo Dufrey Mejía Mejía.

Los programas cifran un texto con AES-128 en modo CBC y luego buscan la clave por fuerza bruta: prueban claves candidatas hasta que el texto descifrado contiene la frase `"es una prueba de"`. Hay una versión secuencial y una paralela con Open MPI que permite elegir cómo se reparten las claves entre los procesos.

## Archivos

| Archivo | Contenido |
| --- | --- |
| `comun.h`, `comun.c` | Funciones compartidas: conversión del número a clave de 16 bytes, lectura de archivos, cifrado y descifrado AES-128-CBC y búsqueda de la frase |
| `aes_cifrado.c` | Cifra un archivo `.txt` con una clave de entrada y lo descifra con la misma clave (inciso 1) |
| `busqueda_secuencial.c` | Búsqueda de la clave por fuerza bruta en un solo proceso; es la referencia para el Speedup |
| `busqueda_mpi.c` | Búsqueda paralela con Open MPI y las estrategias `ciclica`, `bloques` y `cantor` (dovetailing) |
| `Makefile` | Compila los tres programas |
| `medir_speedup.sh` | Compara la versión secuencial con la paralela (2, 3 y 4 procesos) y calcula Speedup y eficiencia |
| `bitacora.sh` | Ejecuta todas las pruebas de los incisos 2 y 3 y guarda cada salida en `resultados/` |
| `datos/` | Archivos de prueba: `mensaje.txt` (texto del enunciado), `texto_largo.txt` (varias líneas) y `vacio.txt` |
| `resultados/` | Salidas de `bitacora.sh`, resumen de las pruebas (`resumen.md`), tablas de Speedup y `correr_speedups.sh`, que las genera |
| `informe/` | Secciones del informe: incisos 2, 3 y 4, discusión y conclusiones, y Anexo 2 (bitácora y speedups) |
| `diagrama/` | Diagrama de flujo de AES-128 (imagen y archivo de draw.io) y diagramas del inciso 4: reparto de claves, flujo de `busqueda_mpi` y terminación asíncrona, generados con `generar_diagramas.py` |

## Requisitos

Ubuntu o WSL con gcc, OpenSSL y Open MPI:

```bash
sudo apt update
sudo apt install -y build-essential libssl-dev openmpi-bin libopenmpi-dev
```

Sin el `apt update`, la instalación de `libssl-dev` puede fallar con un error 404.

Para volver a generar los diagramas del inciso 4 se necesita Python con `matplotlib`.

## Compilar

```bash
make          # compila aes_cifrado, busqueda_secuencial y busqueda_mpi
make clean    # borra los binarios, los archivos .enc y recuperado.txt
```

## Uso

En los tres programas la clave se escribe en decimal. El número se coloca en los últimos 8 bytes de la clave de 16 bytes y los primeros 8 quedan en cero.

### Cifrado y descifrado: `aes_cifrado`

```bash
./aes_cifrado -k 123456                                   # cifra, guarda, descifra y compara
./aes_cifrado -c -k 123456 -f datos/mensaje.txt -o datos/mensaje.enc
./aes_cifrado -d -k 123456 -f datos/mensaje.enc -o recuperado.txt
```

| Opción | Descripción | Por defecto |
| --- | --- | --- |
| `-c` | Solo cifra | |
| `-d` | Solo descifra | |
| `-k clave` | Clave en decimal | `123456` |
| `-f entrada` | Archivo de entrada | `datos/mensaje.txt` (`datos/mensaje.enc` con `-d`) |
| `-o salida` | Archivo de salida | `datos/mensaje.enc` (`recuperado.txt` con `-d`) |

Sin `-c` ni `-d`, el programa hace el ciclo completo y verifica que el texto recuperado sea idéntico al original. El archivo cifrado guarda el IV (16 bytes) seguido del texto cifrado. El IV es aleatorio, por lo que el cifrado cambia en cada ejecución aunque se use la misma clave.

### Búsqueda secuencial: `busqueda_secuencial`

```bash
./busqueda_secuencial -k 123456
./busqueda_secuencial -k 123456 -b 24
./busqueda_secuencial -k 18014398509481984 -t 10
```

| Opción | Descripción | Por defecto |
| --- | --- | --- |
| `-f archivo` | Texto a cifrar | `datos/mensaje.txt` |
| `-k clave` | Clave secreta en decimal | `123456` |
| `-b bits` | Explora 2^bits candidatas (1 a 56) | `56` |
| `-t segundos` | Límite de tiempo; al cumplirse estima el tiempo hasta la clave | sin límite |

### Búsqueda paralela: `busqueda_mpi`

```bash
mpirun -np 4 ./busqueda_mpi -k 123456
mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -s bloques
mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -s cantor -K 16
mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -s ciclica -t 10
```

| Opción | Descripción | Por defecto |
| --- | --- | --- |
| `-f archivo` | Texto a cifrar | `datos/mensaje.txt` |
| `-k clave` | Clave secreta en decimal | `123456` |
| `-b bits` | Explora 2^bits candidatas (1 a 56) | `56` |
| `-t segundos` | Límite de tiempo; al cumplirse estima el tiempo hasta la clave | sin límite |
| `-s modo` | Estrategia: `ciclica`, `bloques` o `cantor` | `ciclica` |
| `-K num` | Sub-bloques por proceso con `-s cantor` | `16` |

Estrategias de distribución con `p` procesos:

- **`ciclica`** (estrategia base): el proceso `r` prueba las claves `r`, `r+p`, `r+2p`, ...
- **`bloques`**: el rango se parte en `p` bloques contiguos de 2^bits / p claves y cada proceso recorre el suyo en orden.
- **`cantor`** (estrategia alternativa, dovetailing): cada bloque se parte en `K` sub-bloques y el proceso avanza una clave en cada sub-bloque por turnos, así que tiene `K` puntos de arranque en lugar de uno. Si el bloque se ve como una matriz de `K` filas, `bloques` lo lee por filas y `cantor` por columnas; `bloques` equivale a `cantor` con `K = 1`. Ninguna clave se repite ni se omite.

La terminación es asíncrona, con `MPI_Isend`, `MPI_Irecv` y `MPI_Test`: cada proceso reporta al proceso 0 cuando encuentra la clave o agota su rango o su tiempo, y el proceso 0 avisa a todos la decisión final. Ningún proceso se bloquea esperando a los demás.

### Medición de Speedup: `medir_speedup.sh`

```bash
./medir_speedup.sh [-k clave] [-b bits] [-r reps] [-s ciclica|bloques|cantor] [-K sub_bloques] [-t seg]
./medir_speedup.sh -k 123456 -r 10                          # prueba a), ciclica
./medir_speedup.sh -k 123456 -r 10 -s cantor -K 16
./medir_speedup.sh -k 18014398509481984 -r 1 -s cantor -t 10
```

Por defecto usa la clave `123456`, 56 bits, 10 repeticiones, la estrategia `ciclica` y `K = 16`. Primero verifica que la versión secuencial y la paralela recuperen la misma clave y el mismo mensaje. Después promedia los tiempos de cada configuración (1, 2, 3 y 4 procesos) e imprime una tabla con:

- Speedup = T_secuencial / T_paralelo
- Eficiencia = Speedup / número de procesos

Si una ejecución llega al límite `-t` sin encontrar la clave, se usa el tiempo estimado hasta la clave que reporta el programa (columna "Tipo" = estimado).

`resultados/correr_speedups.sh` ejecuta las mediciones de las pruebas a), b) y c) y guarda cada tabla en `resultados/speedup_*.txt`.

### Bitácora de pruebas: `bitacora.sh`

```bash
./bitacora.sh [limite_segundos]    # por defecto 10
```

Compila con `make -B` y ejecuta, con 4 procesos, las claves de las pruebas a) 123456, b) 18014398509481984 (2^56 / 4) y c) 18014398509481985 (2^56 / 4 + 1), más dos claves extra: 3377699720527877 (3·2^50 + 5, a mitad de un bloque) y 18014398509481983 (2^54 − 1, última clave del proceso 0). Cada clave se prueba con la versión secuencial (solo a, b y c) y con `ciclica`, `bloques`, `cantor -K 16` y `cantor -K 256`. Las búsquedas que tardarían años se cortan con el límite y el programa estima el tiempo hasta la clave.

Cada salida, con el comando que la generó, queda en `resultados/<prueba>.txt`, y la tabla con el tiempo real o estimado de todas las pruebas queda en `resultados/resumen.md`.
