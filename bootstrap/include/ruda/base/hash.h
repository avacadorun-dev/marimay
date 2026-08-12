#ifndef RUDA_BASE_HASH_H
#define RUDA_BASE_HASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RudaSha256 {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_length;
} RudaSha256;

void ruda_sha256_init(RudaSha256 *hash);

/* False means the SHA-256 2^64-bit input bound would be exceeded. */
bool ruda_sha256_update(RudaSha256 *hash, const void *bytes, size_t length);

/* Consumes the current state and writes exactly 32 digest bytes. */
void ruda_sha256_final(RudaSha256 *hash, unsigned char digest[32]);

bool ruda_sha256(const void *bytes, size_t length, unsigned char digest[32]);

/* Writes 64 lowercase hex digits followed by a zero sentinel. */
void ruda_sha256_hex(const unsigned char digest[32], char output[65]);

/* SipHash-2-4 for explicitly seeded in-memory hash tables. */
uint64_t ruda_hash_sip24(
    const void *bytes,
    size_t length,
    uint64_t key0,
    uint64_t key1
);

#endif
