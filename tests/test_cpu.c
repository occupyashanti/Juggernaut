// tests/test_cpu.c
#include "../core/hash_algorithms.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define TEST_ROUNDS 10000

/**
 * @brief Helper to print a hash as hex.
 */
static void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) printf("%02x", data[i]);
    printf("\n");
}

int main(void) {
    const char *password = "password123";
    const uint8_t salt[16] = {0};
    uint8_t input[32];
    memcpy(input, salt, 16);
    memcpy(input + 16, password, strlen(password));

    // --- MD5 ---
    hash_ctx_t md5ctx;
    uint8_t md5_out[16];
    clock_t md5_start = clock();
    for (int i = 0; i < TEST_ROUNDS; ++i) {
        hash_init(&md5ctx, HASH_MD5);
        hash_update(&md5ctx, (const uint8_t *)password, strlen(password));
        hash_final(&md5ctx, md5_out);
        hash_free(&md5ctx);
    }
    clock_t md5_end = clock();
    printf("MD5:   "); print_hex(md5_out, 16);
    printf("MD5 time:   %.2f ms\n", 1000.0 * (md5_end - md5_start) / CLOCKS_PER_SEC);

    // --- Bcrypt ---
    hash_ctx_t bcryptctx;
    uint8_t bcrypt_out[32];
    clock_t bcrypt_start = clock();
    for (int i = 0; i < TEST_ROUNDS; ++i) {
        hash_init(&bcryptctx, HASH_BCRYPT);
        hash_update(&bcryptctx, input, 16 + strlen(password));
        hash_final(&bcryptctx, bcrypt_out);
        hash_free(&bcryptctx);
    }
    clock_t bcrypt_end = clock();
    printf("Bcrypt: "); print_hex(bcrypt_out, 32);
    printf("Bcrypt time: %.2f ms\n", 1000.0 * (bcrypt_end - bcrypt_start) / CLOCKS_PER_SEC);

    // --- Correctness check (dummy) ---
    hash_init(&md5ctx, HASH_MD5);
    hash_update(&md5ctx, (const uint8_t *)password, strlen(password));
    hash_final(&md5ctx, md5_out);
    int md5_ok = hash_verify(&md5ctx, md5_out, 16);
    printf("MD5 verify:   %s\n", md5_ok ? "PASS" : "FAIL");
    hash_free(&md5ctx);

    hash_init(&bcryptctx, HASH_BCRYPT);
    hash_update(&bcryptctx, input, 16 + strlen(password));
    hash_final(&bcryptctx, bcrypt_out);
    int bcrypt_ok = hash_verify(&bcryptctx, bcrypt_out, 32);
    printf("Bcrypt verify: %s\n", bcrypt_ok ? "PASS" : "FAIL");
    hash_free(&bcryptctx);

    return 0;
}
