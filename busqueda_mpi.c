/* CC3069 Proyecto 02 - busqueda_mpi.c
 * Busqueda paralela por fuerza bruta de una clave AES-128-CBC con Open MPI.
 *
 * Estrategias de distribucion (opcion -s):
 *   ciclica  el proceso r prueba r, r+p, r+2p, ... (por defecto).
 *   bloques  bloques intercalados de tamano K: el bloque global g lo prueba
 *            el proceso g%p; cada proceso recorre [g*K, g*K+K).
 *   cantor   dovetailing: la clave n cae en la anti-diagonal w del
 *            emparejamiento de Cantor; el proceso r recorre las diagonales
 *            w = r, r+p, r+2p, ... Asi el trabajo se reparte en franjas
 *            diagonales y se exploran primero las claves pequenas.
 *
 * Terminacion ASINCRONA (incisos 3 y 4): no hay MPI_Allreduce por lote.
 * Cada proceso avisa con MPI_Isend y sondea con MPI_Irecv / MPI_Test:
 *   - cuando termina (encuentra la clave o agota su rango/tiempo) manda un
 *     REPORTE al proceso 0;
 *   - el proceso 0 cuenta reportes y, al hallar la clave o reunir p reportes,
 *     difunde el FALLO/EXITO con la decision final;
 *   - todos sondean ese aviso con MPI_Test y se detienen sin bloquearse.
 *
 * Uso: mpirun -np 4 ./busqueda_mpi [-f archivo] [-k clave] [-b bits]
 *            [-t seg] [-s ciclica|bloques|cantor] [-K tam_bloque]
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <mpi.h>
#include <openssl/err.h>

#include "comun.h"

#define CLAVE_POR_DEFECTO UINT64_C(123456)
#define BLOQUE_POR_DEFECTO UINT64_C(1024)

#define TAG_REPORTE 101
#define TAG_FIN 102

#ifndef INTERVALO_REVISION
#define INTERVALO_REVISION 4096
#endif

typedef enum {
    EST_CICLICA = 0,
    EST_BLOQUES,
    EST_CANTOR
} estrategia_t;

typedef struct {
    const char *archivo;
    uint64_t secreto;
    int bits;
    uint64_t total;
    double limite;
    estrategia_t estrategia;
    uint64_t bloque;
} opciones;

/* Generador perezoso: entrega la siguiente candidata segun la estrategia.
 * Devuelve 1 si escribio una candidata valida en *salida, 0 si se agoto. */
typedef struct {
    estrategia_t estrategia;
    uint64_t total;
    /* ciclica */
    uint64_t candidata;
    uint64_t paso;
    /* bloques */
    uint64_t b_inicio;
    uint64_t b_actual;
    uint64_t b_resto;
    uint64_t b_salto;
    /* cantor */
    uint64_t diag;
    uint64_t diag_pos;
} generador;

static uint64_t minimo(uint64_t a, uint64_t b)
{
    return a < b ? a : b;
}

static void generador_init(
    generador *g, estrategia_t estrategia, int rank, int size,
    uint64_t bloque, uint64_t total)
{
    memset(g, 0, sizeof(*g));
    g->estrategia = estrategia;
    g->total = total;

    switch (estrategia) {
    case EST_CICLICA:
        g->candidata = (uint64_t)rank;
        g->paso = (uint64_t)size;
        break;
    case EST_BLOQUES:
        g->b_salto = (uint64_t)size * bloque;
        g->b_inicio = (uint64_t)rank * bloque;
        g->b_actual = g->b_inicio;
        g->b_resto = g->b_inicio < total
            ? minimo(bloque, total - g->b_inicio) : 0;
        g->paso = bloque;
        break;
    case EST_CANTOR:
        g->diag = (uint64_t)rank;
        g->diag_pos = 0;
        g->paso = (uint64_t)size;
        break;
    }
}

static int generador_siguiente(generador *g, uint64_t *salida)
{
    switch (g->estrategia) {
    case EST_CICLICA:
        if (g->candidata >= g->total) {
            return 0;
        }
        *salida = g->candidata;
        g->candidata += g->paso;
        return 1;

    case EST_BLOQUES:
        if (g->b_resto == 0) {
            g->b_inicio += g->b_salto;
            if (g->b_inicio >= g->total) {
                return 0;
            }
            g->b_actual = g->b_inicio;
            g->b_resto = minimo(g->paso, g->total - g->b_inicio);
        }
        *salida = g->b_actual;
        g->b_actual++;
        g->b_resto--;
        return 1;

    case EST_CANTOR:
        for (;;) {
            uint64_t inicio = g->diag * (g->diag + 1) / 2;
            if (inicio >= g->total) {
                return 0;
            }
            if (g->diag_pos > g->diag) {
                g->diag += g->paso;
                g->diag_pos = 0;
                continue;
            }
            uint64_t clave = inicio + g->diag_pos;
            g->diag_pos++;
            if (clave >= g->total) {
                continue;
            }
            *salida = clave;
            return 1;
        }
    }

    return 0;
}

