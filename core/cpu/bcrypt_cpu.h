// core/cpu/bcrypt_cpu.h
#ifndef BCRYPT_CPU_H
#define BCRYPT_CPU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Bcrypt CPU context.
 */
typedef struct bcrypt_cpu_ctx bcrypt_cpu_ctx_t;

/**
 * @brief Allocate and initialize a new Bcrypt CPU context.
 * @return Pointer to context
 */
bcrypt_cpu_ctx_t *bcrypt_cpu_create(void);

/**
 * @brief Update Bcrypt context with data (password, salt, etc).
 * @param ctx Context pointer
 * @param data Input data
 * @param len Length of data
 */
void bcrypt_cpu_update(bcrypt_cpu_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize Bcrypt and write output (hash length depends on cost).
 * @param ctx Context pointer
 * @param output Output buffer
 */
void bcrypt_cpu_final(bcrypt_cpu_ctx_t *ctx, uint8_t *output);

/**
 * @brief Verify Bcrypt hash against target.
 * @param ctx Context pointer
 * @param target Target hash
 * @param len Length of target
 * @return 1 if match, 0 otherwise
 */
int bcrypt_cpu_verify(bcrypt_cpu_ctx_t *ctx, const uint8_t *target, size_t len);

/**
 * @brief Free Bcrypt context.
 * @param ctx Context pointer
 */
void bcrypt_cpu_free(bcrypt_cpu_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // BCRYPT_CPU_H 