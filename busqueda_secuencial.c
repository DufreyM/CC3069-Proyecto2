/* CC3069 Proyecto 02 - busqueda_secuencial.c
 * Busqueda secuencial por fuerza bruta de una clave AES-128-CBC.
 * Referencia para el calculo de Speedup.
 * Uso: ./busqueda_secuencial [-f archivo] [-k clave] [-b bits] [-t segundos] */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <openssl/err.h>

#include "comun.h"

#define CLAVE_POR_DEFECTO UINT64_C(123456)
#define MASCARA_RELOJ UINT64_C(0xFFFF)

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
    exit(EXIT_FAILURE);
}

static double segundos_actuales(void)
{
    struct timespec ahora;

    clock_gettime(CLOCK_MONOTONIC, &ahora);
    return (double)ahora.tv_sec + (double)ahora.tv_nsec / 1e9;
}

static void uso(const char *programa)
{
    fprintf(stderr,
        "Uso: %s [-f archivo] [-k clave] [-b bits] [-t segundos]\n"
        "  -f archivo   texto a cifrar (por defecto %s)\n"
        "  -k clave     clave secreta en decimal (por defecto %" PRIu64 ")\n"
        "  -b bits      explora 2^bits candidatas (1-%d, por defecto %d)\n"
        "  -t segundos  limite de tiempo; al cumplirse se estima el total\n",
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

    while ((opcion = getopt(argc, argv, "f:k:b:t:")) != -1) {
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

int main(int argc, char *argv[])
{
    opciones opt;

    opterr = 0;

    if (!leer_opciones(argc, argv, &opt)) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    unsigned char texto[MAX_TEXTO];
    size_t largo_texto = 0;
    resultado_lectura lectura = leer_archivo(
        opt.archivo, texto, sizeof(texto), &largo_texto);

    if (lectura != LECTURA_OK) {
        fprintf(stderr, "Error con '%s': %s.\n",
                opt.archivo, descripcion_lectura(lectura));
        return EXIT_FAILURE;
    }

    texto_cifrado cifrado;
    unsigned char clave_aes[KEY_LEN];

    memset(&cifrado, 0, sizeof(cifrado));

    if (!cifrar_cbc(opt.secreto, texto, (int)largo_texto, &cifrado)) {
        fallar("no se pudo cifrar el texto.");
    }

    make_key(opt.secreto, clave_aes);

    printf("Ejecucion: secuencial (AES-128-CBC)\n");
    printf("Archivo: %s (%zu bytes)\n", opt.archivo, largo_texto);
    printf("Texto original: %.*s\n", (int)largo_texto, (const char *)texto);
    printf("Clave secreta: %" PRIu64 "\n", opt.secreto);
    imprimir_hex("Clave AES (hex): ", clave_aes, KEY_LEN);
    imprimir_hex("IV (hex): ", cifrado.iv, IV_LEN);
    imprimir_hex("Texto cifrado (hex): ", cifrado.datos, cifrado.longitud);
    printf("Frase de busqueda: \"%s\"\n", FRASE_BUSQUEDA);
    printf("Rango explorado: 2^%d = %" PRIu64 " candidatas\n", opt.bits, opt.total);

    if (opt.secreto >= opt.total) {
        printf("Aviso: la clave secreta esta fuera del rango explorado.\n");
    }

    EVP_CIPHER_CTX *ctx = crear_contexto_busqueda();
    unsigned char plano[MAX_CIFRADO];
    uint64_t encontrada = CLAVE_NO_ENCONTRADA;
    uint64_t probadas = 0;
    int limite_alcanzado = 0;

    if (ctx == NULL) {
        fallar("no se pudo crear el contexto de OpenSSL.");
    }

    double inicio = segundos_actuales();

    for (uint64_t candidata = 0; candidata < opt.total; candidata++) {
        int resultado = probar_clave(ctx, candidata, &cifrado, plano);

        probadas++;

        if (resultado < 0) {
            fallar("OpenSSL fallo al descifrar.");
        }

        if (resultado == 1) {
            encontrada = candidata;
            break;
        }

        if (opt.limite > 0.0 &&
            (candidata & MASCARA_RELOJ) == MASCARA_RELOJ &&
            segundos_actuales() - inicio >= opt.limite) {
            limite_alcanzado = 1;
            break;
        }
    }

    double transcurrido = segundos_actuales() - inicio;
    double rendimiento = transcurrido > 0.0 ? (double)probadas / transcurrido : 0.0;

    if (encontrada != CLAVE_NO_ENCONTRADA) {
        unsigned char recuperado[MAX_CIFRADO];
        int largo_recuperado = 0;

        if (!descifrar_cbc(encontrada, &cifrado, recuperado, &largo_recuperado)) {
            fallar("la clave encontrada no descifra el texto completo.");
        }

        printf("Resultado: ENCONTRADA\n");
        printf("Clave encontrada: %" PRIu64 "\n", encontrada);
        printf("Texto recuperado: %.*s\n", largo_recuperado, (const char *)recuperado);
        printf("Verificacion: %s\n",
               encontrada == opt.secreto
                   ? "coincide con la clave usada para cifrar"
                   : "NO coincide con la clave usada para cifrar");
    } else if (limite_alcanzado) {
        printf("Resultado: LIMITE (se detuvo a los %.0f s)\n", opt.limite);
    } else {
        printf("Resultado: NO ENCONTRADA en el rango explorado\n");
    }

    printf("Candidatas probadas: %" PRIu64 "\n", probadas);
    printf("Tiempo: %.6f segundos\n", transcurrido);
    printf("Rendimiento: %.0f claves/s\n", rendimiento);

    if (limite_alcanzado && rendimiento > 0.0 && opt.secreto < opt.total) {
        double intentos = (double)opt.secreto + 1.0;

        printf("Intentos necesarios: %.0f\n", intentos);
        printf("Tiempo estimado hasta la clave (s): %.6g\n", intentos / rendimiento);
        imprimir_duracion("Tiempo estimado hasta la clave: ", intentos / rendimiento);
    }

    OPENSSL_cleanse(clave_aes, sizeof(clave_aes));
    EVP_CIPHER_CTX_free(ctx);

    return EXIT_SUCCESS;
}
