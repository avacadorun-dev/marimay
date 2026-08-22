#ifndef MMAY_BASE_HASH_H
#define MMAY_BASE_HASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MarimaySha256 {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_length;
} MarimaySha256;

void mmay_sha256_init(MarimaySha256 *hash);

/* False means the SHA-256 2^64-bit input bound would be exceeded. */
bool mmay_sha256_update(MarimaySha256 *hash, const void *bytes, size_t length);

/* Consumes the current state and writes exactly 32 digest bytes. */
void mmay_sha256_final(MarimaySha256 *hash, unsigned char digest[32]);

bool mmay_sha256(const void *bytes, size_t length, unsigned char digest[32]);

/* Writes 64 lowercase hex digits followed by a zero sentinel. */
void mmay_sha256_hex(const unsigned char digest[32], char output[65]);

/* SipHash-2-4 for explicitly seeded in-memory hash tables. */
uint64_t mmay_hash_sip24(
    const void *bytes,
    size_t length,
    uint64_t key0,
    uint64_t key1
);

#endif
