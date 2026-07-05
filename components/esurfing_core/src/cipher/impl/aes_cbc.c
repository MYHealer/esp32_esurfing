#include "cipher/CipherInterface.h"
#include "cipher/CipherUtils.h"

#include <string.h>
#include <stdio.h>

#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR >= 6
#include "mbedtls/private/aes.h"
#else
#include "mbedtls/aes.h"
#endif

typedef struct {
    uint8_t key1[16];
    uint8_t key2[16];
    uint8_t iv[16];
} aes_cbc_data_t;

static uint8_t* aes_encrypt_cbc(const uint8_t* data, size_t data_len,
                                const uint8_t* key, const uint8_t* iv,
                                size_t* out_len)
{
    size_t padded_len;
    uint8_t* padded_data = pad_2_multiple(data, data_len, 16, &padded_len);
    if (!padded_data) return NULL;

    uint8_t* output = s_malloc(16 + padded_len);
    if (!output) { s_free(padded_data); return NULL; }
    memcpy(output, iv, 16);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        s_free(padded_data);
        s_free(output);
        return NULL;
    }

    uint8_t local_iv[16];
    memcpy(local_iv, iv, 16);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT,
                              padded_len, local_iv,
                              padded_data, output + 16) != 0)
    {
        mbedtls_aes_free(&aes);
        s_free(padded_data);
        s_free(output);
        return NULL;
    }

    mbedtls_aes_free(&aes);
    s_free(padded_data);
    *out_len = 16 + padded_len;
    return output;
}

static uint8_t* aes_decrypt_cbc(const uint8_t* data, size_t data_len,
                                const uint8_t* key, const uint8_t* iv,
                                size_t* out_len)
{
    if (data_len < 16) return NULL;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        return NULL;
    }

    uint8_t* output = s_malloc(data_len);
    if (!output) { mbedtls_aes_free(&aes); return NULL; }

    uint8_t local_iv[16];
    memcpy(local_iv, iv, 16);
    if (mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT,
                              data_len, local_iv,
                              data, output) != 0)
    {
        mbedtls_aes_free(&aes);
        s_free(output);
        return NULL;
    }

    mbedtls_aes_free(&aes);
    *out_len = data_len;
    return output;
}

static char* aes_cbc_encrypt(cipher_interface_t* self, const char* text)
{
    if (!self || !text) return NULL;
    aes_cbc_data_t* data = self->private_data;
    if (!data) return NULL;
    const size_t text_len = strlen(text);
    size_t r1_len;
    uint8_t* r1 = aes_encrypt_cbc((const uint8_t*)text, text_len,
                                  data->key1, data->iv, &r1_len);
    if (!r1) return NULL;
    size_t r2_len;
    uint8_t* r2 = aes_encrypt_cbc(r1, r1_len, data->key2, data->iv, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    char* hex_result = bytes_2_hex(r2, r2_len);
    s_free(r2);
    return hex_result;
}

static char* aes_cbc_decrypt(cipher_interface_t* self, const char* hex)
{
    if (!self || !hex) return NULL;
    aes_cbc_data_t* data = self->private_data;
    if (!data) return NULL;
    size_t bytes_len;
    uint8_t* bytes = hex_2_bytes(hex, &bytes_len);
    if (!bytes || bytes_len < 32) { s_free(bytes); return NULL; }
    size_t r1_len;
    uint8_t* r1 = aes_decrypt_cbc(bytes + 16, bytes_len - 16,
                                  data->key2, data->iv, &r1_len);
    s_free(bytes);
    if (!r1 || r1_len < 16) { s_free(r1); return NULL; }
    size_t r2_len;
    uint8_t* r2 = aes_decrypt_cbc(r1 + 16, r1_len - 16,
                                  data->key1, data->iv, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    while (r2_len > 0 && r2[r2_len - 1] == 0) r2_len--;
    char* result = s_malloc(r2_len + 1);
    memcpy(result, r2, r2_len);
    result[r2_len] = '\0';
    s_free(r2);
    return result;
}

static void aes_cbc_destroy(cipher_interface_t* self)
{
    if (self) { s_free(self->private_data); s_free(self); }
}

cipher_interface_t* create_aes_cbc_cipher(const uint8_t* key1, const uint8_t* key2, const uint8_t* iv)
{
    if (!key1 || !key2 || !iv) return NULL;
    cipher_interface_t* cipher = s_malloc(sizeof(cipher_interface_t));
    aes_cbc_data_t* data = s_malloc(sizeof(aes_cbc_data_t));
    memcpy(data->key1, key1, 16);
    memcpy(data->key2, key2, 16);
    memcpy(data->iv, iv, 16);
    cipher->encrypt = aes_cbc_encrypt;
    cipher->decrypt = aes_cbc_decrypt;
    cipher->destroy = aes_cbc_destroy;
    cipher->private_data = data;
    return cipher;
}
