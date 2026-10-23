"""CC3069 Proyecto 02 - genera los diagramas del inciso 4 (PNG para el informe).

  distribucion_claves.png     como reparte cada estrategia 64 claves en 4 procesos
  flujo_busqueda_mpi.png      diagrama de flujo de busqueda_mpi.c
  comunicacion_terminacion.png mensajes de la terminacion asincrona

Uso: python diagrama/generar_diagramas.py   (requiere matplotlib)
"""

import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

DIR = os.path.dirname(os.path.abspath(__file__))

# Un color por proceso; cada celda lleva ademas su etiqueta (P0..P3).
PROCESOS = ["#2a78d6", "#eb6834", "#1baf7a", "#eda100"]
TINTA = "#0b0b0b"
TINTA_2 = "#52514e"
LINEA = "#8a8984"
SUPERFICIE = "#fcfcfb"
RESALTE = "#e34948"

CLAVES, P, K = 64, 4, 4


# ----------------------------------------------------------------------
# Las tres estrategias, igual que generador_siguiente() en busqueda_mpi.c
# ----------------------------------------------------------------------
def orden_ciclica():
    return [[c for c in range(r, CLAVES, P)] for r in range(P)]


def orden_bloques_intercalados(k):
    largo = CLAVES // P
    sub = largo // k
    orden = []
    for r in range(P):
        inicio = r * largo
        orden.append([inicio + fila * sub + col
                      for col in range(sub) for fila in range(k)])
    return orden


def dibujar_estrategia(ax, titulo, orden):
    dueno, intento = {}, {}
    for r, claves in enumerate(orden):
        for i, c in enumerate(claves):
            dueno[c], intento[c] = r, i + 1

    for c in range(CLAVES):
        fila, col = divmod(c, 16)
        y = 3 - fila
        ax.add_patch(Rectangle((col + 0.04, y + 0.04), 0.92, 0.92,
                               facecolor=PROCESOS[dueno[c]], edgecolor=SUPERFICIE,
                               linewidth=2))
        ax.text(col + 0.5, y + 0.64, f"P{dueno[c]}", ha="center", va="center",
                fontsize=7, color=TINTA)
        ax.text(col + 0.5, y + 0.3, f"#{intento[c]}", ha="center", va="center",
                fontsize=8.5, color=TINTA, fontweight="bold")

    # Equivalentes de 2^56/4 y 2^56/4 + 1 en el ejemplo (claves 16 y 17).
    for c in (16, 17):
        fila, col = divmod(c, 16)
        ax.add_patch(Rectangle((col + 0.02, 3 - fila + 0.02), 0.96, 0.96,
                               fill=False, edgecolor=RESALTE, linewidth=2.2))

    for fila in range(4):
        ax.text(-0.25, 3 - fila + 0.5, f"{fila * 16}–{fila * 16 + 15}",
                ha="right", va="center", fontsize=8, color=TINTA_2)
    ax.set_title(titulo, loc="left", fontsize=11, color=TINTA, pad=6)
    ax.set_xlim(-1.6, 16.05)
    ax.set_ylim(-0.05, 4.05)
    ax.set_aspect("equal")
    ax.axis("off")


def diagrama_distribucion():
    fig, axes = plt.subplots(3, 1, figsize=(11, 10.2), facecolor=SUPERFICIE)
    dibujar_estrategia(
        axes[0], "Cíclica (base): la clave c le toca al proceso c mod p",
        orden_ciclica())
    dibujar_estrategia(
        axes[1], "Bloques: cada proceso recorre en orden un bloque contiguo de 2^bits / p claves",
        orden_bloques_intercalados(1))
    dibujar_estrategia(
        axes[2], f"Cantor / bloques intercalados (K = {K}): una clave por sub-bloque, por turnos",
        orden_bloques_intercalados(K))

    fig.suptitle("Reparto de 64 claves entre 4 procesos MPI", x=0.06, ha="left",
                 fontsize=14, color=TINTA, fontweight="bold")
    fig.text(0.06, 0.935,
             "Cada celda es una clave (filas de 16). Color y etiqueta: proceso dueño. "
             "#n: intento en que ese proceso la prueba.\n"
             "Recuadro rojo: claves 16 y 17, equivalentes a 2^56/4 y 2^56/4 + 1 "
             "(incisos 2b y 2c).",
             fontsize=9, color=TINTA_2, va="top")
    fig.subplots_adjust(left=0.03, right=0.99, top=0.88, bottom=0.02, hspace=0.25)
    fig.savefig(os.path.join(DIR, "distribucion_claves.png"), dpi=160,
                facecolor=SUPERFICIE)
    plt.close(fig)