static const char *nombre_estrategia(estrategia_t estrategia)
{
    switch (estrategia) {
    case EST_BLOQUES: return "bloques";
    case EST_CANTOR:  return "cantor (dovetailing)";
    case EST_CICLICA:
    default:          return "ciclica";
    }
}

static void fallar(const char *descripcion)
{
    fprintf(stderr, "Error: %s\n", descripcion);
    ERR_print_errors_fp(stderr);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    exit(EXIT_FAILURE);
}

static void uso(const char *programa)
{
    fprintf(stderr,
        "Uso: mpirun -np <p> %s [-f archivo] [-k clave] [-b bits]"
        " [-t segundos] [-s ciclica|bloques|cantor] [-K tam_bloque]\n"
        "  -f archivo   texto a cifrar (por defecto %s)\n"
        "  -k clave     clave secreta en decimal (por defecto %" PRIu64 ")\n"
        "  -b bits      explora 2^bits candidatas (1-%d, por defecto %d)\n"
        "  -t segundos  limite de tiempo; al cumplirse se estima el total\n"
        "  -s modo      ciclica (defecto), bloques o cantor\n"
        "  -K tam       tamano de bloque para -s bloques (por defecto %" PRIu64 ")\n",
        programa, ARCHIVO_POR_DEFECTO, CLAVE_POR_DEFECTO, MAX_BITS, MAX_BITS,
        BLOQUE_POR_DEFECTO);
}

static int leer_opciones(int argc, char *argv[], opciones *opt)
{
    int opcion;
    uint64_t valor;

    opt->archivo = ARCHIVO_POR_DEFECTO;
    opt->secreto = CLAVE_POR_DEFECTO;
    opt->bits = MAX_BITS;
    opt->limite = 0.0;
    opt->estrategia = EST_CICLICA;
    opt->bloque = BLOQUE_POR_DEFECTO;

    while ((opcion = getopt(argc, argv, "f:k:b:t:s:K:")) != -1) {
        switch (opcion) {
        case 'f':
            opt->archivo = optarg;
            break;
        case 'k':
            if (!parse_u64(optarg, &opt->secreto)) {
                return 0;
            }
            break;
        case 'b':
            if (!parse_u64(optarg, &valor) || valor < 1 || valor > MAX_BITS) {
                return 0;
            }
            opt->bits = (int)valor;
            break;
        case 't':
            if (!parse_u64(optarg, &valor) || valor == 0) {
                return 0;
            }
            opt->limite = (double)valor;
            break;
        case 's':
            if (strcmp(optarg, "ciclica") == 0) {
                opt->estrategia = EST_CICLICA;
            } else if (strcmp(optarg, "bloques") == 0) {
                opt->estrategia = EST_BLOQUES;
            } else if (strcmp(optarg, "cantor") == 0) {
                opt->estrategia = EST_CANTOR;
            } else {
                return 0;
            }
            break;
        case 'K':
            if (!parse_u64(optarg, &valor) || valor == 0) {
                return 0;
            }
            opt->bloque = valor;
            break;
        default:
            return 0;
        }
    }

    if (optind != argc) {
        return 0;
    }

    opt->total = UINT64_C(1) << opt->bits;

    /* Un bloque mayor al rango no aporta nada y desborda size*K en el
     * generador (la busqueda nunca terminaria). */
    if (opt->bloque > opt->total) {
        opt->bloque = opt->total;
    }
    return 1;
}

/* Busqueda con terminacion asincrona (MPI_Isend/MPI_Irecv/MPI_Test).
 * Cada proceso prueba candidatas de su generador; cuando termina avisa al
 * proceso 0, que decide y difunde el resultado. */
