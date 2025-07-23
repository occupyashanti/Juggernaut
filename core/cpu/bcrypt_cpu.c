// core/cpu/bcrypt_cpu.c
#include "bcrypt_cpu.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Bcrypt context for CPU operations.
 */
struct bcrypt_cpu_ctx {
    uint8_t password[72];
    size_t password_len;
    uint8_t salt[16];
    size_t salt_len;
    int cost;
    uint8_t hash[32];
    int finalized;
};

bcrypt_cpu_ctx_t *bcrypt_cpu_create(void) {
    bcrypt_cpu_ctx_t *ctx = (bcrypt_cpu_ctx_t *)calloc(1, sizeof(bcrypt_cpu_ctx_t));
    if (!ctx) return NULL;
    ctx->cost = 10; // Default cost
    ctx->finalized = 0;
    return ctx;
}

void bcrypt_cpu_update(bcrypt_cpu_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !data || len == 0) return;
    // For demo: treat first 16 bytes as salt, rest as password
    if (len > 16) {
        memcpy(ctx->salt, data, 16);
        ctx->salt_len = 16;
        memcpy(ctx->password, data + 16, len - 16);
        ctx->password_len = len - 16;
    } else {
        memcpy(ctx->salt, data, len);
        ctx->salt_len = len;
        ctx->password_len = 0;
    }
    ctx->finalized = 0;
}

void bcrypt_cpu_final(bcrypt_cpu_ctx_t *ctx, uint8_t *output) {
    if (!ctx || !output) return;
    // NOTE: Real bcrypt implementation required here.
    // For now, fill with dummy data (0x42). Replace with a call to a portable bcrypt C implementation if available.
    memset(ctx->hash, 0x42, 32);
    memcpy(output, ctx->hash, 32);
    ctx->finalized = 1;
}

int bcrypt_cpu_verify(bcrypt_cpu_ctx_t *ctx, const uint8_t *target, size_t len) {
    if (!ctx || !target || len != 32) return 0;
    uint8_t out[32];
    bcrypt_cpu_final(ctx, out);
    return (memcmp(out, target, 32) == 0) ? 1 : 0;
}

void bcrypt_cpu_free(bcrypt_cpu_ctx_t *ctx) {
    if (ctx) free(ctx);
}
