#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/hash.h"
#include "mmay/base/map.h"

static void mmay_expect_sha256(const void *bytes, size_t length, const char *expected) {
    unsigned char digest[32];
    char hex[65];
    MMAY_EXPECT(mmay_sha256(bytes, length, digest));
    mmay_sha256_hex(digest, hex);
    MMAY_EXPECT(strcmp(hex, expected) == 0);
}

static void mmay_test_hashes(void) {
    static const unsigned char sip_input[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    static const uint64_t sip_key0 = 0x0706050403020100ULL;
    static const uint64_t sip_key1 = 0x0f0e0d0c0b0a0908ULL;
    unsigned char digest[32];
    char hex[65];
    unsigned char a_chunk[1000];
    MarimaySha256 hash;
    uint64_t old_count = 0;
    size_t index = 0;

    mmay_expect_sha256(
        NULL,
        0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    mmay_expect_sha256(
        "abc",
        3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    memset(a_chunk, 'a', sizeof(a_chunk));
    mmay_sha256_init(&hash);
    for (index = 0; index < 1000; index += 1) {
        MMAY_EXPECT(mmay_sha256_update(&hash, a_chunk, sizeof(a_chunk)));
    }
    mmay_sha256_final(&hash, digest);
    mmay_sha256_hex(digest, hex);
    MMAY_EXPECT(strcmp(
        hex,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
    ) == 0);

    mmay_sha256_init(&hash);
    hash.bit_count = UINT64_MAX - 7U;
    old_count = hash.bit_count;
    MMAY_EXPECT(!mmay_sha256_update(&hash, "x", 1));
    MMAY_EXPECT(hash.bit_count == old_count);

    MMAY_EXPECT(mmay_hash_sip24(NULL, 0, sip_key0, sip_key1)
        == 0x726fdb47dd0e0e31ULL);
    MMAY_EXPECT(mmay_hash_sip24(sip_input, 1, sip_key0, sip_key1)
        == 0x74f839c593dc67fdULL);
    MMAY_EXPECT(mmay_hash_sip24(sip_input, 7, sip_key0, sip_key1)
        == 0xab0200f58b01d137ULL);
    MMAY_EXPECT(mmay_hash_sip24(sip_input, 15, sip_key0, sip_key1)
        == 0xa129ca6149be45e5ULL);
}

static void mmay_test_siphash_word(void) {
    static const uint64_t values[] = {
        0, 1, UINT64_MAX, UINT64_C(0x8000000000000000),
        UINT64_C(0x7fffffffffffffff), UINT64_C(0x0706050403020100),
        UINT64_C(0xaaaaaaaaaaaaaaaa), UINT64_C(0x5555555555555555)
    };
    uint64_t random = UINT64_C(0x20260906c036c036);
    size_t index = 0;
    for (index = 0; index < 100000; index += 1) {
        uint64_t word, key0, key1;
        random = random * UINT64_C(6364136223846793005)
            + UINT64_C(1442695040888963407);
        word = index < sizeof(values) / sizeof(values[0]) ? values[index] : random;
        random = random * UINT64_C(6364136223846793005)
            + UINT64_C(1442695040888963407);
        key0 = random;
        random = random * UINT64_C(6364136223846793005)
            + UINT64_C(1442695040888963407);
        key1 = random;
        MMAY_EXPECT(mmay_hash_sip24_word(word, key0, key1)
            == mmay_hash_sip24(&word, sizeof(word), key0, key1));
        MMAY_EXPECT(mmay_hash_sip24_word(word, 0, UINT64_MAX)
            == mmay_hash_sip24(&word, sizeof(word), 0, UINT64_MAX));
    }
}

static void mmay_test_map_behavior(void) {
    MarimayMap map;
    size_t index = 0;
    static const unsigned char binary_key[] = {'a', 0, 'b'};
    int binary_value = 700;
    MMAY_EXPECT(mmay_map_init(
        &map,
        mmay_system_allocator(),
        sizeof(int),
        0x0123456789abcdefULL,
        0xfedcba9876543210ULL
    ));
    for (index = 0; index < 500; index += 1) {
        char key[32];
        int value = (int)(index * 3);
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(key));
        if (length > 0 && (size_t)length < sizeof(key)) {
            MMAY_EXPECT(mmay_map_put(&map, key, (size_t)length, &value));
        }
    }
    MMAY_EXPECT(map.length == 500);
    for (index = 0; index < 500; index += 1) {
        char key[32];
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        const int *value = mmay_map_get_const(&map, key, (size_t)length);
        MMAY_EXPECT(value != NULL);
        if (value != NULL) {
            MMAY_EXPECT(*value == (int)(index * 3));
        }
    }
    for (index = 0; index < 500; index += 2) {
        char key[32];
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        MMAY_EXPECT(mmay_map_remove(&map, key, (size_t)length));
        MMAY_EXPECT(!mmay_map_remove(&map, key, (size_t)length));
    }
    MMAY_EXPECT(map.length == 250);
    MMAY_EXPECT(mmay_map_put(&map, binary_key, sizeof(binary_key), &binary_value));
    MMAY_EXPECT(*(const int *)mmay_map_get_const(&map, binary_key, sizeof(binary_key)) == 700);
    binary_value = 701;
    MMAY_EXPECT(mmay_map_put(&map, binary_key, sizeof(binary_key), &binary_value));
    MMAY_EXPECT(*(int *)mmay_map_get(&map, binary_key, sizeof(binary_key)) == 701);
    MMAY_EXPECT((uintptr_t)mmay_map_get(&map, binary_key, sizeof(binary_key)) % _Alignof(int) == 0);
    mmay_map_dispose(&map);
}

static void mmay_test_map_failures(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMap map;
        int value = 42;
        bool inserted = false;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_map_init(&map, allocator, sizeof(value), 1, 2));
        inserted = mmay_map_put(&map, "key", 3, &value);
        if (inserted) {
            reached_success = true;
            MMAY_EXPECT(map.length == 1);
        } else {
            MMAY_EXPECT(map.length == 0);
            MMAY_EXPECT(mmay_map_get(&map, "key", 3) == NULL);
        }
        mmay_map_dispose(&map);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMap map;
        size_t index = 0;
        bool inserted = false;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_map_init(&map, allocator, sizeof(int), 3, 4));
        for (index = 0; index < 11; index += 1) {
            char key[16];
            int value = (int)index;
            int length = snprintf(key, sizeof(key), "key-%zu", index);
            MMAY_EXPECT(length > 0 && (size_t)length < sizeof(key));
            MMAY_EXPECT(mmay_map_put(&map, key, (size_t)length, &value));
        }
        state.failure_index = state.attempts + failure_index;
        {
            int value = 99;
            inserted = mmay_map_put(&map, "growth-key", strlen("growth-key"), &value);
        }
        if (inserted) {
            reached_success = true;
            MMAY_EXPECT(map.length == 12);
        } else {
            MMAY_EXPECT(map.length == 11);
            MMAY_EXPECT(mmay_map_get(&map, "growth-key", strlen("growth-key")) == NULL);
        }
        {
            size_t attempts = state.attempts;
            int update = 1234;
            state.failure_index = attempts;
            MMAY_EXPECT(mmay_map_put(&map, "key-0", strlen("key-0"), &update));
            MMAY_EXPECT(state.attempts == attempts);
            MMAY_EXPECT(*(int *)mmay_map_get(&map, "key-0", strlen("key-0")) == update);
        }
        mmay_map_dispose(&map);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_map_max_alignment(void) {
    bool completed = false;
    for (size_t failure = 0; failure < 32; failure += 1) {
        MarimayFailAllocator state;
        MarimayMap map;
        MarimayMaxAlign value;
        memset(&value, 0x5a, sizeof(value));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MMAY_EXPECT(mmay_map_init(&map, mmay_fail_allocator(&state), sizeof(value), 1, 2));
        completed = mmay_map_put(&map, "aligned", 7, &value);
        if (completed) {
            const void *copy = mmay_map_get_const(&map, "aligned", 7);
            MMAY_EXPECT(copy != NULL);
            MMAY_EXPECT((uintptr_t)copy % _Alignof(MarimayMaxAlign) == 0);
            if (copy != NULL) MMAY_EXPECT(memcmp(copy, &value, sizeof(value)) == 0);
        } else {
            MMAY_EXPECT(map.length == 0);
        }
        mmay_map_dispose(&map);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (completed) break;
    }
    MMAY_EXPECT(completed);
}

void mmay_test_hash_map(void) {
    mmay_test_hashes();
    mmay_test_siphash_word();
    mmay_test_map_behavior();
    mmay_test_map_failures();
    mmay_test_map_max_alignment();
}
