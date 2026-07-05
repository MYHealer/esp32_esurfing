/**
 * Minimal DES/3DES implementation for ESP-IDF v6 (mbedTLS 4.x removed DES).
 * API-compatible with mbedTLS DES functions.
 * Standard DES (NIST FIPS 46-3), minimal and correct.
 */

#ifndef DES_MINIMAL_H
#define DES_MINIMAL_H

#include <stdint.h>
#include <string.h>

#define MBEDTLS_DES_ENCRYPT 1
#define MBEDTLS_DES_DECRYPT 0

typedef struct { uint32_t ek[32]; uint32_t dk[32]; } mbedtls_des_context;
typedef struct { uint32_t ek[3][32]; uint32_t dk[3][32]; } mbedtls_des3_context;

/* ---- Standard DES tables ---- */

static const uint8_t SBOX[8][64] = {
    /* S1 */ {14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13},
    /* S2 */ {15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9},
    /* S3 */ {10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12},
    /* S4 */ {7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14},
    /* S5 */ {2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3},
    /* S6 */ {12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13},
    /* S7 */ {4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12},
    /* S8 */ {13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,1,15,13,8,10,3,7,4,12,5,6,2,0,14,9,11,7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,2,1,7,8,13,15,9,0,14,4,6,12,10,5,3,11}
};

static const uint8_t IP_TAB[64] = {
    58,50,42,34,26,18,10,2,60,52,44,36,28,20,12,4,
    62,54,46,38,30,22,14,6,64,56,48,40,32,24,16,8,
    57,49,41,33,25,17,9,1,59,51,43,35,27,19,11,3,
    61,53,45,37,29,21,13,5,63,55,47,39,31,23,15,7
};

static const uint8_t FP_TAB[64] = {
    40,8,48,16,56,24,64,32,39,7,47,15,55,23,63,31,
    38,6,46,14,54,22,62,30,37,5,45,13,53,21,61,29,
    36,4,44,12,52,20,60,28,35,3,43,11,51,19,59,27,
    34,2,42,10,50,18,58,26,33,1,41,9,49,17,57,25
};

static const uint8_t E_TAB[48] = {
    32,1,2,3,4,5,4,5,6,7,8,9,8,9,10,11,
    12,13,12,13,14,15,16,17,16,17,18,19,20,21,20,21,
    22,23,24,25,24,25,26,27,28,29,28,29,30,31,32,1
};

static const uint8_t P_TAB[32] = {
    16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
    2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25
};

static const uint8_t PC1_TAB[56] = {
    57,49,41,33,25,17,9,1,58,50,42,34,26,18,10,2,
    59,51,43,35,27,19,11,3,60,52,44,36,63,55,47,39,
    31,23,15,7,62,54,46,38,30,22,14,6,61,53,45,37,
    29,21,13,5,28,20,12,4
};

static const uint8_t PC2_TAB[48] = {
    14,17,11,24,1,5,3,28,15,6,21,10,23,19,12,4,
    26,8,16,7,27,20,13,2,41,52,31,37,47,55,30,40,
    51,45,33,48,44,49,39,56,34,53,46,42,50,36,29,32
};

static const uint8_t SHIFTS[16] = {1,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1};

/* ---- Helper: permute bits ---- */
static inline uint64_t permute(uint64_t in, const uint8_t *tab, int inbits, int outbits) {
    uint64_t out = 0;
    for (int i = 0; i < outbits; i++) {
        out = (out << 1) | ((in >> (inbits - tab[i])) & 1);
    }
    return out;
}

static inline uint32_t rotl28(uint32_t v, int n) {
    return ((v << n) | (v >> (28 - n))) & 0x0FFFFFFF;
}

/* ---- Key schedule ---- */
static inline void des_keyschedule(uint32_t subkeys[32], const uint8_t key[8], int decrypt) {
    uint64_t k = 0;
    for (int i = 0; i < 8; i++) k = (k << 8) | key[i];

    uint64_t C = permute(k, PC1_TAB, 64, 28) << 28;
    uint64_t D = permute(k, PC1_TAB + 28, 64, 28);
    /* C and D are packed in a 56-bit value: high 28 = C, low 28 = D */
    uint64_t CD = permute(k, PC1_TAB, 64, 56);

    uint32_t C28 = (uint32_t)(CD >> 28);
    uint32_t D28 = (uint32_t)(CD & 0x0FFFFFFF);

    for (int round = 0; round < 16; round++) {
        int r = decrypt ? 15 - round : round;
        C28 = rotl28(C28, SHIFTS[round]);
        D28 = rotl28(D28, SHIFTS[round]);

        uint64_t combined = ((uint64_t)C28 << 28) | D28;
        uint64_t K = permute(combined, PC2_TAB, 56, 48);

        subkeys[r * 2]     = (uint32_t)(K >> 24);
        subkeys[r * 2 + 1] = (uint32_t)(K & 0x00FFFFFF);
    }
}

/* ---- Single DES round (Feistel) ---- */
static inline uint32_t des_f(uint32_t R, const uint32_t sk[2]) {
    /* Expand R to 48 bits and XOR with subkey */
    uint64_t x = 0;
    for (int i = 0; i < 48; i++) {
        x = (x << 1) | ((R >> (32 - E_TAB[i])) & 1);
    }
    /* sk[0] holds bits 47-24, sk[1] holds bits 23-0 */
    uint64_t kval = ((uint64_t)sk[0] << 24) | sk[1];
    x ^= kval;

    /* S-box substitution: 8 groups of 6 bits -> 8 groups of 4 bits */
    uint32_t out = 0;
    for (int i = 0; i < 8; i++) {
        uint8_t six = (uint8_t)((x >> (42 - i * 6)) & 0x3F);
        uint8_t row = (uint8_t)(((six >> 5) & 1) * 2 + (six & 1));
        uint8_t col = (uint8_t)((six >> 1) & 0xF);
        out = (out << 4) | SBOX[i][row * 16 + col];
    }

    /* P permutation */
    uint32_t p = 0;
    for (int i = 0; i < 32; i++) {
        p = (p << 1) | ((out >> (32 - P_TAB[i])) & 1);
    }
    return p;
}

