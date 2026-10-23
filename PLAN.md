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
- [ ] `README` del Proyecto 02 (reemplaza el del Lab 03). Ya se puede cerrar:
      las opciones finales son `-s ciclica|bloques|cantor`, `-K` (sub-bloques,
      por defecto 16) y `-t`; scripts `medir_speedup.sh` y `bitacora.sh`.

**Cindy — incisos 3 y 4 (estrategia alternativa)** ✅
- [x] `-s bloques` y `-s cantor -K` en `busqueda_mpi.c`. Se reimplementaron como
      en la propuesta: bloques **contiguos** de 2^bits/p por proceso, y cantor
      parte cada bloque en K sub-bloques recorridos por turnos. (La primera
      versión dejaba 2^54 en el intento ~2^52, igual que la cíclica.)
- [x] Terminación asíncrona con `MPI_Isend` / `MPI_Irecv` / `MPI_Test`.
- [x] `-K` limitado al tamaño del bloque (antes se desbordaba `size*K`).
- [x] Arnés que verifica que ninguna clave se repite ni se omite (1-9 procesos).
- [x] Diagramas en `diagrama/` (`generar_diagramas.py`): reparto de claves,
      flujo de `busqueda_mpi.c` y secuencia de la terminación asíncrona.
- [x] Pseudocódigo y explicación: `informe/inciso4_estrategia_alternativa.md`.

### Fase 2 — Mediciones — Leonardo + Cindy ✅ (faltan capturas)
- [x] `medir_speedup.sh` con `-k -b -r -s -K -t`; usa el tiempo estimado si se
      corta con `-t`.
- [x] `bitacora.sh`: compila y corre a), b), c) y dos claves extra con cíclica,
      bloques, cantor K=16 y K=256 → `resultados/NN_*.txt` y `resumen.md`.
- [x] Speedups → `resultados/speedup_*.txt` (`resultados/correr_speedups.sh`).
      Cíclica con 123456: S = 3.90 con 4 procesos.
- [ ] **Capturas de pantalla** de compilación, cada prueba y las tablas de
      speedup (correr los scripts y capturar; lista en `informe/anexo2_*.md`).

### Fase 3 — Informe y entrega (al final, todos) 🟨
- [ ] Majo: formato UVG, antecedentes, Anexo 1 (catálogo de funciones). El
      código ya está congelado; las secciones listas están en `informe/`.
- [x] Leonardo: inciso 2 y 2d (`informe/inciso2_pruebas.md`) y Anexo 2
      (`informe/anexo2_bitacora_speedup.md`).
- [x] Cindy: comparación de estrategias (`informe/inciso3_comparacion.md`),
      discusión, conclusiones y recomendaciones (`informe/discusion_conclusiones.md`).
- [ ] Todos: revisar los textos, pegar capturas, `valgrind` (no está instalado
      en el WSL de Leonardo: `sudo apt install valgrind`), PDF final y `.zip`.

## Reglas de coordinación
- Nadie redefine el modo de cifrado ni `make_key`: todo sale de `comun.h`.
- Cindy trabaja en el mismo `busqueda_mpi.c`, agregando su estrategia como opción `-s`.
- Los diagramas se regeneran con `python diagrama/generar_diagramas.py` (matplotlib).
- El Anexo 1 (catálogo de funciones) se hace al final, con el código ya congelado.

## Cómo compilar y correr (WSL Ubuntu)
```bash
sudo apt update   # sin esto, libssl-dev puede dar 404
sudo apt install -y build-essential libssl-dev openmpi-bin libopenmpi-dev
make
./aes_cifrado -k 123456
./busqueda_secuencial -k 123456 -b 24
mpirun -np 4 ./busqueda_mpi -k 18014398509481984 -s cantor -K 16
./bitacora.sh 10
```
