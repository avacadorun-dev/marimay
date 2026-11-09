#include "mmay/base/hash.h"

#include <string.h>

static const uint32_t mmay_sha256_constants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static uint32_t mmay_rotate_right32(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32U - count));
}

static uint64_t mmay_rotate_left64(uint64_t value, unsigned int count) {
    return (value << count) | (value >> (64U - count));
}

static uint32_t mmay_load_big32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24)
        | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8)
        | (uint32_t)bytes[3];
}

static uint64_t mmay_load_little64(const unsigned char *bytes) {
    return (uint64_t)bytes[0]
        | ((uint64_t)bytes[1] << 8)
        | ((uint64_t)bytes[2] << 16)
        | ((uint64_t)bytes[3] << 24)
        | ((uint64_t)bytes[4] << 32)
        | ((uint64_t)bytes[5] << 40)
        | ((uint64_t)bytes[6] << 48)
        | ((uint64_t)bytes[7] << 56);
}

static void mmay_store_big32(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)(value >> 24);
    bytes[1] = (unsigned char)(value >> 16);
    bytes[2] = (unsigned char)(value >> 8);
    bytes[3] = (unsigned char)value;
}

static void mmay_sha256_compress(MarimaySha256 *hash, const unsigned char block[64]) {
    uint32_t words[64];
    uint32_t a = 0;
    uint32_t b = 0;
    uint32_t c = 0;
    uint32_t d = 0;
    uint32_t e = 0;
    uint32_t f = 0;
    uint32_t g = 0;
    uint32_t h = 0;
    size_t index = 0;
    for (index = 0; index < 16; index += 1) {
        words[index] = mmay_load_big32(block + index * 4);
    }
    for (index = 16; index < 64; index += 1) {
        uint32_t previous15 = words[index - 15];
        uint32_t previous2 = words[index - 2];
        uint32_t small0 = mmay_rotate_right32(previous15, 7)
            ^ mmay_rotate_right32(previous15, 18) ^ (previous15 >> 3);
        uint32_t small1 = mmay_rotate_right32(previous2, 17)
            ^ mmay_rotate_right32(previous2, 19) ^ (previous2 >> 10);
        words[index] = words[index - 16] + small0 + words[index - 7] + small1;
    }
    a = hash->state[0];
    b = hash->state[1];
    c = hash->state[2];
    d = hash->state[3];
    e = hash->state[4];
    f = hash->state[5];
    g = hash->state[6];
    h = hash->state[7];
    for (index = 0; index < 64; index += 1) {
        uint32_t sum1 = mmay_rotate_right32(e, 6) ^ mmay_rotate_right32(e, 11)
            ^ mmay_rotate_right32(e, 25);
        uint32_t choose = (e & f) ^ ((~e) & g);
        uint32_t temporary1 = h + sum1 + choose + mmay_sha256_constants[index]
            + words[index];
        uint32_t sum0 = mmay_rotate_right32(a, 2) ^ mmay_rotate_right32(a, 13)
            ^ mmay_rotate_right32(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temporary2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    hash->state[0] += a;
    hash->state[1] += b;
    hash->state[2] += c;
    hash->state[3] += d;
    hash->state[4] += e;
    hash->state[5] += f;
    hash->state[6] += g;
    hash->state[7] += h;
}

void mmay_sha256_init(MarimaySha256 *hash) {
    hash->state[0] = 0x6a09e667U;
    hash->state[1] = 0xbb67ae85U;
    hash->state[2] = 0x3c6ef372U;
    hash->state[3] = 0xa54ff53aU;
    hash->state[4] = 0x510e527fU;
    hash->state[5] = 0x9b05688cU;
    hash->state[6] = 0x1f83d9abU;
    hash->state[7] = 0x5be0cd19U;
    hash->bit_count = 0;
    hash->block_length = 0;
}

bool mmay_sha256_update(MarimaySha256 *hash, const void *bytes, size_t length) {
    const unsigned char *cursor = bytes;
    size_t remaining = length;
    if (hash == NULL || (bytes == NULL && length != 0)) {
        return false;
    }
    if ((uint64_t)length > (UINT64_MAX - hash->bit_count) / 8U) {
        return false;
    }
    hash->bit_count += (uint64_t)length * 8U;
    while (remaining != 0) {
        size_t available = sizeof(hash->block) - hash->block_length;
        size_t copied = remaining < available ? remaining : available;
        memcpy(hash->block + hash->block_length, cursor, copied);
        hash->block_length += copied;
        cursor += copied;
        remaining -= copied;
        if (hash->block_length == sizeof(hash->block)) {
            mmay_sha256_compress(hash, hash->block);
            hash->block_length = 0;
        }
    }
    return true;
}

void mmay_sha256_final(MarimaySha256 *hash, unsigned char digest[32]) {
    uint64_t bit_count = hash->bit_count;
    size_t index = 0;
    hash->block[hash->block_length] = 0x80U;
    hash->block_length += 1;
    if (hash->block_length > 56) {
        memset(hash->block + hash->block_length, 0, 64 - hash->block_length);
        mmay_sha256_compress(hash, hash->block);
        hash->block_length = 0;
    }
    memset(hash->block + hash->block_length, 0, 56 - hash->block_length);
    for (index = 0; index < 8; index += 1) {
        hash->block[63 - index] = (unsigned char)(bit_count >> (index * 8));
    }
    mmay_sha256_compress(hash, hash->block);
    for (index = 0; index < 8; index += 1) {
        mmay_store_big32(digest + index * 4, hash->state[index]);
    }
    hash->block_length = 0;
}

bool mmay_sha256(const void *bytes, size_t length, unsigned char digest[32]) {
    MarimaySha256 hash;
    if (digest == NULL) {
        return false;
    }
    mmay_sha256_init(&hash);
    if (!mmay_sha256_update(&hash, bytes, length)) {
        return false;
    }
    mmay_sha256_final(&hash, digest);
    return true;
}

void mmay_sha256_hex(const unsigned char digest[32], char output[65]) {
    static const char digits[] = "0123456789abcdef";
    size_t index = 0;
    for (index = 0; index < 32; index += 1) {
        output[index * 2] = digits[digest[index] >> 4];
        output[index * 2 + 1] = digits[digest[index] & 0x0fU];
    }
    output[64] = '\0';
}

#define MMAY_SIP_ROUND() \
    do { \
        value0 += value1; \
        value1 = mmay_rotate_left64(value1, 13); \
        value1 ^= value0; \
        value0 = mmay_rotate_left64(value0, 32); \
        value2 += value3; \
        value3 = mmay_rotate_left64(value3, 16); \
        value3 ^= value2; \
        value0 += value3; \
        value3 = mmay_rotate_left64(value3, 21); \
        value3 ^= value0; \
        value2 += value1; \
        value1 = mmay_rotate_left64(value1, 17); \
        value1 ^= value2; \
        value2 = mmay_rotate_left64(value2, 32); \
    } while (0)

uint64_t mmay_hash_sip24(
    const void *bytes,
    size_t length,
    uint64_t key0,
    uint64_t key1
) {
    const unsigned char *input = bytes;
    size_t offset = 0;
    uint64_t value0 = 0x736f6d6570736575ULL ^ key0;
    uint64_t value1 = 0x646f72616e646f6dULL ^ key1;
    uint64_t value2 = 0x6c7967656e657261ULL ^ key0;
    uint64_t value3 = 0x7465646279746573ULL ^ key1;
    uint64_t final = (uint64_t)(length & 0xffU) << 56;
    if (input == NULL && length != 0) {
        return 0;
    }
    while (length - offset >= 8) {
        uint64_t message = mmay_load_little64(input + offset);
        value3 ^= message;
        MMAY_SIP_ROUND();
        MMAY_SIP_ROUND();
        value0 ^= message;
        offset += 8;
    }
    switch (length - offset) {
        case 7:
            final |= (uint64_t)input[offset + 6] << 48;
            /* fall through */
        case 6:
            final |= (uint64_t)input[offset + 5] << 40;
            /* fall through */
        case 5:
            final |= (uint64_t)input[offset + 4] << 32;
            /* fall through */
        case 4:
            final |= (uint64_t)input[offset + 3] << 24;
            /* fall through */
        case 3:
            final |= (uint64_t)input[offset + 2] << 16;
            /* fall through */
        case 2:
            final |= (uint64_t)input[offset + 1] << 8;
            /* fall through */
        case 1:
            final |= (uint64_t)input[offset];
            /* fall through */
        case 0:
            break;
    }
    value3 ^= final;
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    value0 ^= final;
    value2 ^= 0xffU;
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    return value0 ^ value1 ^ value2 ^ value3;
}

uint64_t mmay_hash_sip24_word(uint64_t word, uint64_t key0, uint64_t key1) {
    uint64_t value0 = 0x736f6d6570736575ULL ^ key0;
    uint64_t value1 = 0x646f72616e646f6dULL ^ key1;
    uint64_t value2 = 0x6c7967656e657261ULL ^ key0;
    uint64_t value3 = 0x7465646279746573ULL ^ key1;
    const uint64_t final = UINT64_C(8) << 56;
    /* Preserve the generic routine's byte order on every C17 host. Compilers
     * fold this native-word load without a variable-length byte/tail loop. */
    const uint64_t message = mmay_load_little64((const unsigned char *)&word);
    value3 ^= message;
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    value0 ^= message;
    value3 ^= final;
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    value0 ^= final;
    value2 ^= 0xffU;
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    MMAY_SIP_ROUND();
    return value0 ^ value1 ^ value2 ^ value3;
}

#undef MMAY_SIP_ROUND

#define MMAY_HASH_P1 UINT64_C(0x9e3779b185ebca87)
#define MMAY_HASH_P2 UINT64_C(0xc2b2ae3d27d4eb4f)
#define MMAY_HASH_P3 UINT64_C(0x165667b19e3779f9)
#define MMAY_HASH_P4 UINT64_C(0x85ebca77c2b2ae63)
#define MMAY_HASH_P5 UINT64_C(0x27d4eb2f165667c5)

static uint64_t mmay_hash_round(uint64_t accumulator, uint64_t lane) {
    accumulator += lane * MMAY_HASH_P2;
    accumulator = mmay_rotate_left64(accumulator, 31);
    accumulator *= MMAY_HASH_P1;
    return accumulator;
}

static uint64_t mmay_hash_merge_accumulator(uint64_t hash, uint64_t accumulator) {
    hash ^= mmay_hash_round(0, accumulator);
    hash = hash * MMAY_HASH_P1 + MMAY_HASH_P4;
    return hash;
}

static uint32_t mmay_load_little32(const unsigned char *bytes) {
    return (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
}

static uint64_t mmay_hash_avalanche(uint64_t hash) {
    hash ^= hash >> 33;
    hash *= MMAY_HASH_P2;
    hash ^= hash >> 29;
    hash *= MMAY_HASH_P3;
    hash ^= hash >> 32;
    return hash;
}

uint64_t mmay_hash_seeded(
    const void *bytes,
    size_t length,
    uint64_t seed0,
    uint64_t seed1
) {
    const unsigned char *input = bytes;
    size_t offset = 0;
    uint64_t hash = 0;
    const uint64_t seed = seed0 ^ mmay_rotate_left64(seed1, 32);
    if (input == NULL && length != 0) {
        return 0;
    }
    if (length >= 32) {
        uint64_t accumulator1 = seed + MMAY_HASH_P1 + MMAY_HASH_P2;
        uint64_t accumulator2 = seed + MMAY_HASH_P2;
        uint64_t accumulator3 = seed;
        uint64_t accumulator4 = seed - MMAY_HASH_P1;
        while (length - offset >= 32) {
            accumulator1 = mmay_hash_round(
                accumulator1, mmay_load_little64(input + offset));
            accumulator2 = mmay_hash_round(
                accumulator2, mmay_load_little64(input + offset + 8));
            accumulator3 = mmay_hash_round(
                accumulator3, mmay_load_little64(input + offset + 16));
            accumulator4 = mmay_hash_round(
                accumulator4, mmay_load_little64(input + offset + 24));
            offset += 32;
        }
        hash = mmay_rotate_left64(accumulator1, 1)
            + mmay_rotate_left64(accumulator2, 7)
            + mmay_rotate_left64(accumulator3, 12)
            + mmay_rotate_left64(accumulator4, 18);
        hash = mmay_hash_merge_accumulator(hash, accumulator1);
        hash = mmay_hash_merge_accumulator(hash, accumulator2);
        hash = mmay_hash_merge_accumulator(hash, accumulator3);
        hash = mmay_hash_merge_accumulator(hash, accumulator4);
    } else {
        hash = seed + MMAY_HASH_P5;
    }
    hash += length;
    while (length - offset >= 8) {
        uint64_t lane = mmay_hash_round(0, mmay_load_little64(input + offset));
        hash ^= lane;
        hash = mmay_rotate_left64(hash, 27) * MMAY_HASH_P1 + MMAY_HASH_P4;
        offset += 8;
    }
    if (length - offset >= 4) {
        hash ^= (uint64_t)mmay_load_little32(input + offset) * MMAY_HASH_P1;
        hash = mmay_rotate_left64(hash, 23) * MMAY_HASH_P2 + MMAY_HASH_P3;
        offset += 4;
    }
    while (offset < length) {
        hash ^= (uint64_t)input[offset] * MMAY_HASH_P5;
        hash = mmay_rotate_left64(hash, 11) * MMAY_HASH_P1;
        offset += 1;
    }
    return mmay_hash_avalanche(hash);
}

uint64_t mmay_hash_seeded_word(uint64_t word, uint64_t seed0, uint64_t seed1) {
    /* Preserve the generic routine's byte order on every C17 host. Compilers
     * fold this native-word load without a variable-length byte/tail loop. */
    const uint64_t message = mmay_load_little64((const unsigned char *)&word);
    uint64_t hash = (seed0 ^ mmay_rotate_left64(seed1, 32)) + MMAY_HASH_P5
        + sizeof(word);
    uint64_t lane = mmay_hash_round(0, message);
    hash ^= lane;
    hash = mmay_rotate_left64(hash, 27) * MMAY_HASH_P1 + MMAY_HASH_P4;
    return mmay_hash_avalanche(hash);
}