static uint64_t buscar(
    EVP_CIPHER_CTX *ctx, const texto_cifrado *cifrado,
    int rank, int size, const opciones *opt, double inicio, uint64_t *probadas)
{
    unsigned char plano[MAX_CIFRADO];
    generador gen;
    generador_init(&gen, opt->estrategia, rank, size, opt->bloque, opt->total);

    uint64_t encontrada_local = CLAVE_NO_ENCONTRADA;
    uint64_t decision = CLAVE_NO_ENCONTRADA;
    uint64_t locales = 0;
    int agotado = 0;
    int reporte_enviado = 0;
    int detener = 0;

    /* Canal de reporte hacia el proceso 0 (cada proceso lo usa una vez). */
    uint64_t reporte_valor = CLAVE_NO_ENCONTRADA;
    MPI_Request req_reporte = MPI_REQUEST_NULL;

    /* El proceso 0 escucha reportes de todos; los demas escuchan el FIN. */
    uint64_t buzon = CLAVE_NO_ENCONTRADA;
    MPI_Request req_escucha = MPI_REQUEST_NULL;
    int reportados = 0;
    uint64_t clave_reportada = CLAVE_NO_ENCONTRADA;

    /* Buffers del proceso 0 para difundir la decision final. */
    MPI_Request *req_fin = NULL;
    uint64_t fin_valor = CLAVE_NO_ENCONTRADA;

    if (rank == 0) {
        MPI_Irecv(&buzon, 1, MPI_UINT64_T, MPI_ANY_SOURCE,
                  TAG_REPORTE, MPI_COMM_WORLD, &req_escucha);
        req_fin = malloc(sizeof(MPI_Request) * (size_t)size);
        if (req_fin == NULL) {
            fallar("sin memoria para la terminacion asincrona.");
        }
        for (int i = 0; i < size; i++) {
            req_fin[i] = MPI_REQUEST_NULL;
        }
    } else {
        MPI_Irecv(&decision, 1, MPI_UINT64_T, 0,
                  TAG_FIN, MPI_COMM_WORLD, &req_escucha);
    }

    while (!detener) {
        /* 1. Probar un lote de candidatas (si aun queda trabajo). */
        if (!agotado && encontrada_local == CLAVE_NO_ENCONTRADA) {
            for (int i = 0; i < INTERVALO_REVISION; i++) {
                uint64_t candidata;

                if (!generador_siguiente(&gen, &candidata)) {
                    agotado = 1;
                    break;
                }

                int resultado = probar_clave(ctx, candidata, cifrado, plano);
                locales++;

                if (resultado < 0) {
                    fallar("OpenSSL fallo al descifrar.");
                }
                if (resultado == 1) {
                    encontrada_local = candidata;
                    break;
                }
            }

            if (opt->limite > 0.0 && MPI_Wtime() - inicio >= opt->limite) {
                agotado = 1;
            }
        }

        /* 2. Si este proceso ya termino, reportar una sola vez al proceso 0. */
        if (!reporte_enviado &&
            (encontrada_local != CLAVE_NO_ENCONTRADA || agotado)) {
            reporte_enviado = 1;
            reporte_valor = encontrada_local;

            if (rank == 0) {
                /* El proceso 0 se contabiliza a si mismo sin mensajearse. */
                reportados++;
                if (reporte_valor != CLAVE_NO_ENCONTRADA) {
                    clave_reportada = reporte_valor;
                }
            } else {
                MPI_Isend(&reporte_valor, 1, MPI_UINT64_T, 0,
                          TAG_REPORTE, MPI_COMM_WORLD, &req_reporte);
            }
        }

        /* 3. Proceso 0: recoger reportes y, al decidir, difundir el FIN. */
        if (rank == 0) {
            int llego = 0;
            MPI_Test(&req_escucha, &llego, MPI_STATUS_IGNORE);
            if (llego) {
                reportados++;
                if (buzon != CLAVE_NO_ENCONTRADA) {
                    clave_reportada = buzon;
                }
                MPI_Irecv(&buzon, 1, MPI_UINT64_T, MPI_ANY_SOURCE,
                          TAG_REPORTE, MPI_COMM_WORLD, &req_escucha);
            }

            if (clave_reportada != CLAVE_NO_ENCONTRADA || reportados >= size) {
                decision = clave_reportada;
                fin_valor = decision;
                for (int d = 1; d < size; d++) {
                    MPI_Isend(&fin_valor, 1, MPI_UINT64_T, d,
                              TAG_FIN, MPI_COMM_WORLD, &req_fin[d]);
                }
                detener = 1;
            }
        } else {
            /* 4. Los demas sondean el aviso de FIN sin bloquearse. */
            int llego = 0;
            MPI_Test(&req_escucha, &llego, MPI_STATUS_IGNORE);
            if (llego) {
                detener = 1;
            }
        }
    }

    /* Limpieza de las peticiones pendientes. */
    if (rank == 0) {
        MPI_Cancel(&req_escucha);
        MPI_Wait(&req_escucha, MPI_STATUS_IGNORE);
        for (int d = 1; d < size; d++) {
            if (req_fin[d] != MPI_REQUEST_NULL) {
                MPI_Wait(&req_fin[d], MPI_STATUS_IGNORE);
            }
        }
        free(req_fin);
    } else {
        if (req_reporte != MPI_REQUEST_NULL) {
            MPI_Wait(&req_reporte, MPI_STATUS_IGNORE);
        }
        /* req_escucha ya se completo al recibir el FIN (detener == 1). */
    }

    *probadas = locales;
    return decision;
}

