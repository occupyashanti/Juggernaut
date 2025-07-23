// core/hash_algorithms.c
#include "hash_algorithms.h"
#include "cpu/md5_simd.h"
#include "cpu/bcrypt_cpu.h"
#include <stdlib.h>
#include <string.h>

#ifdef USE_OPENMP
#include <omp.h>
#endif

#ifdef USE_CUDA
// CUDA/OpenCL integration hooks would go here (future extension)
#endif

/** Internal function pointer table for hash operations */
typedef struct {
    void (*init)(void **ctx);
    void (*update)(void *ctx, const uint8_t *data, size_t len);
    void (*final)(void *ctx, uint8_t *output);
    int  (*verify)(void *ctx, const uint8_t *target, size_t len);
    void (*free)(void *ctx);
} hash_ops_t;

// Forward declarations for each algorithm
static void md5_init(void **ctx);
static void md5_update(void *ctx, const uint8_t *data, size_t len);
static void md5_final(void *ctx, uint8_t *output);
static int  md5_verify(void *ctx, const uint8_t *target, size_t len);
static void md5_free(void *ctx);

static void bcrypt_init(void **ctx);
static void bcrypt_update(void *ctx, const uint8_t *data, size_t len);
static void bcrypt_final(void *ctx, uint8_t *output);
static int  bcrypt_verify(void *ctx, const uint8_t *target, size_t len);
static void bcrypt_free(void *ctx);

// SHA256 stubs (to be implemented)
static void sha256_init(void **ctx) { *ctx = NULL; }
static void sha256_update(void *ctx, const uint8_t *data, size_t len) { (void)ctx; (void)data; (void)len; }
static void sha256_final(void *ctx, uint8_t *output) { (void)ctx; (void)output; }
static int  sha256_verify(void *ctx, const uint8_t *target, size_t len) { (void)ctx; (void)target; (void)len; return 0; }
static void sha256_free(void *ctx) { (void)ctx; }

static const hash_ops_t hash_ops_table[] = {
    [HASH_MD5] = { md5_init, md5_update, md5_final, md5_verify, md5_free },
    [HASH_BCRYPT] = { bcrypt_init, bcrypt_update, bcrypt_final, bcrypt_verify, bcrypt_free },
    [HASH_SHA256] = { sha256_init, sha256_update, sha256_final, sha256_verify, sha256_free },
    [HASH_UNKNOWN] = { NULL, NULL, NULL, NULL, NULL }
};

void hash_init(hash_ctx_t *ctx, hash_type_t type) {
    if (!ctx || type >= HASH_UNKNOWN) return;
    ctx->type = type;
    ctx->context = NULL;
    hash_ops_table[type].init(&ctx->context);
}

void hash_update(hash_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !ctx->context) return;
    hash_ops_table[ctx->type].update(ctx->context, data, len);
}

void hash_final(hash_ctx_t *ctx, uint8_t *output) {
    if (!ctx || !ctx->context) return;
    hash_ops_table[ctx->type].final(ctx->context, output);
}

int hash_verify(hash_ctx_t *ctx, const uint8_t *target, size_t len) {
    if (!ctx || !ctx->context) return 0;
    return hash_ops_table[ctx->type].verify(ctx->context, target, len);
}

void hash_free(hash_ctx_t *ctx) {
    if (!ctx || !ctx->context) return;
    hash_ops_table[ctx->type].free(ctx->context);
    ctx->context = NULL;
}

// MD5 wrappers
static void md5_init(void **ctx) { *ctx = md5_simd_create(); }
static void md5_update(void *ctx, const uint8_t *data, size_t len) { md5_simd_update(ctx, data, len); }
static void md5_final(void *ctx, uint8_t *output) { md5_simd_final(ctx, output); }
static int  md5_verify(void *ctx, const uint8_t *target, size_t len) { return md5_simd_verify(ctx, target, len); }
static void md5_free(void *ctx) { md5_simd_free(ctx); }

// Bcrypt wrappers
static void bcrypt_init(void **ctx) { *ctx = bcrypt_cpu_create(); }
static void bcrypt_update(void *ctx, const uint8_t *data, size_t len) { bcrypt_cpu_update(ctx, data, len); }
static void bcrypt_final(void *ctx, uint8_t *output) { bcrypt_cpu_final(ctx, output); }
static int  bcrypt_verify(void *ctx, const uint8_t *target, size_t len) { return bcrypt_cpu_verify(ctx, target, len); }
static void bcrypt_free(void *ctx) { bcrypt_cpu_free(ctx); }
