#include "../test_support.h"

#include "../../../runtime/common/managed_map_probe_internal.h"
#include "../../../runtime/common/managed_map_storage_internal.h"
#include "../../../runtime/common/managed_map_index_internal.h"

/* Independently declared entry and control encodings; no production pack or
 * metadata-store helper constructs the scalar model. */
typedef struct Entry {
    MarimayManagedMapEntry prefix;
    uint64_t key;
    uint64_t value;
} Entry;

static void mmay_test_probe_walks(void) {
    Entry entries[64] = {0};
    MarimayManagedMap map = {0};
    MarimayFailAllocator tracked;
    MarimayAllocator allocator;
    uint64_t random = UINT64_C(0x20260907);
    size_t allocation = 64U * sizeof(size_t) + 71U, capacity, layout, start, query, i;
    size_t *buckets;
    mmay_fail_allocator_init(&tracked, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&tracked);
    buckets = mmay_allocate(&allocator, allocation);
    MMAY_EXPECT(buckets != NULL);
    if (buckets == NULL) {
        MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
        return;
    }
    map.buckets = buckets;
    map.entries = (unsigned char *)(void *)entries;
    map.entry_stride = sizeof(entries[0]);
    for (capacity = 8; capacity <= 64; capacity *= 2U) {
        unsigned char *controls = (unsigned char *)(void *)(buckets + capacity);
        map.bucket_capacity = capacity;
        map.entry_count = capacity;
        for (layout = 0; layout < 256; layout++) {
            for (start = 0; start < capacity; start++) {
                uint64_t hash = UINT64_MAX - (uint64_t)(capacity - 1U) + (uint64_t)start;
                for (i = 0; i < capacity; i++) {
                    unsigned int kind;
                    random = random * UINT64_C(6364136223846793005) + 1U;
                    kind = (unsigned int)(random >> 60U) % 6U;
                    buckets[i] = kind == 0 ? MMAY_MAP_EMPTY : kind == 1 ? MMAY_MAP_TOMBSTONE : i;
                    entries[i].prefix.hash = kind < 3 ? hash : kind == 3 ? hash ^ UINT64_C(64)
                        : kind == 4 ? hash ^ (UINT64_C(1) << 57U) : ~hash;
                    entries[i].prefix.active = true;
                    entries[i].key = (uint64_t)i;
                    controls[i] = kind == 0 ? 0x80U : kind == 1 ? 0xfeU
                        : (unsigned char)(entries[i].prefix.hash >> 57U);
                }
                for (i = 0; i < 7; i++) controls[capacity + i] = controls[i];
                for (query = 0; query <= capacity; query++) {
                    size_t ordinal, expected_bucket = MMAY_MAP_EMPTY, expected_entry = MMAY_MAP_EMPTY;
                    size_t bucket = 0, entry = 0;
                    bool found = false;
                    MarimayManagedStatus expected_status = MMAY_MANAGED_LIMIT_REACHED;
                    for (ordinal = 0; ordinal < capacity; ordinal++) {
                        size_t position = (start + ordinal) % capacity;
                        if (buckets[position] == MMAY_MAP_TOMBSTONE) {
                            if (expected_bucket == MMAY_MAP_EMPTY) expected_bucket = position;
                        } else if (buckets[position] == MMAY_MAP_EMPTY) {
                            if (expected_bucket == MMAY_MAP_EMPTY) expected_bucket = position;
                            break;
                        } else if (entries[position].prefix.hash == hash && position == query) {
                            expected_bucket = position;
                            expected_entry = position;
                            break;
                        }
                    }
                    if (expected_bucket != MMAY_MAP_EMPTY) expected_status = MMAY_MANAGED_OK;
                    MMAY_EXPECT(mmay_managed_map_probe(&map, hash, true, (uint64_t)query,
                        &bucket, &entry, &found, NULL) == expected_status);
                    MMAY_EXPECT(bucket == expected_bucket && entry == expected_entry);
                    MMAY_EXPECT(found == (expected_entry != MMAY_MAP_EMPTY));
                }
                {
                    MarimayManagedMapProbe progress = {0, MMAY_MAP_EMPTY};
                    size_t ordinal, tombstone = MMAY_MAP_EMPTY, bucket = 0, entry = 0;
                    bool found = false, terminated = false;
                    for (ordinal = 0; ordinal < capacity; ordinal++) {
                        size_t position = (start + ordinal) % capacity;
                        if (buckets[position] == MMAY_MAP_TOMBSTONE) {
                            if (tombstone == MMAY_MAP_EMPTY) tombstone = position;
                        } else if (buckets[position] == MMAY_MAP_EMPTY) {
                            if (tombstone == MMAY_MAP_EMPTY) tombstone = position;
                            terminated = true;
                            break;
                        } else if (entries[position].prefix.hash == hash) {
                            MMAY_EXPECT(mmay_managed_map_probe(&map, hash, false, 0,
                                &bucket, &entry, &found, &progress) == MMAY_MANAGED_OK);
                            MMAY_EXPECT(found && bucket == position && entry == position);
                            MMAY_EXPECT(progress.next_index == ordinal + 1U && progress.first_tombstone == tombstone);
                        }
                    }
                    MMAY_EXPECT(mmay_managed_map_probe(&map, hash, false, 0,
                        &bucket, &entry, &found, &progress) == (terminated || tombstone != MMAY_MAP_EMPTY
                            ? MMAY_MANAGED_OK : MMAY_MANAGED_LIMIT_REACHED));
                    MMAY_EXPECT(!found && entry == MMAY_MAP_EMPTY && bucket == tombstone);
                }
            }
        }
    }
    mmay_deallocate(&allocator, buckets, allocation);
    MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
}

