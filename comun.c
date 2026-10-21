/* CC3069 Proyecto 02 - comun.c
 * Implementacion de las funciones compartidas (comun.h). */

#define _POSIX_C_SOURCE 200809L

#include "comun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

#define SEGUNDOS_POR_ANIO (365.25 * 24.0 * 3600.0)

void make_key(uint64_t valor, unsigned char clave[KEY_LEN])
{
    memset(clave, 0, KEY_LEN);

    for (int i = 0; i < 8; i++) {
        clave[KEY_LEN - 1 - i] = (unsigned char)((valor >> (8 * i)) & UINT64_C(0xFF));
    }
}

int parse_u64(const char *texto, uint64_t *valor)
{
    char *fin = NULL;

    if (texto == NULL || texto[0] == '-' || texto[0] == '\0') {
        return 0;
    }

    errno = 0;
    unsigned long long leido = strtoull(texto, &fin, 10);

    if (errno != 0 || fin == texto || *fin != '\0') {
        return 0;
    }

    *valor = (uint64_t)leido;
    return 1;
}

resultado_lectura leer_archivo(
    const char *ruta, unsigned char *buffer, size_t capacidad, size_t *leidos)
{
    FILE *archivo = fopen(ruta, "rb");

    *leidos = 0;

    if (archivo == NULL) {
        return errno == ENOENT ? LECTURA_NO_EXISTE : LECTURA_ERROR;
    }

    size_t total = fread(buffer, 1, capacidad, archivo);
    int sobra = fgetc(archivo) != EOF;   /* un byte de mas => excede el limite */
    int fallo = ferror(archivo);

    fclose(archivo);

    if (fallo) {
        return LECTURA_ERROR;
    }

    if (sobra) {
        return LECTURA_MUY_GRANDE;
    }

    if (total > 0 && buffer[total - 1] == '\n') {   /* salto final del editor */
        total--;
        if (total > 0 && buffer[total - 1] == '\r') {
            total--;
        }
    }

    if (total == 0) {
        return LECTURA_VACIO;
    }

    *leidos = total;
    return LECTURA_OK;
}

const char *descripcion_lectura(resultado_lectura resultado)
{
    switch (resultado) {
    case LECTURA_OK:
        return "lectura correcta";
    case LECTURA_NO_EXISTE:
        return "el archivo no existe";
    case LECTURA_VACIO:
        return "el archivo esta vacio";
    case LECTURA_MUY_GRANDE:
        return "el archivo excede el tamano maximo permitido";
    default:
        return "no se pudo leer el archivo";
    }
}

int cifrar_cbc(
    uint64_t clave, const unsigned char *texto, int longitud, texto_cifrado *salida)
{
    unsigned char key[KEY_LEN];
    int escritos = 0;
    int escritos_final = 0;
    int ok = 0;
    EVP_CIPHER_CTX *ctx = NULL;

    if (longitud <= 0 || longitud > MAX_TEXTO) {
        return 0;
    }

    make_key(clave, key);

    if (RAND_bytes(salida->iv, IV_LEN) != 1) {
        goto fin;
    }

    ctx = EVP_CIPHER_CTX_new();

    if (ctx == NULL ||
        EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, salida->iv) != 1 ||
        EVP_EncryptUpdate(ctx, salida->datos, &escritos, texto, longitud) != 1 ||
        EVP_EncryptFinal_ex(ctx, salida->datos + escritos, &escritos_final) != 1) {
        goto fin;
    }

    salida->longitud = escritos + escritos_final;
    ok = 1;

fin:
    OPENSSL_cleanse(key, sizeof(key));
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

int descifrar_cbc(
    uint64_t clave, const texto_cifrado *cifrado, unsigned char *texto, int *longitud)
{
    unsigned char key[KEY_LEN];
    int escritos = 0;
    int escritos_final = 0;
    int ok = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    make_key(clave, key);

    if (ctx == NULL ||
        EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, cifrado->iv) != 1 ||
        EVP_DecryptUpdate(ctx, texto, &escritos, cifrado->datos, cifrado->longitud) != 1 ||
        EVP_DecryptFinal_ex(ctx, texto + escritos, &escritos_final) != 1) {
        goto fin;
    }

    *longitud = escritos + escritos_final;
    ok = 1;

fin:
    OPENSSL_cleanse(key, sizeof(key));
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

EVP_CIPHER_CTX *crear_contexto_busqueda(void)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (ctx == NULL) {
        return NULL;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return NULL;
    }

    return ctx;
}

int probar_clave(
    EVP_CIPHER_CTX *ctx, uint64_t candidata,
    const texto_cifrado *cifrado, unsigned char *plano)
{
    unsigned char key[KEY_LEN];
    int escritos = 0;

    make_key(candidata, key);

    /* Sin padding: la validacion la hace la frase, no PKCS#7. */
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, cifrado->iv) != 1 ||
        EVP_CIPHER_CTX_set_padding(ctx, 0) != 1 ||
        EVP_DecryptUpdate(ctx, plano, &escritos, cifrado->datos, cifrado->longitud) != 1) {
        return -1;
    }

    return contiene_frase(plano, escritos, FRASE_BUSQUEDA);
}

int contiene_frase(const unsigned char *texto, int longitud, const char *frase)
{
    int largo_frase = (int)strlen(frase);

    for (int i = 0; i + largo_frase <= longitud; i++) {
        if (memcmp(texto + i, frase, (size_t)largo_frase) == 0) {
            return 1;
        }
    }

    return 0;
}

void imprimir_hex(const char *etiqueta, const unsigned char *datos, int longitud)
{
    printf("%s", etiqueta);

    for (int i = 0; i < longitud; i++) {
        printf("%02x", datos[i]);
    }

    putchar('\n');
}

void imprimir_duracion(const char *etiqueta, double segundos)
{
    if (segundos < 60.0) {
        printf("%s%.3f segundos\n", etiqueta, segundos);
    } else if (segundos < 3600.0) {
        printf("%s%.2f minutos\n", etiqueta, segundos / 60.0);
    } else if (segundos < 86400.0) {
        printf("%s%.2f horas\n", etiqueta, segundos / 3600.0);
    } else if (segundos < SEGUNDOS_POR_ANIO) {
        printf("%s%.2f días\n", etiqueta, segundos / 86400.0);
    } else {
        printf("%s%.2f años\n", etiqueta, segundos / SEGUNDOS_POR_ANIO);
    }
}