# ----------------------------------------------------------------------
# Diagrama de flujo
# ----------------------------------------------------------------------
ANCHO, ALTO = 3.4, 0.62


def caja(ax, x, y, texto, forma="proceso", ancho=ANCHO, alto=ALTO, relleno="#ffffff"):
    if forma == "decision":
        w, h = ancho / 2, alto * 0.85
        ax.add_patch(Polygon([(x, y + h), (x + w, y), (x, y - h), (x - w, y)],
                             closed=True, facecolor="#fff4d6", edgecolor=TINTA,
                             linewidth=1.2))
    elif forma == "terminal":
        ax.add_patch(FancyBboxPatch((x - ancho / 2, y - alto / 2), ancho, alto,
                                    boxstyle=f"round,pad=0,rounding_size={min(0.3, alto / 2)}",
                                    facecolor="#e8e7e3", edgecolor=TINTA, linewidth=1.2))
    elif forma == "mpi":
        ax.add_patch(Rectangle((x - ancho / 2, y - alto / 2), ancho, alto,
                               facecolor="#dceafb", edgecolor=TINTA, linewidth=1.2))
    else:
        ax.add_patch(Rectangle((x - ancho / 2, y - alto / 2), ancho, alto,
                               facecolor=relleno, edgecolor=TINTA, linewidth=1.2))
    ax.text(x, y, texto, ha="center", va="center", fontsize=8.3, color=TINTA,
            linespacing=1.25)


def flecha(ax, puntos, etiqueta=None, pos_etiqueta=None):
    for (x0, y0), (x1, y1) in zip(puntos[:-2], puntos[1:-1]):
        ax.plot([x0, x1], [y0, y1], color=TINTA, linewidth=1.1)
    ax.annotate("", xy=puntos[-1], xytext=puntos[-2],
                arrowprops=dict(arrowstyle="-|>", color=TINTA, linewidth=1.1,
                                shrinkA=0, shrinkB=0))
    if etiqueta:
        ax.text(*pos_etiqueta, etiqueta, fontsize=8, color=TINTA_2, ha="center",
                va="center", fontweight="bold")


