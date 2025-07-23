// core/hash_algorithms.h
#ifndef HASH_ALGORITHMS_H
#define HASH_ALGORITHMS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Supported hash algorithm types.
 */
typedef enum {
    HASH_MD5,
    HASH_BCRYPT,
    HASH_SHA256,
    HASH_UNKNOWN
} hash_type_t;

/**
 * @brief Opaque hash context for all algorithms.
 */
typedef struct {
    hash_type_t type;   /**< Algorithm type */
    void *context;      /**< Algorithm-specific context */
} hash_ctx_t;

/**
 * @brief Initialize a hash context for the given type.
 * @param ctx Pointer to hash context
 * @param type Hash algorithm type
 */
void hash_init(hash_ctx_t *ctx, hash_type_t type);

/**
 * @brief Update hash context with data.
 * @param ctx Pointer to hash context
 * @param data Input data
 * @param len Length of input data
 */
void hash_update(hash_ctx_t *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize hash and write output.
 * @param ctx Pointer to hash context
 * @param output Output buffer (size depends on algorithm)
 */
void hash_final(hash_ctx_t *ctx, uint8_t *output);

/**
 * @brief Verify a hash against a target value.
 * @param ctx Pointer to hash context
 * @param target Target hash value
 * @param len Length of target
 * @return 1 if match, 0 otherwise
 */
int hash_verify(hash_ctx_t *ctx, const uint8_t *target, size_t len);

/**
 * @brief Free any resources associated with the hash context.
 * @param ctx Pointer to hash context
 */
void hash_free(hash_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // HASH_ALGORITHMS_H 