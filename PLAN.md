# Plan de trabajo — Proyecto 02 (CC3069)

Búsqueda paralela de claves AES-128 con Open MPI.
Equipo: Leonardo Dufrey Mejía, María José Girón, Cindy Gualim.

Repositorio: https://github.com/DufreyM/CC3069-Proyecto2

## Orden de trabajo

El trabajo tiene dependencias: no todo se puede hacer al mismo tiempo. El
orden es por fases; dentro de una fase las tareas sí corren en paralelo.

### Fase 0 — Base común (primero, bloquea a todos) — Leonardo ✅
Todo el equipo construye sobre `comun.h`, así que va primero.
- [x] `comun.h` / `comun.c`: `make_key`, lectura de archivos, AES-128-CBC, `contiene_frase`.
- [x] Acuerdo de modo de cifrado (CBC), IV de 16 bytes y frase `"es una prueba de"`.

### Fase 1 — Desarrollo (en paralelo, después de la Fase 0)

**Leonardo — inciso 2 (versión secuencial y paralela base)** ✅ código
- [x] `busqueda_secuencial.c` (referencia del Speedup), con `-t` y estimación.
- [x] `busqueda_mpi.c`: estrategia cíclica base, terminación por `MPI_Allreduce`.
- [x] `Makefile` del Proyecto 02 y archivos de datos de prueba.

**María José — inciso 1 (cifrado/descifrado desde archivo)** ✅ código
- [x] `aes_cifrado.c`: cifrar un `.txt` con clave de entrada y descifrar con la misma.
      Usa las funciones ya listas en `comun.h`. Modos `-c`, `-d` y ciclo completo;
      el archivo cifrado guarda IV + datos. Agregado al `Makefile` y `.gitignore`.
- [x] Capturas: texto original, cifrado y recuperado (y casos de error).
- [ ] `README` del Proyecto 02 (reemplaza el del Lab 03). Se cierra cuando Cindy
      agregue `-s bloques|cantor` y se adapte `medir_speedup.sh`.

**Cindy — incisos 3 y 4 (estrategia alternativa)** 🟨 código listo
- [x] Agregar a `busqueda_mpi.c` las opciones `-s bloques` y `-s cantor` con `-K`
      (bloques intercalados / dovetailing), en el mismo programa que la cíclica.
- [x] Terminación asíncrona con `MPI_Isend` / `MPI_Irecv` / `MPI_Test`: cada
      proceso reporta al 0 y el 0 difunde el fin (reemplaza el `MPI_Allreduce`).
      Probado con 1, 3 y 4 procesos, rango agotado y `-t`.
- [ ] Diagramas de flujo y pseudocódigo de ambas estrategias (inciso 4).
- [x] Validar `-K` contra el rango: con `-K` enorme (p. ej. 2^62 y 4 procesos)
      `size*K` se desbordaba y la búsqueda no terminaba. Ahora `-K` se limita
      a 2^bits (Leonardo).

### Fase 2 — Mediciones (después de que compile todo) — Leonardo + Cindy ⬜
- [x] Adaptar `medir_speedup.sh` a los binarios del Proyecto 02 (clave y bits).
- [ ] Agregar a `medir_speedup.sh` la estrategia (`-s`, `-K`) y el límite `-t`
      para 2^54 / 2^54+1, que no terminan en tiempo razonable.
- [ ] Pruebas a) 123456, b) 2^54, c) 2^54+1 con cíclica, bloques y dovetailing.
- [ ] Capturas de compilación y de cada prueba.
- [ ] Cálculo de Speedup = T_secuencial / T_paralelo.

### Fase 3 — Informe y entrega (al final, todos) ⬜
- [ ] Majo: formato UVG, antecedentes, Anexo 1 (catálogo de funciones).
- [ ] Leonardo: inciso 2d (por qué varía el tiempo) y Anexo 2 (bitácora y Speedup).
- [ ] Cindy: comparación de estrategias, discusión y conclusiones.
- [ ] Todos: revisión con `valgrind`, estilo, PDF final y `.zip` con el código.

## Reglas de coordinación
- Nadie redefine el modo de cifrado ni `make_key`: todo sale de `comun.h`.
- Cindy trabaja en el mismo `busqueda_mpi.c`, agregando su estrategia como opción `-s`.
- El Anexo 1 (catálogo de funciones) se hace al final, con el código ya congelado.

## Cómo compilar y correr (WSL Ubuntu)
```bash
sudo apt update   # sin esto, libssl-dev puede dar 404
sudo apt install -y build-essential libssl-dev openmpi-bin libopenmpi-dev
make
./aes_cifrado -k 123456
./busqueda_secuencial -k 123456 -b 24
mpirun -np 4 ./busqueda_mpi -k 123456 -b 24
```
