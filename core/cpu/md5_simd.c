// core/cpu/md5_simd.c
#include "md5_simd.h"
#include <stdlib.h>
#include <string.h>
#include <immintrin.h> // AVX2/AVX-512
#include <stdint.h>

/**
 * @brief MD5 context for SIMD operations.
 */
struct md5_simd_ctx {
    uint32_t state[4];
    uint64_t bitcount;
    uint8_t buffer[64];
    size_t buffer_len;
};

// Forward declarations for SIMD and fallback
static void md5_simd_process_block(struct md5_simd_ctx *ctx, const uint8_t *block);
static void md5_scalar_process_block(struct md5_simd_ctx *ctx, const uint8_t *block);

md5_simd_ctx_t *md5_simd_create(void) {
    md5_simd_ctx_t *ctx = (md5_simd_ctx_t *)calloc(1, sizeof(md5_simd_ctx_t));
    if (!ctx) return NULL;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->bitcount = 0;
    ctx->buffer_len = 0;
    return ctx;
}

void md5_simd_update(md5_simd_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !data) return;
    size_t offset = 0;
    // Fill buffer if partial
    if (ctx->buffer_len > 0) {
        size_t to_copy = 64 - ctx->buffer_len;
        if (to_copy > len) to_copy = len;
        memcpy(ctx->buffer + ctx->buffer_len, data, to_copy);
        ctx->buffer_len += to_copy;
        offset += to_copy;
        if (ctx->buffer_len == 64) {
            md5_simd_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
    // Process full blocks
    while (offset + 64 <= len) {
        md5_simd_process_block(ctx, data + offset);
        offset += 64;
    }
    // Buffer remaining
    if (offset < len) {
        memcpy(ctx->buffer, data + offset, len - offset);
        ctx->buffer_len = len - offset;
    }
    ctx->bitcount += len * 8;
}

void md5_simd_final(md5_simd_ctx_t *ctx, uint8_t *output) {
    if (!ctx || !output) return;
    // Padding
    uint8_t pad[64] = {0x80};
    size_t pad_len = (ctx->buffer_len < 56) ? (56 - ctx->buffer_len) : (120 - ctx->buffer_len);
    uint64_t bitcount_le = ctx->bitcount;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    bitcount_le = __builtin_bswap64(bitcount_le);
#endif
    md5_simd_update(ctx, pad, pad_len);
    md5_simd_update(ctx, (uint8_t *)&bitcount_le, 8);
    // Output
    for (int i = 0; i < 4; ++i) {
        output[i*4+0] = (ctx->state[i] >> 0) & 0xff;
        output[i*4+1] = (ctx->state[i] >> 8) & 0xff;
        output[i*4+2] = (ctx->state[i] >> 16) & 0xff;
        output[i*4+3] = (ctx->state[i] >> 24) & 0xff;
    }
}

int md5_simd_verify(md5_simd_ctx_t *ctx, const uint8_t *target, size_t len) {
    if (!ctx || !target || len != 16) return 0;
    uint8_t out[16];
    md5_simd_final(ctx, out);
    return (memcmp(out, target, 16) == 0) ? 1 : 0;
}

void md5_simd_free(md5_simd_ctx_t *ctx) {
    if (ctx) free(ctx);
}

// SIMD-accelerated block processing (AVX2/AVX-512 if available)
static void md5_simd_process_block(struct md5_simd_ctx *ctx, const uint8_t *block) {
#if defined(__AVX2__)
    // TODO: AVX2 implementation for parallel MD5 (multiple blocks)
    md5_scalar_process_block(ctx, block); // Fallback for now
#elif defined(__AVX512F__)
    // TODO: AVX-512 implementation for parallel MD5
    md5_scalar_process_block(ctx, block); // Fallback for now
#else
    md5_scalar_process_block(ctx, block);
#endif
}

// Scalar MD5 block processing (RFC 1321)
static void md5_scalar_process_block(struct md5_simd_ctx *ctx, const uint8_t *block) {
    // RFC 1321 constants
    static const uint32_t T[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    #define F(x,y,z) (((x)&(y)) | ((~x)&(z)))
    #define G(x,y,z) (((x)&(z)) | ((y)&(~z)))
    #define H(x,y,z) ((x)^(y)^(z))
    #define I(x,y,z) ((y)^((x)|(~z)))
    #define ROTATE_LEFT(x,n) (((x)<<(n))|((x)>>(32-(n))))
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t X[16];
    for (int i = 0; i < 16; ++i) {
        X[i] = (uint32_t)block[i*4+0] | ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
    }
    // Round 1
    #define FF(a,b,c,d,x,s,ac) { a += F(b,c,d) + X[x] + ac; a = ROTATE_LEFT(a,s); a += b; }
    FF(a,b,c,d, 0, 7,T[ 0]); FF(d,a,b,c, 1,12,T[ 1]); FF(c,d,a,b, 2,17,T[ 2]); FF(b,c,d,a, 3,22,T[ 3]);
    FF(a,b,c,d, 4, 7,T[ 4]); FF(d,a,b,c, 5,12,T[ 5]); FF(c,d,a,b, 6,17,T[ 6]); FF(b,c,d,a, 7,22,T[ 7]);
    FF(a,b,c,d, 8, 7,T[ 8]); FF(d,a,b,c, 9,12,T[ 9]); FF(c,d,a,b,10,17,T[10]); FF(b,c,d,a,11,22,T[11]);
    FF(a,b,c,d,12, 7,T[12]); FF(d,a,b,c,13,12,T[13]); FF(c,d,a,b,14,17,T[14]); FF(b,c,d,a,15,22,T[15]);
    // Round 2
    #define GG(a,b,c,d,x,s,ac) { a += G(b,c,d) + X[x] + ac; a = ROTATE_LEFT(a,s); a += b; }
    GG(a,b,c,d, 1, 5,T[16]); GG(d,a,b,c, 6, 9,T[17]); GG(c,d,a,b,11,14,T[18]); GG(b,c,d,a, 0,20,T[19]);
    GG(a,b,c,d, 5, 5,T[20]); GG(d,a,b,c,10, 9,T[21]); GG(c,d,a,b,15,14,T[22]); GG(b,c,d,a, 4,20,T[23]);
    GG(a,b,c,d, 9, 5,T[24]); GG(d,a,b,c,14, 9,T[25]); GG(c,d,a,b, 3,14,T[26]); GG(b,c,d,a, 8,20,T[27]);
    GG(a,b,c,d,13, 5,T[28]); GG(d,a,b,c, 2, 9,T[29]); GG(c,d,a,b, 7,14,T[30]); GG(b,c,d,a,12,20,T[31]);
    // Round 3
    #define HH(a,b,c,d,x,s,ac) { a += H(b,c,d) + X[x] + ac; a = ROTATE_LEFT(a,s); a += b; }
    HH(a,b,c,d, 5, 4,T[32]); HH(d,a,b,c, 8,11,T[33]); HH(c,d,a,b,11,16,T[34]); HH(b,c,d,a,14,23,T[35]);
    HH(a,b,c,d, 1, 4,T[36]); HH(d,a,b,c, 4,11,T[37]); HH(c,d,a,b, 7,16,T[38]); HH(b,c,d,a,10,23,T[39]);
    HH(a,b,c,d,13, 4,T[40]); HH(d,a,b,c, 0,11,T[41]); HH(c,d,a,b, 3,16,T[42]); HH(b,c,d,a, 6,23,T[43]);
    // Round 4
    #define II(a,b,c,d,x,s,ac) { a += I(b,c,d) + X[x] + ac; a = ROTATE_LEFT(a,s); a += b; }
    II(a,b,c,d, 0, 6,T[44]); II(d,a,b,c, 7,10,T[45]); II(c,d,a,b,14,15,T[46]); II(b,c,d,a, 5,21,T[47]);
    II(a,b,c,d,12, 6,T[48]); II(d,a,b,c, 3,10,T[49]); II(c,d,a,b,10,15,T[50]); II(b,c,d,a, 1,21,T[51]);
    II(a,b,c,d, 8, 6,T[52]); II(d,a,b,c,15,10,T[53]); II(c,d,a,b, 6,15,T[54]); II(b,c,d,a,13,21,T[55]);
    II(a,b,c,d, 4, 6,T[56]); II(d,a,b,c,11,10,T[57]); II(c,d,a,b, 2,15,T[58]); II(b,c,d,a, 9,21,T[59]);
    // Update state
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    #undef F
    #undef G
    #undef H
    #undef I
    #undef ROTATE_LEFT
    #undef FF
    #undef GG
    #undef HH
    #undef II
}
