#ifndef JUG_AUTO_DETECT_H
#define JUG_AUTO_DETECT_H

#include <stddef.h>
#include "../core/hash_algorithms.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hash type guess with confidence.
 */
typedef struct {
    hash_type_t type;
    double confidence;   // 0.0 - 1.0
} jug_hash_guess_t;

/**
 * @brief Auto-detect hash types from a file.
 * @param path Path to file
 * @param out Output array
 * @param max_out Max guesses
 * @return Number of guesses
 */
size_t jug_auto_detect_file(const char *path, jug_hash_guess_t *out, size_t max_out);

/**
 * @brief Auto-detect hash types from a buffer.
 * @param buf Input buffer
 * @param len Buffer length
 * @param out Output array
 * @param max_out Max guesses
 * @return Number of guesses
 */
size_t jug_auto_detect_buffer(const char *buf, size_t len, jug_hash_guess_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif // JUG_AUTO_DETECT_H 