int main(int argc, char *argv[])
{
    int rank;
    int size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    opciones opt;

    opterr = 0;

    if (!leer_opciones(argc, argv, &opt)) {
        if (rank == 0) {
            uso(argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    texto_cifrado cifrado;
    size_t largo_texto = 0;

    memset(&cifrado, 0, sizeof(cifrado));

    if (rank == 0) {
        unsigned char texto[MAX_TEXTO];
        resultado_lectura lectura = leer_archivo(
            opt.archivo, texto, sizeof(texto), &largo_texto);

        if (lectura != LECTURA_OK) {
            fprintf(stderr, "Error con '%s': %s.\n",
                    opt.archivo, descripcion_lectura(lectura));
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        if (!cifrar_cbc(opt.secreto, texto, (int)largo_texto, &cifrado)) {
            fallar("no se pudo cifrar el texto.");
        }

        unsigned char clave_aes[KEY_LEN];

        make_key(opt.secreto, clave_aes);
        printf("Ejecucion: paralela (Open MPI, %d procesos, estrategia %s)\n",
               size, nombre_estrategia(opt.estrategia));
        printf("Archivo: %s (%zu bytes)\n", opt.archivo, largo_texto);
        printf("Clave secreta: %" PRIu64 "\n", opt.secreto);
        imprimir_hex("IV (hex): ", cifrado.iv, IV_LEN);
        imprimir_hex("Texto cifrado (hex): ", cifrado.datos, cifrado.longitud);
        printf("Frase de busqueda: \"%s\"\n", FRASE_BUSQUEDA);
        printf("Rango explorado: 2^%d = %" PRIu64 " candidatas\n",
               opt.bits, opt.total);
        if (opt.estrategia == EST_BLOQUES) {
            printf("Tamano de bloque (K): %" PRIu64 "\n", opt.bloque);
        }

        if (opt.secreto >= opt.total) {
            printf("Aviso: la clave secreta esta fuera del rango explorado.\n");
        }

        OPENSSL_cleanse(clave_aes, sizeof(clave_aes));
    }

    MPI_Bcast(&cifrado, (int)sizeof(cifrado), MPI_BYTE, 0, MPI_COMM_WORLD);

    EVP_CIPHER_CTX *ctx = crear_contexto_busqueda();

    if (ctx == NULL) {
        fallar("no se pudo crear el contexto de OpenSSL.");
    }

    uint64_t probadas = 0;

    MPI_Barrier(MPI_COMM_WORLD);
    double inicio = MPI_Wtime();

    uint64_t encontrada = buscar(
        ctx, &cifrado, rank, size, &opt, inicio, &probadas);

    double transcurrido_local = MPI_Wtime() - inicio;
    double transcurrido = 0.0;
    uint64_t probadas_total = 0;

    MPI_Reduce(&transcurrido_local, &transcurrido, 1,
               MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&probadas, &probadas_total, 1,
               MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        double rendimiento = transcurrido > 0.0
            ? (double)probadas_total / transcurrido : 0.0;

        if (encontrada != CLAVE_NO_ENCONTRADA) {
            unsigned char recuperado[MAX_CIFRADO];
            int largo_recuperado = 0;

            if (!descifrar_cbc(encontrada, &cifrado, recuperado, &largo_recuperado)) {
                fallar("la clave encontrada no descifra el texto completo.");
            }

            printf("Resultado: ENCONTRADA\n");
            printf("Clave encontrada: %" PRIu64 "\n", encontrada);
            printf("Texto recuperado: %.*s\n",
                   largo_recuperado, (const char *)recuperado);
            printf("Verificacion: %s\n",
                   encontrada == opt.secreto
                       ? "coincide con la clave usada para cifrar"
                       : "NO coincide con la clave usada para cifrar");
        } else {
            printf("Resultado: no se encontro la clave"
                   " (rango agotado o limite de tiempo)\n");
        }

        printf("Candidatas probadas: %" PRIu64 "\n", probadas_total);
        printf("Tiempo: %.6f segundos\n", transcurrido);
        printf("Rendimiento: %.0f claves/s\n", rendimiento);

        if (encontrada == CLAVE_NO_ENCONTRADA &&
            rendimiento > 0.0 && opt.secreto < opt.total) {
            imprimir_duracion("Tiempo estimado (rango completo): ",
                              (double)opt.total / rendimiento);
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
