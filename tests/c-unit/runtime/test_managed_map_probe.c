#include "../test_support.h"

#include "../../../runtime/common/managed_map_probe_internal.h"

static void mmay_test_probe_walks(void) {
    MarimayManagedMapEntry entries[8] = {0};
    size_t buckets[8];
    MarimayManagedMap map = {0};
    uint64_t random = UINT64_C(0x20260907);
    size_t layout, start, query, position;
    map.bucket_capacity = 8;
    map.entry_count = 8;
    map.entries = entries;
    map.buckets = buckets;
    /* Every bucket is either empty, a tombstone, a matching-hash entry or a
     * different-hash entry. Vary the start to cover wraparound independently
     * of hash distribution; exact-key collisions are deliberate, not guessed. */
    for (layout = 0; layout < 4096; layout += 1) {
        random = random * UINT64_C(6364136223846793005) + 1;
        for (start = 0; start < 8; start += 1) {
            const uint64_t hash = UINT64_MAX - 7U + (uint64_t)start;
            for (position = 0; position < 8; position += 1) {
                const unsigned int kind = (unsigned int)((random >> (position * 2U)) & 3U);
                buckets[position] = kind == 0 ? MMAY_MAP_EMPTY
                    : kind == 1 ? MMAY_MAP_TOMBSTONE : position;
                entries[position].hash = kind == 2 ? hash : hash ^ UINT64_C(8);
                entries[position].key.f64_bits = (uint64_t)position;
                entries[position].active = true;
            }
            for (query = 0; query <= 8; query += 1) {
                size_t ordinal, expected_bucket = MMAY_MAP_EMPTY;
                size_t expected_entry = MMAY_MAP_EMPTY;
                size_t bucket = 0, entry = 0;
                bool found = false;
                MarimayManagedStatus expected_status = MMAY_MANAGED_LIMIT_REACHED;
                for (ordinal = 0; ordinal < 8; ordinal += 1) {
                    position = (start + ordinal) % 8;
                    if (buckets[position] == MMAY_MAP_TOMBSTONE) {
                        if (expected_bucket == MMAY_MAP_EMPTY) expected_bucket = position;
                    } else if (buckets[position] == MMAY_MAP_EMPTY) {
                        if (expected_bucket == MMAY_MAP_EMPTY) expected_bucket = position;
                        break;
                    } else if (entries[position].hash == hash && position == query) {
                        expected_bucket = position;
                        expected_entry = position;
                        break;
                    }
                }
                if (expected_bucket != MMAY_MAP_EMPTY) expected_status = MMAY_MANAGED_OK;
                MMAY_EXPECT(mmay_managed_map_probe(
                    &map, hash, true, (uint64_t)query, &bucket, &entry, &found, NULL
                ) == expected_status);
                MMAY_EXPECT(bucket == expected_bucket && entry == expected_entry);
                MMAY_EXPECT(found == (expected_entry != MMAY_MAP_EMPTY));
            }
            {
                MarimayManagedMapProbe progress = {0, MMAY_MAP_EMPTY};
                size_t ordinal, tombstone = MMAY_MAP_EMPTY;
                size_t bucket = 0, entry = 0;
                bool found = false, terminated = false;
                for (ordinal = 0; ordinal < 8; ordinal += 1) {
                    position = (start + ordinal) % 8;
                    if (buckets[position] == MMAY_MAP_TOMBSTONE) {
                        if (tombstone == MMAY_MAP_EMPTY) tombstone = position;
                    } else if (buckets[position] == MMAY_MAP_EMPTY) {
                        if (tombstone == MMAY_MAP_EMPTY) tombstone = position;
                        terminated = true;
                        break;
                    } else if (entries[position].hash == hash) {
                        MMAY_EXPECT(mmay_managed_map_probe(
                            &map, hash, false, 0, &bucket, &entry, &found, &progress
                        ) == MMAY_MANAGED_OK);
                        MMAY_EXPECT(found && bucket == position && entry == position);
                        MMAY_EXPECT(progress.next_index == ordinal + 1U);
                    }
                }
                MMAY_EXPECT(mmay_managed_map_probe(
                    &map, hash, false, 0, &bucket, &entry, &found, &progress
                ) == (terminated || tombstone != MMAY_MAP_EMPTY
                    ? MMAY_MANAGED_OK : MMAY_MANAGED_LIMIT_REACHED));
                MMAY_EXPECT(!found && entry == MMAY_MAP_EMPTY && bucket == tombstone);
            }
        }
    }
}

static void mmay_test_probe_invalid_storage(void) {
    MarimayManagedMapEntry item = {0};
    size_t storage = 0, bucket = 0, entry = 0;
    bool found = false;
    MarimayManagedMap map = {0};
    map.bucket_capacity = 1;
    map.entry_count = 1;
    map.entries = &item;
    map.buckets = &storage;
    item.active = true;
    item.hash = UINT64_MAX;
    item.key.f64_bits = UINT64_MAX;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_OK && found);
    item.active = false;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    item.active = true;
    storage = 1;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    storage = 0;
    map.entries = NULL;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    map.entries = &item;
    map.bucket_capacity = 0;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    map.bucket_capacity = 1;
    map.buckets = NULL;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    map.buckets = &storage;
    MMAY_EXPECT(mmay_managed_map_probe(NULL, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
}

void mmay_test_managed_map_probe(void) {
    mmay_test_probe_walks();
    mmay_test_probe_invalid_storage();
}
