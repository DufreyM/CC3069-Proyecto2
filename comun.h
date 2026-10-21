/* CC3069 Proyecto 02 - comun.h
 * Funciones compartidas: claves, lectura de archivos, AES-128-CBC y
 * busqueda de la frase que valida una clave candidata. */

#ifndef COMUN_H
#define COMUN_H

#include <stddef.h>
#include <stdint.h>
#include <openssl/evp.h>

#define KEY_LEN 16
#define IV_LEN 16
#define BLOCK_LEN 16
#define MAX_TEXTO 4096
#define MAX_CIFRADO (MAX_TEXTO + BLOCK_LEN)
#define MAX_BITS 56
#define FRASE_BUSQUEDA "es una prueba de"
#define ARCHIVO_POR_DEFECTO "datos/mensaje.txt"
#define CLAVE_NO_ENCONTRADA UINT64_MAX

typedef struct {
    unsigned char iv[IV_LEN];
    unsigned char datos[MAX_CIFRADO];
    int longitud;
} texto_cifrado;

typedef enum {
    LECTURA_OK = 0,
    LECTURA_NO_EXISTE,
    LECTURA_VACIO,
    LECTURA_MUY_GRANDE,
    LECTURA_ERROR
} resultado_lectura;

void make_key(uint64_t valor, unsigned char clave[KEY_LEN]);

int parse_u64(const char *texto, uint64_t *valor);

resultado_lectura leer_archivo(
    const char *ruta, unsigned char *buffer, size_t capacidad, size_t *leidos);

const char *descripcion_lectura(resultado_lectura resultado);

int cifrar_cbc(
    uint64_t clave, const unsigned char *texto, int longitud, texto_cifrado *salida);

int descifrar_cbc(
    uint64_t clave, const texto_cifrado *cifrado, unsigned char *texto, int *longitud);

EVP_CIPHER_CTX *crear_contexto_busqueda(void);

int probar_clave(
    EVP_CIPHER_CTX *ctx, uint64_t candidata,
    const texto_cifrado *cifrado, unsigned char *plano);

int contiene_frase(const unsigned char *texto, int longitud, const char *frase);

void imprimir_hex(const char *etiqueta, const unsigned char *datos, int longitud);

void imprimir_duracion(const char *etiqueta, double segundos);

#endif /* COMUN_H */