/* ---- Process one 8-byte block ---- */
static inline void des_block(const uint32_t subkeys[32], const uint8_t in[8], uint8_t out[8]) {
    /* IP */
    uint64_t block = 0;
    for (int i = 0; i < 8; i++) block = (block << 8) | in[i];
    uint64_t ip = permute(block, IP_TAB, 64, 64);

    uint32_t L = (uint32_t)(ip >> 32);
    uint32_t R = (uint32_t)(ip & 0xFFFFFFFF);

    /* 16 Feistel rounds */
    for (int i = 0; i < 16; i++) {
        uint32_t f = des_f(R, &subkeys[i * 2]);
        uint32_t newR = L ^ f;
        L = R;
        R = newR;
    }

    /* Swap L/R, then FP */
    uint64_t rl = ((uint64_t)R << 32) | L;
    uint64_t fp = permute(rl, FP_TAB, 64, 64);

    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t)(fp >> (56 - i * 8));
    }
}

/* ==== Public API (mbedTLS compatible) ==== */

static inline void mbedtls_des_init(mbedtls_des_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }
static inline void mbedtls_des_free(mbedtls_des_context *ctx) { (void)ctx; }
static inline void mbedtls_des3_init(mbedtls_des3_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }
static inline void mbedtls_des3_free(mbedtls_des3_context *ctx) { (void)ctx; }

static inline int mbedtls_des_setkey_enc(mbedtls_des_context *ctx, const unsigned char key[8]) {
    des_keyschedule(ctx->ek, key, 0);
    return 0;
}

static inline int mbedtls_des_setkey_dec(mbedtls_des_context *ctx, const unsigned char key[8]) {
    des_keyschedule(ctx->dk, key, 1);
    return 0;
}

static inline int mbedtls_des_crypt_ecb(mbedtls_des_context *ctx, const unsigned char input[8], unsigned char output[8]) {
    des_block(ctx->ek, input, output);
    return 0;
}

static inline int mbedtls_des_crypt_cbc(mbedtls_des_context *ctx, int mode, size_t length,
                          unsigned char iv[8], const unsigned char *input, unsigned char *output) {
    if (length % 8) return -1;
    if (mode == MBEDTLS_DES_ENCRYPT) {
        for (size_t i = 0; i < length; i += 8) {
            for (int j = 0; j < 8; j++) output[i + j] = input[i + j] ^ iv[j];
            des_block(ctx->ek, output + i, output + i);
            memcpy(iv, output + i, 8);
        }
    } else {
        for (size_t i = 0; i < length; i += 8) {
            unsigned char temp[8];
            des_block(ctx->dk, input + i, temp);
            for (int j = 0; j < 8; j++) output[i + j] = temp[j] ^ iv[j];
            memcpy(iv, input + i, 8);
        }
    }
    return 0;
}

static inline int mbedtls_des3_set3key_enc(mbedtls_des3_context *ctx, const unsigned char key[24]) {
    des_keyschedule(ctx->ek[0], key, 0);
    des_keyschedule(ctx->dk[1], key + 8, 1);  /* key2 decrypt keys for middle round */
    des_keyschedule(ctx->ek[2], key + 16, 0);
    return 0;
}

static inline int mbedtls_des3_set3key_dec(mbedtls_des3_context *ctx, const unsigned char key[24]) {
    des_keyschedule(ctx->dk[0], key + 16, 1);
    des_keyschedule(ctx->ek[1], key + 8, 0);  /* key2 encrypt keys for middle round */
    des_keyschedule(ctx->dk[2], key, 1);
    return 0;
}

static inline int mbedtls_des3_crypt_ecb(mbedtls_des3_context *ctx, const unsigned char input[8], unsigned char output[8]) {
    unsigned char temp[8];
    des_block(ctx->ek[0], input, output);
    des_block(ctx->dk[1], output, temp);
    des_block(ctx->ek[2], temp, output);
    return 0;
}

static inline int mbedtls_des3_crypt_cbc(mbedtls_des3_context *ctx, int mode, size_t length,
                           unsigned char iv[8], const unsigned char *input, unsigned char *output) {
    if (length % 8) return -1;
    if (mode == MBEDTLS_DES_ENCRYPT) {
        for (size_t i = 0; i < length; i += 8) {
            unsigned char block[8], temp[8];
            for (int j = 0; j < 8; j++) block[j] = input[i + j] ^ iv[j];
            des_block(ctx->ek[0], block, temp);
            des_block(ctx->dk[1], temp, block);
            des_block(ctx->ek[2], block, output + i);
            memcpy(iv, output + i, 8);
        }
    } else {
        for (size_t i = 0; i < length; i += 8) {
            unsigned char a[8], b[8];
            des_block(ctx->dk[2], input + i, a);
            des_block(ctx->ek[1], a, b);
            des_block(ctx->dk[0], b, output + i);
            for (int j = 0; j < 8; j++) output[i + j] ^= iv[j];
            memcpy(iv, input + i, 8);
        }
    }
    return 0;
}

#endif /* DES_MINIMAL_H */
