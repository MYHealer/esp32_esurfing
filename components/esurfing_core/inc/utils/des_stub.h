/**
 * @brief Minimal DES/3DES implementation for ESP32 ESurfingClient
 *
 * mbedTLS 4.x (IDF v6) removed DES support.
 * This is a standalone implementation sufficient for the ESurfing auth protocol.
 * Based on public domain DES reference implementation.
 */

#ifndef DES_STUB_H
#define DES_STUB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 3DES (Triple DES) context */
typedef struct {
    uint32_t ek[3][32];  /* encryption subkeys */
    uint32_t dk[3][32];  /* decryption subkeys */
} des3_context;

/* Single DES context */
typedef struct {
    uint32_t ek[32];     /* encryption subkeys */
    uint32_t dk[32];     /* decryption subkeys */
} des_context;

void mbedtls_des3_init(des3_context *ctx);
void mbedtls_des3_free(des3_context *ctx);
int  mbedtls_des3_set3key_enc(des3_context *ctx, const unsigned char key[24]);
int  mbedtls_des3_set3key_dec(des3_context *ctx, const unsigned char key[24]);
int  mbedtls_des3_crypt_ecb(des3_context *ctx, const unsigned char input[8], unsigned char output[8]);
int  mbedtls_des3_crypt_cbc(des3_context *ctx, int mode, size_t length,
                            unsigned char iv[8], const unsigned char *input, unsigned char *output);

void mbedtls_des_init(des_context *ctx);
void mbedtls_des_free(des_context *ctx);
int  mbedtls_des_setkey_enc(des_context *ctx, const unsigned char key[8]);
int  mbedtls_des_setkey_dec(des_context *ctx, const unsigned char key[8]);
int  mbedtls_des_crypt_ecb(des_context *ctx, const unsigned char input[8], unsigned char output[8]);
int  mbedtls_des_crypt_cbc(des_context *ctx, int mode, size_t length,
                           unsigned char iv[8], const unsigned char *input, unsigned char *output);

#define MBEDTLS_DES_ENCRYPT 1
#define MBEDTLS_DES_DECRYPT 0

#ifdef __cplusplus
}
#endif

#endif /* DES_STUB_H */
