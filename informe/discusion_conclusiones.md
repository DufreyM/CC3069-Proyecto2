# Discusión, conclusiones y recomendaciones

> Sección de Cindy (rúbrica, criterio 3: retos, aprendizajes, soluciones y
> análisis general del rendimiento).

## Discusión

### Análisis general del rendimiento

- **El paralelismo funciona bien cuando el orden no cambia.** Con la estrategia
  cíclica y la clave 123456 el speedup es de 2.02, 3.00 y 3.90 con 2, 3 y 4
  procesos (eficiencia de 97.5 % a 100.8 %). La búsqueda casi no se comunica: un
  `MPI_Bcast` al inicio, un aviso al final y dos `MPI_Reduce`. Cada proceso
  prueba unas 4.37 M claves por segundo, igual que la versión secuencial.
- **El tiempo depende de la posición de la clave, no del número de procesos.**
  Con 4 procesos, b) y c) se estiman en ≈ 32.7 años con la cíclica y salen en
  ≈ 2 ms con bloques o dovetailing. Ese cambio no es un speedup de 10^12: es el
  mismo trabajo en otro orden. El speedup "aparente" contra la versión
  secuencial (Anexo 2) solo dice que la clave cayó al principio del recorrido
  de algún proceso.
- **Mejorar unas claves empeora otras.** El dovetailing con K = 16 resuelve
  a), b) y c) en menos de medio segundo, pero hace 123456 unas 16 veces más
  lenta que con bloques (0.45 s). Con K = 256 la penalización llega a 7.3 s.
  Ninguna estrategia estática mejora el tiempo promedio para una clave al azar
  (≈ 65 años con 4 procesos en esta máquina).
- **La terminación asíncrona cumple su objetivo.** Ningún proceso espera a los
  demás mientras busca, y el aviso de fin llega en ~1 ms (un lote de 4096
  claves del proceso 0).

### Retos y soluciones

| Reto | Solución |
|---|---|
| El Laboratorio 03 validaba con la etiqueta de AES-GCM, pero el enunciado pide una frase de búsqueda | Cambio a AES-128-CBC con IV aleatorio; cada candidata se acepta si el texto contiene "es una prueba de" y luego se verifica descifrando el mensaje completo |
| Búsquedas b) y c) imposibles de completar (décadas) | Opción `-t`: el programa se detiene, mide su rendimiento y estima el tiempo hasta la clave con `posicion_clave()` |
| Terminar sin que nadie se bloquee | Buzones `MPI_Irecv` revisados con `MPI_Test`; un solo reporte por proceso; `MPI_Cancel` + `MPI_Wait` del buzón pendiente antes de `MPI_Finalize` |
| La primera versión de la estrategia recorría las diagonales del emparejamiento de Cantor y repartía bloques pequeños por turnos: en la práctica conservaba el orden creciente de las claves y 2^54 seguía en el intento ~2^52 | Se reimplementó como en la propuesta: bloques contiguos por proceso, y dentro de cada uno K sub-bloques recorridos por turnos |
| Un `-K` enorme desbordaba `size * K` y la búsqueda no terminaba | `K` se limita al tamaño del bloque de cada proceso |
| Con 3 procesos 2^56 no se divide en partes iguales | Reparto casi uniforme (`inicio_parte` / `largo_parte`): los primeros bloques reciben una clave extra; un arnés verificó que ninguna clave se repite ni se omite |
| Comparar tiempos de milisegundos con ruido del sistema | 10 repeticiones por configuración (`medir_speedup.sh`), medición desde una `MPI_Barrier` y tiempo del último proceso (`MPI_MAX`) |

### Lo aprendido

- En la búsqueda por fuerza bruta la **distribución del trabajo** determina
  qué claves se encuentran rápido; el número de procesos solo divide el tiempo
  entre `p`.
- Un speedup enorme puede ser engañoso: hay que separar la ganancia por
  paralelismo (comparar con el mismo orden de recorrido) de la ganancia por
  reordenar el espacio.
- La comunicación no bloqueante exige más cuidado que una colectiva, pero
  elimina las esperas mientras se trabaja.
- Las pruebas exhaustivas en rangos pequeños (biyección del generador) dan
  confianza antes de lanzar búsquedas que no se pueden completar.

## Conclusiones

1. El tiempo de búsqueda depende de la **posición de la clave en el recorrido
   de su proceso**: con la cíclica crece linealmente con el valor de la clave
   (7.6 ms para 123456, ≈ 32.7 años para 2^56/4).
2. La versión paralela cíclica escala casi linealmente: speedup de **3.90 con 4
   procesos** (eficiencia del 97.5 %) en la prueba a).
3. La estrategia alternativa de bloques intercalados (dovetailing, K = 16)
   encuentra 2^56/4 y 2^56/4 + 1 en **≈ 2 ms**, sin agregar trabajo, a cambio
   de hacer K veces más lentas las claves pequeñas (0.45 s para 123456).
4. Ninguna distribución estática es la mejor para todas las claves: todas
   prueban las mismas 2^56 claves y tienen el mismo tiempo promedio; cada una
   favorece claves distintas, y el peor caso (por ejemplo 2^54 − 1) sigue
   existiendo.
5. La terminación asíncrona con `MPI_Isend` / `MPI_Irecv` / `MPI_Test`
   reemplaza la sincronización colectiva periódica sin bloquear a ningún
   proceso, con una latencia de aviso de ~1 ms.

## Recomendaciones

- Elegir la estrategia según lo que se sepa de la clave: cíclica si se esperan
  claves pequeñas y dovetailing con un `K` moderado (8 a 32) si la clave puede
  estar en cualquier parte del espacio.
- Probar un reparto **dinámico** (maestro–trabajadores que entregan trozos bajo
  demanda) o un orden pseudoaleatorio, para no depender de dónde caen los
  puntos de arranque con cada `p`.
- Ejecutar en varias máquinas reales (clúster) para medir el costo de red, que
  en una sola máquina es casi nulo.
- Ajustar el tamaño del lote (`INTERVALO_REVISION`) si se necesita menor
  latencia de terminación: 4096 claves equivale a ~1 ms por proceso.