def diagrama_flujo():
    fig, ax = plt.subplots(figsize=(11, 15), facecolor=SUPERFICIE)
    ax.set_xlim(0, 11)
    ax.set_ylim(-0.1, 15.4)
    ax.axis("off")

    xc = 4.2
    caja(ax, xc, 14.9, "Inicio: MPI_Init, MPI_Comm_rank, MPI_Comm_size", "terminal")
    caja(ax, xc, 14.0, "Leer opciones: -f -k -b -t -s -K\n(validación defensiva)")
    caja(ax, xc, 12.95, "¿rank == 0?", "decision")
    caja(ax, 8.4, 12.95, "Leer el .txt, cifrar con\nAES-128-CBC (IV aleatorio)",
         ancho=3.0)
    caja(ax, xc, 11.9, "MPI_Bcast(texto cifrado + IV)", "mpi")
    caja(ax, xc, 11.05, "MPI_Barrier;  t0 = MPI_Wtime()", "mpi")
    caja(ax, xc, 10.2, "generador_init según -s:\ncíclica | bloques | cantor (K)")
    caja(ax, xc, 9.15, "Probar hasta 4096 candidatas:\ndescifrar y buscar \"es una prueba de\"")
    caja(ax, xc, 8.0, "¿Encontró la clave o\nagotó rango / -t?", "decision", alto=0.75)
    caja(ax, 8.4, 8.0, "Reportar una sola vez:\nMPI_Isend(TAG_REPORTE → 0)\n"
         "(el proceso 0 se cuenta solo)", "mpi", ancho=3.0, alto=0.85)
    caja(ax, xc, 6.35, "¿rank == 0?", "decision")
    caja(ax, 1.75, 5.2, "MPI_Test(buzón TAG_FIN)", "mpi", ancho=2.9)
    caja(ax, 1.75, 4.05, "¿Llegó el FIN?", "decision", ancho=2.9)
    caja(ax, 6.65, 5.2, "MPI_Test(buzón TAG_REPORTE)\ncontar reportes", "mpi", ancho=3.3)
    caja(ax, 6.65, 4.05, "¿Hay clave o\nllegaron p reportes?", "decision", ancho=3.3,
         alto=0.75)
    caja(ax, 6.65, 2.85, "MPI_Isend(TAG_FIN, decisión)\na los procesos 1..p-1", "mpi",
         ancho=3.3)
    caja(ax, xc, 1.85, "Limpiar peticiones (MPI_Cancel / MPI_Wait)\n"
         "MPI_Reduce(tiempo MAX, candidatas SUM)", "mpi", ancho=4.4, alto=0.72)
    caja(ax, xc, 0.95, "Proceso 0: imprime clave, texto recuperado,\n"
         "tiempo y estimación si se cortó con -t", ancho=4.4, alto=0.72)
    caja(ax, xc, 0.22, "MPI_Finalize", "terminal", alto=0.4)

    flecha(ax, [(xc, 14.59), (xc, 14.31)])
    flecha(ax, [(xc, 13.69), (xc, 13.48)])
    flecha(ax, [(xc + 1.7, 12.95), (6.9, 12.95)], "sí", (6.25, 13.12))
    flecha(ax, [(8.4, 12.64), (8.4, 11.9), (xc + 1.7, 11.9)])
    flecha(ax, [(xc, 12.42), (xc, 12.21)], "no", (xc + 0.25, 12.32))
    flecha(ax, [(xc, 11.59), (xc, 11.36)])
    flecha(ax, [(xc, 10.74), (xc, 10.51)])
    flecha(ax, [(xc, 9.89), (xc, 9.46)])
    flecha(ax, [(xc, 8.84), (xc, 8.64)])
    flecha(ax, [(xc + 1.7, 8.0), (6.9, 8.0)], "sí", (6.25, 8.17))
    flecha(ax, [(xc, 7.36), (xc, 6.88)], "no", (xc + 0.25, 7.2))
    flecha(ax, [(8.4, 7.575), (8.4, 7.12), (xc + 0.02, 7.12)])
    flecha(ax, [(xc - 1.7, 6.35), (1.75, 6.35), (1.75, 5.51)], "no", (2.4, 6.52))
    flecha(ax, [(xc + 1.7, 6.35), (6.65, 6.35), (6.65, 5.51)], "sí", (6.0, 6.52))
    flecha(ax, [(1.75, 4.89), (1.75, 4.58)])
    flecha(ax, [(6.65, 4.89), (6.65, 4.69)])
    flecha(ax, [(6.65, 3.41), (6.65, 3.16)], "sí", (6.9, 3.3))

    # Ramas "no": vuelven al siguiente lote.
    retorno = 0.25
    flecha(ax, [(1.75 - 1.45, 4.05), (retorno, 4.05), (retorno, 9.15),
                (xc - 1.7, 9.15)], "no", (0.55, 4.22))
    flecha(ax, [(6.65 + 1.65, 4.05), (10.6, 4.05), (10.6, 9.15), (xc + 1.7, 9.15)],
           "no", (8.65, 4.22))
    ax.text(10.75, 6.6, "siguiente lote", rotation=90, fontsize=8, color=TINTA_2,
            va="center")
    ax.text(0.1, 6.6, "siguiente lote", rotation=90, fontsize=8, color=TINTA_2,
            va="center")

    # Ambas salidas de "detener" van a la limpieza.
    flecha(ax, [(1.75, 3.52), (1.75, 1.85), (xc - 2.2, 1.85)], "sí", (2.0, 3.35))
    flecha(ax, [(6.65, 2.54), (6.65, 2.4), (xc + 0.8, 2.4), (xc + 0.8, 2.21)])
    flecha(ax, [(xc, 1.49), (xc, 1.31)])
    flecha(ax, [(xc, 0.59), (xc, 0.42)])

    ax.text(0.1, 15.3, "Diagrama de flujo de busqueda_mpi.c (las tres estrategias "
            "comparten el ciclo; solo cambia el generador)", fontsize=11,
            color=TINTA, fontweight="bold")
    ax.add_patch(Rectangle((7.6, 10.35), 0.3, 0.2, facecolor="#dceafb", edgecolor=TINTA))
    ax.text(8.0, 10.45, "llamada a Open MPI", fontsize=8, color=TINTA_2, va="center")
    ax.add_patch(Rectangle((7.6, 10.0), 0.3, 0.2, facecolor="#fff4d6", edgecolor=TINTA))
    ax.text(8.0, 10.1, "decisión", fontsize=8, color=TINTA_2, va="center")
    fig.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.01)
    fig.savefig(os.path.join(DIR, "flujo_busqueda_mpi.png"), dpi=150,
                facecolor=SUPERFICIE)
    plt.close(fig)