static void mmay_test_probe_invalid_storage(void) {
    Entry item = {0};
    MarimayFailAllocator tracked;
    MarimayAllocator allocator;
    const size_t bytes = 8U * sizeof(size_t) + 15U;
    size_t *storage, bucket = 0, entry = 0, i;
    unsigned char *controls;
    bool found = false;
    MarimayManagedMap map = {0};
    mmay_fail_allocator_init(&tracked, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&tracked);
    storage = mmay_allocate(&allocator, bytes);
    MMAY_EXPECT(storage != NULL);
    if (storage == NULL) goto done;
    controls = (unsigned char *)(void *)(storage + 8U);
    for (i = 0; i < 8; i++) storage[i] = MMAY_MAP_EMPTY;
    for (i = 0; i < 15; i++) controls[i] = 0x80U;
    storage[7] = 0;
    controls[7] = 0x7fU;
    map.bucket_capacity = 8;
    map.entry_count = 1;
    map.entry_stride = sizeof(item);
    map.entries = (unsigned char *)(void *)&item;
    map.buckets = storage;
    item.prefix.active = true;
    item.prefix.hash = UINT64_MAX;
    item.key = UINT64_MAX;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_OK && found);
    item.prefix.active = false;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    item.prefix.active = true;
    storage[7] = 1;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    storage[7] = 0;
    map.entries = NULL;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    map.entries = (unsigned char *)(void *)&item;
    /* An inspected empty/deleted control must agree with its bucket marker. */
    controls[7] = 0x80U;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    controls[7] = 0xfeU;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_REFERENCE);
    controls[7] = 0x7fU;
    /* Rejected capacities are checked before any control-tail load. */
    for (i = 0; i <= 9; i++) {
        if (i == 8) continue;
        map.bucket_capacity = i;
        MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
            &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    }
    map.bucket_capacity = 8;
    map.buckets = NULL;
    MMAY_EXPECT(mmay_managed_map_probe(&map, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_map_probe(NULL, UINT64_MAX, true, UINT64_MAX,
        &bucket, &entry, &found, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
done:
    mmay_deallocate(&allocator, storage, bytes);
    MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
}

static void mmay_test_probe_index_sizes(void) {
    size_t bytes = SIZE_MAX;
    MMAY_EXPECT(mmay_map_index_bytes(0, &bytes) && bytes == 0);
    MMAY_EXPECT(mmay_map_index_bytes(8, &bytes)
        && bytes == 8U * sizeof(size_t) + 15U);
    MMAY_EXPECT(!mmay_map_index_bytes(SIZE_MAX, &bytes));
    MMAY_EXPECT(!mmay_map_index_bytes(SIZE_MAX / sizeof(size_t), &bytes));
    MMAY_EXPECT(!mmay_map_index_bytes(SIZE_MAX / (sizeof(size_t) + 1U), &bytes));
}

void mmay_test_managed_map_probe(void) {
    mmay_test_probe_walks();
    mmay_test_probe_invalid_storage();
    mmay_test_probe_index_sizes();
}
