/* CC3069 Proyecto 02 - aes_cifrado.c
 * Cifrado y descifrado AES-128-CBC de un archivo de texto con una clave
 * de entrada (inciso 1). El archivo cifrado guarda el IV (16 bytes)
 * seguido del texto cifrado.
 * Uso: ./aes_cifrado [-c | -d] [-k clave] [-f entrada] [-o salida]
 *   sin -c ni -d: cifra, guarda, vuelve a leer, descifra y compara. */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "comun.h"

#define CLAVE_POR_DEFECTO UINT64_C(123456)
#define CIFRADO_POR_DEFECTO "datos/mensaje.enc"
#define RECUPERADO_POR_DEFECTO "recuperado.txt"

typedef enum {
    MODO_COMPLETO = 0,
    MODO_CIFRAR,
    MODO_DESCIFRAR
} modo_ejecucion;

typedef struct {
    modo_ejecucion modo;
    uint64_t clave;
    const char *entrada;
    const char *salida;
} opciones;

static void fallar(const char *descripcion)
{
    fprintf(stderr, "Error: %s\n", descripcion);
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

static void uso(const char *programa)
{
    fprintf(stderr,
        "Uso: %s [-c | -d] [-k clave] [-f entrada] [-o salida]\n"
        "  -c           solo cifra: entrada .txt -> salida cifrada (IV + datos)\n"
        "  -d           solo descifra: entrada cifrada -> salida .txt\n"
        "               sin -c ni -d cifra, guarda, descifra y compara\n"
        "  -k clave     clave en decimal (por defecto %" PRIu64 ")\n"
        "  -f entrada   archivo de entrada (por defecto %s o %s con -d)\n"
        "  -o salida    archivo de salida (por defecto %s o %s con -d)\n",
        programa, CLAVE_POR_DEFECTO,
        ARCHIVO_POR_DEFECTO, CIFRADO_POR_DEFECTO,
        CIFRADO_POR_DEFECTO, RECUPERADO_POR_DEFECTO);
}

static int leer_opciones(int argc, char *argv[], opciones *opt)
{
    int opcion;

    opt->modo = MODO_COMPLETO;
    opt->clave = CLAVE_POR_DEFECTO;
    opt->entrada = NULL;
    opt->salida = NULL;

    while ((opcion = getopt(argc, argv, "cdk:f:o:")) != -1) {
        switch (opcion) {
        case 'c':
        case 'd':
            if (opt->modo != MODO_COMPLETO) {
                return 0;   /* -c y -d son excluyentes */
            }
            opt->modo = opcion == 'c' ? MODO_CIFRAR : MODO_DESCIFRAR;
            break;
        case 'k':
            if (!parse_u64(optarg, &opt->clave)) {
                return 0;
            }
            break;
        case 'f':
            opt->entrada = optarg;
            break;
        case 'o':
            opt->salida = optarg;
            break;
        default:
            return 0;
        }
    }

    if (optind != argc) {
        return 0;
    }

    if (opt->entrada == NULL) {
        opt->entrada = opt->modo == MODO_DESCIFRAR
            ? CIFRADO_POR_DEFECTO : ARCHIVO_POR_DEFECTO;
    }

    if (opt->salida == NULL) {
        opt->salida = opt->modo == MODO_DESCIFRAR
            ? RECUPERADO_POR_DEFECTO : CIFRADO_POR_DEFECTO;
    }

    return 1;
}

static int escribir_archivo(
    const char *ruta, const unsigned char *datos, size_t longitud)
{
    FILE *archivo = fopen(ruta, "wb");

    if (archivo == NULL) {
        return 0;
    }

    size_t escritos = fwrite(datos, 1, longitud, archivo);
    int fallo = fclose(archivo) != 0;

    return escritos == longitud && !fallo;
}

static int guardar_cifrado(const char *ruta, const texto_cifrado *cifrado)
{
    unsigned char buffer[IV_LEN + MAX_CIFRADO];

    memcpy(buffer, cifrado->iv, IV_LEN);
    memcpy(buffer + IV_LEN, cifrado->datos, (size_t)cifrado->longitud);

    return escribir_archivo(ruta, buffer, IV_LEN + (size_t)cifrado->longitud);
}

/* No usa leer_archivo(): esa funcion quita un '\n' final, lo que en datos
 * binarios cortaria el ultimo bloque cuando su ultimo byte vale 0x0A. */
static int cargar_cifrado(const char *ruta, texto_cifrado *cifrado)
{
    unsigned char buffer[IV_LEN + MAX_CIFRADO];
    FILE *archivo = fopen(ruta, "rb");

    if (archivo == NULL) {
        fprintf(stderr, "Error con '%s': %s.\n", ruta,
                errno == ENOENT ? "el archivo no existe" : "no se pudo abrir");
        return 0;
    }

    size_t total = fread(buffer, 1, sizeof(buffer), archivo);
    int sobra = fgetc(archivo) != EOF;
    int fallo = ferror(archivo);

    fclose(archivo);

    if (fallo) {
        fprintf(stderr, "Error con '%s': no se pudo leer el archivo.\n", ruta);
        return 0;
    }

    if (sobra || total <= IV_LEN || (total - IV_LEN) % BLOCK_LEN != 0) {
        fprintf(stderr,
                "Error con '%s': no tiene el formato IV + bloques de %d bytes.\n",
                ruta, BLOCK_LEN);
        return 0;
    }

    memcpy(cifrado->iv, buffer, IV_LEN);
    memcpy(cifrado->datos, buffer + IV_LEN, total - IV_LEN);
    cifrado->longitud = (int)(total - IV_LEN);
    return 1;
}

static void imprimir_encabezado(const char *modo, uint64_t clave)
{
    unsigned char clave_aes[KEY_LEN];

    make_key(clave, clave_aes);

    printf("Ejecucion: %s (AES-128-CBC)\n", modo);
    printf("Clave: %" PRIu64 "\n", clave);
    imprimir_hex("Clave AES (hex): ", clave_aes, KEY_LEN);

    OPENSSL_cleanse(clave_aes, sizeof(clave_aes));
}

static void imprimir_cifrado(const texto_cifrado *cifrado)
{
    imprimir_hex("IV (hex): ", cifrado->iv, IV_LEN);
    printf("Texto cifrado: %d bytes\n", cifrado->longitud);
    imprimir_hex("Texto cifrado (hex): ", cifrado->datos, cifrado->longitud);
}

static int cifrar_archivo(const opciones *opt, texto_cifrado *cifrado,
                          unsigned char *texto, size_t *largo_texto)
{
    resultado_lectura lectura = leer_archivo(
        opt->entrada, texto, MAX_TEXTO, largo_texto);

    if (lectura != LECTURA_OK) {
        fprintf(stderr, "Error con '%s': %s.\n",
                opt->entrada, descripcion_lectura(lectura));
        return 0;
    }

    printf("Archivo original: %s (%zu bytes)\n", opt->entrada, *largo_texto);
    printf("Texto original: %.*s\n", (int)*largo_texto, (const char *)texto);

    if (!cifrar_cbc(opt->clave, texto, (int)*largo_texto, cifrado)) {
        fallar("no se pudo cifrar el texto.");
    }

    imprimir_cifrado(cifrado);

    if (!guardar_cifrado(opt->salida, cifrado)) {
        fprintf(stderr, "Error: no se pudo escribir '%s'.\n", opt->salida);
        return 0;
    }

    printf("Archivo cifrado: %s (%d bytes = IV + texto cifrado)\n",
           opt->salida, IV_LEN + cifrado->longitud);
    return 1;
}

static int descifrar_archivo(const char *entrada, const char *salida,
                             uint64_t clave, unsigned char *recuperado,
                             int *largo_recuperado)
{
    texto_cifrado cifrado;

    memset(&cifrado, 0, sizeof(cifrado));

    if (!cargar_cifrado(entrada, &cifrado)) {
        return 0;
    }

    printf("Archivo cifrado leido: %s\n", entrada);
    imprimir_cifrado(&cifrado);

    if (!descifrar_cbc(clave, &cifrado, recuperado, largo_recuperado)) {
        fprintf(stderr,
                "Error: no se pudo descifrar (clave incorrecta o archivo danado).\n");
        return 0;
    }

    printf("Texto recuperado: %.*s\n", *largo_recuperado, (const char *)recuperado);

    if (!escribir_archivo(salida, recuperado, (size_t)*largo_recuperado)) {
        fprintf(stderr, "Error: no se pudo escribir '%s'.\n", salida);
        return 0;
    }

    printf("Archivo recuperado: %s (%d bytes)\n", salida, *largo_recuperado);
    return 1;
}

int main(int argc, char *argv[])
{
    opciones opt;
    texto_cifrado cifrado;
    unsigned char texto[MAX_TEXTO];
    unsigned char recuperado[MAX_CIFRADO];
    size_t largo_texto = 0;
    int largo_recuperado = 0;
    int ok = 0;

    opterr = 0;

    if (!leer_opciones(argc, argv, &opt)) {
        uso(argv[0]);
        return EXIT_FAILURE;
    }

    memset(&cifrado, 0, sizeof(cifrado));

    switch (opt.modo) {
    case MODO_CIFRAR:
        imprimir_encabezado("cifrado", opt.clave);
        ok = cifrar_archivo(&opt, &cifrado, texto, &largo_texto);
        break;

    case MODO_DESCIFRAR:
        imprimir_encabezado("descifrado", opt.clave);
        ok = descifrar_archivo(opt.entrada, opt.salida, opt.clave,
                               recuperado, &largo_recuperado);
        break;

    default:
        imprimir_encabezado("cifrado y descifrado", opt.clave);
        ok = cifrar_archivo(&opt, &cifrado, texto, &largo_texto);

        if (ok) {
            putchar('\n');
            ok = descifrar_archivo(opt.salida, RECUPERADO_POR_DEFECTO, opt.clave,
                                   recuperado, &largo_recuperado);
        }

        if (ok) {
            int coincide = (size_t)largo_recuperado == largo_texto &&
                           memcmp(texto, recuperado, largo_texto) == 0;

            printf("\nVerificacion: el texto recuperado %s al original\n",
                   coincide ? "es identico" : "NO es identico");
            ok = coincide;
        }
        break;
    }

    OPENSSL_cleanse(texto, sizeof(texto));
    OPENSSL_cleanse(recuperado, sizeof(recuperado));

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