# ----------------------------------------------------------------------
# Diagrama de secuencia de la terminación asíncrona
# ----------------------------------------------------------------------
def diagrama_comunicacion():
    fig, ax = plt.subplots(figsize=(11, 7.2), facecolor=SUPERFICIE)
    ax.set_xlim(0, 11)
    ax.set_ylim(0, 7.4)
    ax.axis("off")

    xs = [1.5, 4.0, 6.5, 9.0]
    for r, x in enumerate(xs):
        ax.add_patch(FancyBboxPatch((x - 0.6, 6.45), 1.2, 0.5,
                                    boxstyle="round,pad=0,rounding_size=0.1",
                                    facecolor=PROCESOS[r], edgecolor=TINTA))
        ax.text(x, 6.7, f"Proceso {r}", ha="center", va="center", fontsize=9,
                color=TINTA, fontweight="bold")
        ax.plot([x, x], [0.5, 6.45], color=LINEA, linewidth=1, linestyle=(0, (4, 3)))

    def mensaje(x0, x1, y, texto, color=TINTA, dy=0.0):
        ax.annotate("", xy=(x1, y - dy), xytext=(x0, y),
                    arrowprops=dict(arrowstyle="-|>", color=color, linewidth=1.4))
        ax.text((x0 + x1) / 2, y + 0.12 - dy / 2, texto, ha="center", va="bottom",
                fontsize=8, color=TINTA)

    def nota(x, y, texto, ancho=2.2):
        ax.add_patch(Rectangle((x - ancho / 2, y - 0.22), ancho, 0.44,
                               facecolor="#ffffff", edgecolor=LINEA, zorder=3))
        ax.text(x, y, texto, ha="center", va="center", fontsize=7.6, color=TINTA,
                zorder=4)

    for x in xs[1:]:
        mensaje(xs[0], x, 5.9, "", LINEA, 0.0)
    ax.text(5.25, 6.02, "MPI_Bcast: texto cifrado + IV", ha="center", fontsize=8,
            color=TINTA)
    for x in xs:
        nota(x, 5.35, "Irecv (buzón) + lotes\nde 4096 + MPI_Test", 2.0)

    nota(xs[1], 4.45, "¡encontró la clave!", 1.8)
    mensaje(xs[1], xs[0], 3.95, "MPI_Isend TAG_REPORTE (clave)")
    nota(xs[0], 3.35, "MPI_Test: llegó reporte\n→ decide", 2.0)
    for x in xs[1:]:
        mensaje(xs[0], x, 2.75 - 0.0 * x, "")
    ax.text(5.25, 2.87, "MPI_Isend TAG_FIN (clave) a 1..p-1", ha="center",
            fontsize=8, color=TINTA)
    for x in xs[2:]:
        nota(x, 2.2, "MPI_Test: llegó FIN\n→ detener", 1.9)
    nota(xs[1], 2.2, "llega FIN → detener", 1.9)
    nota(xs[0], 2.2, "Cancel/Wait del buzón", 1.9)
    for x in xs[1:]:
        ax.annotate("", xy=(xs[0], 1.25), xytext=(x, 1.45),
                    arrowprops=dict(arrowstyle="-|>", color=LINEA, linewidth=1.1))
    ax.text(5.25, 1.55, "MPI_Reduce: tiempo (MAX) y candidatas (SUM)", ha="center",
            fontsize=8, color=TINTA)
    nota(xs[0], 0.85, "imprime resultado", 1.9)

    ax.text(0.2, 7.15, "Terminación asíncrona: nadie se bloquea mientras busca",
            fontsize=12, color=TINTA, fontweight="bold")
    ax.text(0.2, 0.15, "Si nadie encuentra la clave (rango agotado o -t), cada proceso "
            "manda un REPORTE vacío; el proceso 0 difunde FIN al reunir p reportes.",
            fontsize=8, color=TINTA_2)
    fig.subplots_adjust(left=0.01, right=0.99, top=0.99, bottom=0.01)
    fig.savefig(os.path.join(DIR, "comunicacion_terminacion.png"), dpi=160,
                facecolor=SUPERFICIE)
    plt.close(fig)


if __name__ == "__main__":
    diagrama_distribucion()
    diagrama_flujo()
    diagrama_comunicacion()
    print("Diagramas generados en", DIR)
