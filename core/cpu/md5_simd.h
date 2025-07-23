// core/cpu/md5_simd.h
#ifndef MD5_SIMD_H
#define MD5_SIMD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque MD5 SIMD context.
 */
typedef struct md5_simd_ctx md5_simd_ctx_t;

/**
 * @brief Allocate and initialize a new MD5 SIMD context.
 * @return Pointer to context
 */
md5_simd_ctx_t *md5_simd_create(void);

/**
 * @brief Update MD5 context with data (SIMD-accelerated).
 * @param ctx Context pointer
 * @param data Input data
 * @param len Length of data
 */
void md5_simd_update(md5_simd_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize MD5 and write output (16 bytes).
 * @param ctx Context pointer
 * @param output Output buffer (16 bytes)
 */
void md5_simd_final(md5_simd_ctx_t *ctx, uint8_t *output);

/**
 * @brief Verify MD5 hash against target.
 * @param ctx Context pointer
 * @param target Target hash (16 bytes)
 * @param len Length of target (should be 16)
 * @return 1 if match, 0 otherwise
 */
int md5_simd_verify(md5_simd_ctx_t *ctx, const uint8_t *target, size_t len);

/**
 * @brief Free MD5 context.
 * @param ctx Context pointer
 */
void md5_simd_free(md5_simd_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // MD5_SIMD_H 