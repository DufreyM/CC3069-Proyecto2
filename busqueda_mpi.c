/* CC3069 Proyecto 02 - busqueda_mpi.c
 * Busqueda paralela por fuerza bruta de una clave AES-128-CBC con Open MPI.
 * Estrategia base (ciclica): el proceso r prueba r, r+p, r+2p, ... y los
 * procesos sincronizan con MPI_Allreduce. La estrategia alternativa
 * (bloques/dovetailing) se agrega en buscar() via la opcion -s.
 * Uso: mpirun -np 4 ./busqueda_mpi [-f archivo] [-k clave] [-b bits] [-t seg] [-s ciclica] */

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

#ifndef INTERVALO_REVISION
#define INTERVALO_REVISION 4096
#endif

typedef struct {
    const char *archivo;
    uint64_t secreto;
    int bits;
    uint64_t total;
    double limite;
} opciones;

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
        " [-t segundos] [-s ciclica]\n"
        "  -f archivo   texto a cifrar (por defecto %s)\n"
        "  -k clave     clave secreta en decimal (por defecto %" PRIu64 ")\n"
        "  -b bits      explora 2^bits candidatas (1-%d, por defecto %d)\n"
        "  -t segundos  limite de tiempo; al cumplirse se estima el total\n"
        "  -s ciclica   estrategia de distribucion (por defecto ciclica)\n",
        programa, ARCHIVO_POR_DEFECTO, CLAVE_POR_DEFECTO, MAX_BITS, MAX_BITS);
}

static int leer_opciones(int argc, char *argv[], opciones *opt)
{
    int opcion;
    uint64_t valor;

    opt->archivo = ARCHIVO_POR_DEFECTO;
    opt->secreto = CLAVE_POR_DEFECTO;
    opt->bits = MAX_BITS;
    opt->limite = 0.0;

    while ((opcion = getopt(argc, argv, "f:k:b:t:s:")) != -1) {
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
            if (strcmp(optarg, "ciclica") != 0) {
                return 0;
            }
            break;
        default:
            return 0;
        }
    }

    if (optind != argc) {
        return 0;
    }

    opt->total = UINT64_C(1) << opt->bits;
    return 1;
}

static uint64_t buscar_ciclica(
    EVP_CIPHER_CTX *ctx, const texto_cifrado *cifrado,
    int rank, int size, const opciones *opt, double inicio, uint64_t *probadas)
{
    unsigned char plano[MAX_CIFRADO];
    uint64_t paso = (uint64_t)size;
    uint64_t candidata = (uint64_t)rank;
    uint64_t encontrada_local = CLAVE_NO_ENCONTRADA;
    uint64_t encontrada = CLAVE_NO_ENCONTRADA;
    uint64_t locales = 0;
    int detener = 0;

    while (!detener) {
        for (int i = 0;
             i < INTERVALO_REVISION &&
             candidata < opt->total &&
             encontrada_local == CLAVE_NO_ENCONTRADA;
             i++) {
            int resultado = probar_clave(ctx, candidata, cifrado, plano);

            locales++;

            if (resultado < 0) {
                fallar("OpenSSL fallo al descifrar.");
            }

            if (resultado == 1) {
                encontrada_local = candidata;
            }

            candidata += paso;
        }

        MPI_Allreduce(&encontrada_local, &encontrada, 1,
                      MPI_UINT64_T, MPI_MIN, MPI_COMM_WORLD);

        if (encontrada != CLAVE_NO_ENCONTRADA) {
            detener = 1;
        } else {
            int local_fin = candidata >= opt->total;

            if (opt->limite > 0.0 && MPI_Wtime() - inicio >= opt->limite) {
                local_fin = 1;
            }

            int todos_fin = 0;

            MPI_Allreduce(&local_fin, &todos_fin, 1,
                          MPI_INT, MPI_LAND, MPI_COMM_WORLD);
            detener = todos_fin;
        }
    }

    *probadas = locales;
    return encontrada;
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
        printf("Ejecucion: paralela (Open MPI, %d procesos, estrategia ciclica)\n", size);
        printf("Archivo: %s (%zu bytes)\n", opt.archivo, largo_texto);
        printf("Clave secreta: %" PRIu64 "\n", opt.secreto);
        imprimir_hex("IV (hex): ", cifrado.iv, IV_LEN);
        imprimir_hex("Texto cifrado (hex): ", cifrado.datos, cifrado.longitud);
        printf("Frase de busqueda: \"%s\"\n", FRASE_BUSQUEDA);
        printf("Rango explorado: 2^%d = %" PRIu64 " candidatas\n", opt.bits, opt.total);

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

    uint64_t encontrada = buscar_ciclica(
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
            printf("Clave encontrada: %" PRIu64 " (proceso %" PRIu64 ")\n",
                   encontrada, encontrada % (uint64_t)size);
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

        /* La clave k la prueba el proceso k%p en su intento (k/p)+1. */
        if (encontrada == CLAVE_NO_ENCONTRADA &&
            rendimiento > 0.0 && opt.secreto < opt.total) {
            double intentos_proceso = (double)(opt.secreto / (uint64_t)size) + 1.0;
            double ritmo_proceso = rendimiento / (double)size;

            printf("Intentos del proceso dueno: %.0f\n", intentos_proceso);
            imprimir_duracion("Tiempo estimado: ", intentos_proceso / ritmo_proceso);
        }
    }

    EVP_CIPHER_CTX_free(ctx);
    MPI_Finalize();

    return EXIT_SUCCESS;
}
