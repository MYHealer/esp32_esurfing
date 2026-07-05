#include "cipher/CipherInterface.h"
#include "cipher/CipherUtils.h"

#include <string.h>
#include <stdio.h>

#include "utils/des_minimal.h"

typedef struct {
    uint8_t key1[24];
    uint8_t key2[24];
    uint8_t iv1[8];
    uint8_t iv2[8];
} desede_cbc_data_t;

static uint8_t* desede_encrypt_cbc(const uint8_t* data, size_t data_len,
                                   const uint8_t* key, const uint8_t* iv,
                                   size_t* out_len)
{
    size_t padded_len;
    uint8_t* padded_data = pad_2_multiple(data, data_len, 8, &padded_len);
    if (!padded_data) return NULL;

    uint8_t* output = s_malloc(padded_len);
    if (!output) { s_free(padded_data); return NULL; }

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    if (mbedtls_des3_set3key_enc(&ctx, key) != 0)
    {
        mbedtls_des3_free(&ctx);
        s_free(padded_data);
        s_free(output);
        return NULL;
    }

    uint8_t local_iv[8];
    memcpy(local_iv, iv, 8);
    if (mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_ENCRYPT,
                               padded_len, local_iv,
                               padded_data, output) != 0)
    {
        mbedtls_des3_free(&ctx);
        s_free(padded_data);
        s_free(output);
        return NULL;
    }

    mbedtls_des3_free(&ctx);
    s_free(padded_data);
    *out_len = padded_len;
    return output;
}

static uint8_t* desede_decrypt_cbc(const uint8_t* data, size_t data_len,
                                   const uint8_t* key, const uint8_t* iv,
                                   size_t* out_len)
{
    if (data_len == 0) return NULL;

    uint8_t* output = s_malloc(data_len);
    if (!output) return NULL;

    mbedtls_des3_context ctx;
    mbedtls_des3_init(&ctx);
    if (mbedtls_des3_set3key_dec(&ctx, key) != 0)
    {
        mbedtls_des3_free(&ctx);
        s_free(output);
        return NULL;
    }

    uint8_t local_iv[8];
    memcpy(local_iv, iv, 8);
    if (mbedtls_des3_crypt_cbc(&ctx, MBEDTLS_DES_DECRYPT,
                               data_len, local_iv,
                               data, output) != 0)
    {
        mbedtls_des3_free(&ctx);
        s_free(output);
        return NULL;
    }

    mbedtls_des3_free(&ctx);
    *out_len = data_len;
    return output;
}

static char* desede_cbc_encrypt(cipher_interface_t* self, const char* text)
{
    if (!self || !text) return NULL;
    desede_cbc_data_t* data = self->private_data;
    if (!data) return NULL;
    const size_t text_len = strlen(text);
    size_t r1_len;
    uint8_t* r1 = desede_encrypt_cbc((const uint8_t*)text, text_len,
                                     data->key1, data->iv1, &r1_len);
    if (!r1) return NULL;
    size_t r2_len;
    uint8_t* r2 = desede_encrypt_cbc(r1, r1_len, data->key2, data->iv2, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    char* hex_result = bytes_2_hex(r2, r2_len);
    s_free(r2);
    return hex_result;
}

static char* desede_cbc_decrypt(cipher_interface_t* self, const char* hex)
{
    if (!self || !hex) return NULL;
    desede_cbc_data_t* data = self->private_data;
    if (!data) return NULL;
    size_t bytes_len;
    uint8_t* bytes = hex_2_bytes(hex, &bytes_len);
    if (!bytes) return NULL;
    size_t r1_len;
    uint8_t* r1 = desede_decrypt_cbc(bytes, bytes_len, data->key2, data->iv2, &r1_len);
    s_free(bytes);
    if (!r1) return NULL;
    size_t r2_len;
    uint8_t* r2 = desede_decrypt_cbc(r1, r1_len, data->key1, data->iv1, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    while (r2_len > 0 && r2[r2_len - 1] == 0) r2_len--;
    char* result = s_malloc(r2_len + 1);
    memcpy(result, r2, r2_len);
    result[r2_len] = '\0';
    s_free(r2);
    return result;
}

static void desede_cbc_destroy(cipher_interface_t* self)
{
    if (self) { s_free(self->private_data); s_free(self); }
}

cipher_interface_t* create_desede_cbc_cipher(const uint8_t* key1, const uint8_t* key2,
                                             const uint8_t* iv1, const uint8_t* iv2)
{
    if (!key1 || !key2 || !iv1 || !iv2) return NULL;
    cipher_interface_t* cipher = s_malloc(sizeof(cipher_interface_t));
    desede_cbc_data_t* data = s_malloc(sizeof(desede_cbc_data_t));
    memcpy(data->key1, key1, 24);
    memcpy(data->key2, key2, 24);
    memcpy(data->iv1, iv1, 8);
    memcpy(data->iv2, iv2, 8);
    cipher->encrypt = desede_cbc_encrypt;
    cipher->decrypt = desede_cbc_decrypt;
    cipher->destroy = desede_cbc_destroy;
    cipher->private_data = data;
    return cipher;
}
