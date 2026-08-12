#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/hash.h"
#include "ruda/base/map.h"

static void ruda_expect_sha256(const void *bytes, size_t length, const char *expected) {
    unsigned char digest[32];
    char hex[65];
    RUDA_EXPECT(ruda_sha256(bytes, length, digest));
    ruda_sha256_hex(digest, hex);
    RUDA_EXPECT(strcmp(hex, expected) == 0);
}

static void ruda_test_hashes(void) {
    static const unsigned char sip_input[16] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    };
    static const uint64_t sip_key0 = 0x0706050403020100ULL;
    static const uint64_t sip_key1 = 0x0f0e0d0c0b0a0908ULL;
    unsigned char digest[32];
    char hex[65];
    unsigned char a_chunk[1000];
    RudaSha256 hash;
    uint64_t old_count = 0;
    size_t index = 0;

    ruda_expect_sha256(
        NULL,
        0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    ruda_expect_sha256(
        "abc",
        3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    memset(a_chunk, 'a', sizeof(a_chunk));
    ruda_sha256_init(&hash);
    for (index = 0; index < 1000; index += 1) {
        RUDA_EXPECT(ruda_sha256_update(&hash, a_chunk, sizeof(a_chunk)));
    }
    ruda_sha256_final(&hash, digest);
    ruda_sha256_hex(digest, hex);
    RUDA_EXPECT(strcmp(
        hex,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"
    ) == 0);

    ruda_sha256_init(&hash);
    hash.bit_count = UINT64_MAX - 7U;
    old_count = hash.bit_count;
    RUDA_EXPECT(!ruda_sha256_update(&hash, "x", 1));
    RUDA_EXPECT(hash.bit_count == old_count);

    RUDA_EXPECT(ruda_hash_sip24(NULL, 0, sip_key0, sip_key1)
        == 0x726fdb47dd0e0e31ULL);
    RUDA_EXPECT(ruda_hash_sip24(sip_input, 1, sip_key0, sip_key1)
        == 0x74f839c593dc67fdULL);
    RUDA_EXPECT(ruda_hash_sip24(sip_input, 7, sip_key0, sip_key1)
        == 0xab0200f58b01d137ULL);
    RUDA_EXPECT(ruda_hash_sip24(sip_input, 15, sip_key0, sip_key1)
        == 0xa129ca6149be45e5ULL);
}

static void ruda_test_map_behavior(void) {
    RudaMap map;
    size_t index = 0;
    static const unsigned char binary_key[] = {'a', 0, 'b'};
    int binary_value = 700;
    RUDA_EXPECT(ruda_map_init(
        &map,
        ruda_system_allocator(),
        sizeof(int),
        0x0123456789abcdefULL,
        0xfedcba9876543210ULL
    ));
    for (index = 0; index < 500; index += 1) {
        char key[32];
        int value = (int)(index * 3);
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        RUDA_EXPECT(length > 0 && (size_t)length < sizeof(key));
        if (length > 0 && (size_t)length < sizeof(key)) {
            RUDA_EXPECT(ruda_map_put(&map, key, (size_t)length, &value));
        }
    }
    RUDA_EXPECT(map.length == 500);
    for (index = 0; index < 500; index += 1) {
        char key[32];
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        const int *value = ruda_map_get_const(&map, key, (size_t)length);
        RUDA_EXPECT(value != NULL);
        if (value != NULL) {
            RUDA_EXPECT(*value == (int)(index * 3));
        }
    }
    for (index = 0; index < 500; index += 2) {
        char key[32];
        int length = snprintf(key, sizeof(key), "common-prefix-%04zu", index);
        RUDA_EXPECT(ruda_map_remove(&map, key, (size_t)length));
        RUDA_EXPECT(!ruda_map_remove(&map, key, (size_t)length));
    }
    RUDA_EXPECT(map.length == 250);
    RUDA_EXPECT(ruda_map_put(&map, binary_key, sizeof(binary_key), &binary_value));
    RUDA_EXPECT(*(const int *)ruda_map_get_const(&map, binary_key, sizeof(binary_key)) == 700);
    binary_value = 701;
    RUDA_EXPECT(ruda_map_put(&map, binary_key, sizeof(binary_key), &binary_value));
    RUDA_EXPECT(*(int *)ruda_map_get(&map, binary_key, sizeof(binary_key)) == 701);
    RUDA_EXPECT((uintptr_t)ruda_map_get(&map, binary_key, sizeof(binary_key)) % _Alignof(int) == 0);
    ruda_map_dispose(&map);
}

static void ruda_test_map_failures(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMap map;
        int value = 42;
        bool inserted = false;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_map_init(&map, allocator, sizeof(value), 1, 2));
        inserted = ruda_map_put(&map, "key", 3, &value);
        if (inserted) {
            reached_success = true;
            RUDA_EXPECT(map.length == 1);
        } else {
            RUDA_EXPECT(map.length == 0);
            RUDA_EXPECT(ruda_map_get(&map, "key", 3) == NULL);
        }
        ruda_map_dispose(&map);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMap map;
        size_t index = 0;
        bool inserted = false;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_map_init(&map, allocator, sizeof(int), 3, 4));
        for (index = 0; index < 11; index += 1) {
            char key[16];
            int value = (int)index;
            int length = snprintf(key, sizeof(key), "key-%zu", index);
            RUDA_EXPECT(length > 0 && (size_t)length < sizeof(key));
            RUDA_EXPECT(ruda_map_put(&map, key, (size_t)length, &value));
        }
        state.failure_index = state.attempts + failure_index;
        {
            int value = 99;
            inserted = ruda_map_put(&map, "growth-key", strlen("growth-key"), &value);
        }
        if (inserted) {
            reached_success = true;
            RUDA_EXPECT(map.length == 12);
        } else {
            RUDA_EXPECT(map.length == 11);
            RUDA_EXPECT(ruda_map_get(&map, "growth-key", strlen("growth-key")) == NULL);
        }
        {
            size_t attempts = state.attempts;
            int update = 1234;
            state.failure_index = attempts;
            RUDA_EXPECT(ruda_map_put(&map, "key-0", strlen("key-0"), &update));
            RUDA_EXPECT(state.attempts == attempts);
            RUDA_EXPECT(*(int *)ruda_map_get(&map, "key-0", strlen("key-0")) == update);
        }
        ruda_map_dispose(&map);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_hash_map(void) {
    ruda_test_hashes();
    ruda_test_map_behavior();
    ruda_test_map_failures();
}
