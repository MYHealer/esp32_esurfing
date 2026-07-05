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
} aes_ecb_data_t;

static uint8_t* aes_encrypt_ecb(const uint8_t* data, const size_t data_len,
                                const uint8_t* key, size_t* out_len)
{
    size_t padded_len;
    uint8_t* padded_data = pad_2_multiple(data, data_len, 16, &padded_len);
    if (!padded_data) return NULL;

    uint8_t* output = s_malloc(padded_len);
    if (!output) { s_free(padded_data); return NULL; }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        s_free(padded_data);
        s_free(output);
        return NULL;
    }

    for (size_t i = 0; i < padded_len; i += 16)
    {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                                  padded_data + i, output + i) != 0)
        {
            mbedtls_aes_free(&aes);
            s_free(padded_data);
            s_free(output);
            return NULL;
        }
    }

    mbedtls_aes_free(&aes);
    s_free(padded_data);
    *out_len = padded_len;
    return output;
}

static uint8_t* aes_decrypt_ecb(const uint8_t* data, const size_t data_len,
                                const uint8_t* key, size_t* out_len)
{
    if (data_len % 16 != 0) return NULL;

    uint8_t* output = s_malloc(data_len);
    if (!output) return NULL;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_dec(&aes, key, 128) != 0)
    {
        mbedtls_aes_free(&aes);
        s_free(output);
        return NULL;
    }

    for (size_t i = 0; i < data_len; i += 16)
    {
        if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT,
                                  data + i, output + i) != 0)
        {
            mbedtls_aes_free(&aes);
            s_free(output);
            return NULL;
        }
    }

    mbedtls_aes_free(&aes);
    *out_len = data_len;
    return output;
}

static char* aes_ecb_encrypt(cipher_interface_t* self, const char* text)
{
    if (!self || !text) return NULL;
    aes_ecb_data_t* data = self->private_data;
    if (!data) return NULL;
    const size_t text_len = strlen(text);
    size_t r1_len;
    uint8_t* r1 = aes_encrypt_ecb((const uint8_t*)text, text_len,
                                  data->key1, &r1_len);
    if (!r1) return NULL;
    size_t r2_len;
    uint8_t* r2 = aes_encrypt_ecb(r1, r1_len, data->key2, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    char* hex_result = bytes_2_hex(r2, r2_len);
    s_free(r2);
    return hex_result;
}

static char* aes_ecb_decrypt(cipher_interface_t* self, const char* hex)
{
    if (!self || !hex) return NULL;
    aes_ecb_data_t* data = self->private_data;
    if (!data) return NULL;
    size_t bytes_len;
    uint8_t* bytes = hex_2_bytes(hex, &bytes_len);
    if (!bytes) return NULL;
    size_t r1_len;
    uint8_t* r1 = aes_decrypt_ecb(bytes, bytes_len, data->key2, &r1_len);
    s_free(bytes);
    if (!r1) return NULL;
    size_t r2_len;
    uint8_t* r2 = aes_decrypt_ecb(r1, r1_len, data->key1, &r2_len);
    s_free(r1);
    if (!r2) return NULL;
    while (r2_len > 0 && r2[r2_len - 1] == 0) r2_len--;
    char* result = s_malloc(r2_len + 1);
    memcpy(result, r2, r2_len);
    result[r2_len] = '\0';
    s_free(r2);
    return result;
}

static void aes_ecb_destroy(cipher_interface_t* self)
{
    if (self) { s_free(self->private_data); s_free(self); }
}

cipher_interface_t* create_aes_ecb_cipher(const uint8_t* key1, const uint8_t* key2)
{
    if (!key1 || !key2) return NULL;
    cipher_interface_t* cipher = s_malloc(sizeof(cipher_interface_t));
    aes_ecb_data_t* data = s_malloc(sizeof(aes_ecb_data_t));
    memcpy(data->key1, key1, 16);
    memcpy(data->key2, key2, 16);
    cipher->encrypt = aes_ecb_encrypt;
    cipher->decrypt = aes_ecb_decrypt;
    cipher->destroy = aes_ecb_destroy;
    cipher->private_data = data;
    return cipher;
}
