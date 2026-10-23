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
| `busqueda_mpi.c` | Búsqueda paralela con Open MPI y las estrategias `ciclica`, `bloques` y `cantor` |
| `Makefile` | Compila los tres programas |
| `medir_speedup.sh` | Compara la versión secuencial con la paralela (2, 3 y 4 procesos) y calcula Speedup y eficiencia |
| `datos/` | Archivos de prueba: `mensaje.txt` (texto del enunciado), `texto_largo.txt` (varias líneas) y `vacio.txt` |
| `diagrama/` | Diagrama de flujo de AES-128 (imagen y archivo editable de draw.io) |

## Requisitos

Ubuntu o WSL con gcc, OpenSSL y Open MPI:

```bash
sudo apt update
sudo apt install -y build-essential libssl-dev openmpi-bin libopenmpi-dev
```

Sin el `apt update`, la instalación de `libssl-dev` puede fallar con un error 404.

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
./busqueda_secuencial -k 18014398509481984 -t 60
```

| Opción | Descripción | Por defecto |
| --- | --- | --- |
| `-f archivo` | Texto a cifrar | `datos/mensaje.txt` |
| `-k clave` | Clave secreta en decimal | `123456` |
| `-b bits` | Explora 2^bits candidatas (1 a 56) | `56` |
| `-t segundos` | Límite de tiempo; al cumplirse estima el tiempo total | sin límite |

### Búsqueda paralela: `busqueda_mpi`

```bash
mpirun -np 4 ./busqueda_mpi -k 123456
mpirun -np 4 ./busqueda_mpi -k 123456 -s cantor
mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -s bloques -K 18014398509481984
```

| Opción | Descripción | Por defecto |
| --- | --- | --- |
| `-f archivo` | Texto a cifrar | `datos/mensaje.txt` |
| `-k clave` | Clave secreta en decimal | `123456` |
| `-b bits` | Explora 2^bits candidatas (1 a 56) | `56` |
| `-t segundos` | Límite de tiempo; al cumplirse estima el tiempo total | sin límite |
| `-s modo` | Estrategia: `ciclica`, `bloques` o `cantor` | `ciclica` |
| `-K tam` | Tamaño de bloque para `-s bloques` (si es mayor que 2^bits, se limita a 2^bits) | `1024` |

Estrategias de distribución con `p` procesos:

- **`ciclica`**: el proceso `r` prueba las claves `r`, `r+p`, `r+2p`, ...
- **`bloques`**: el espacio se divide en bloques de `K` claves consecutivas; el bloque `g` le toca al proceso `g % p`, que lo recorre completo antes de pasar a su siguiente bloque. Con `-K 18014398509481984` (2^56 / 4) y 4 procesos, cada proceso recibe un solo bloque contiguo.
- **`cantor`**: la clave `n` pertenece a una anti-diagonal `w` del emparejamiento de Cantor; el proceso `r` recorre las diagonales `w = r`, `r+p`, `r+2p`, ...

Al terminar, los procesos se coordinan de forma asíncrona con `MPI_Isend`, `MPI_Irecv` y `MPI_Test`: cada proceso reporta al proceso 0 y este avisa a todos que la búsqueda terminó.

### Medición de Speedup: `medir_speedup.sh`

```bash
./medir_speedup.sh [clave] [bits] [repeticiones]
./medir_speedup.sh 123456 56 10    # prueba a) del enunciado
./medir_speedup.sh 123456 24 10    # rango reducido para pruebas rápidas
```

Primero verifica que la versión secuencial y la paralela con 4 procesos recuperen la misma clave y el mismo mensaje. Después promedia los tiempos de cada configuración (1, 2, 3 y 4 procesos) e imprime una tabla con:

- Speedup = T_secuencial / T_paralelo
- Eficiencia = Speedup / número de procesos
