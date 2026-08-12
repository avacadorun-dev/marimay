#include "ruda/base/map.h"

#include <stddef.h>
#include <string.h>

#include "ruda/base/hash.h"
#include "ruda/base/owned_bytes.h"

typedef struct RudaMapEntry {
    RudaAllocator allocator;
    size_t allocation_size;
    uint64_t hash;
    RudaOwnedBytes key;
    max_align_t alignment_anchor;
    unsigned char value[];
} RudaMapEntry;

struct RudaMapSlot {
    RudaMapEntry *entry;
    bool tombstone;
};

static bool ruda_map_key_equal(
    const RudaMapEntry *entry,
    uint64_t hash,
    const void *key,
    size_t key_length
) {
    return entry->hash == hash && entry->key.length == key_length
        && (key_length == 0 || memcmp(entry->key.data, key, key_length) == 0);
}

static RudaMapEntry *ruda_map_entry_create(
    RudaMap *map,
    uint64_t hash,
    const void *key,
    size_t key_length,
    const void *value
) {
    size_t allocation_size = 0;
    RudaMapEntry *entry = NULL;
    if (!ruda_size_add(offsetof(RudaMapEntry, value), map->value_size, &allocation_size)) {
        return NULL;
    }
    entry = ruda_allocate(&map->allocator, allocation_size);
    if (entry == NULL) {
        return NULL;
    }
    entry->allocator = map->allocator;
    entry->allocation_size = allocation_size;
    entry->hash = hash;
    entry->key.data = NULL;
    entry->key.length = 0;
    if (!ruda_owned_bytes_copy(&entry->key, map->allocator, key, key_length)) {
        ruda_deallocate(&map->allocator, entry, allocation_size);
        return NULL;
    }
    memcpy(entry->value, value, map->value_size);
    return entry;
}

static void ruda_map_entry_destroy(RudaMapEntry *entry) {
    RudaAllocator allocator;
    size_t allocation_size = 0;
    if (entry == NULL) {
        return;
    }
    allocator = entry->allocator;
    allocation_size = entry->allocation_size;
    ruda_owned_bytes_dispose(&entry->key);
    ruda_deallocate(&allocator, entry, allocation_size);
}

static size_t ruda_map_find_slot(
    const RudaMap *map,
    uint64_t hash,
    const void *key,
    size_t key_length,
    bool *found
) {
    size_t mask = map->capacity - 1;
    size_t index = (size_t)hash & mask;
    size_t first_tombstone = SIZE_MAX;
    for (;;) {
        const RudaMapSlot *slot = &map->slots[index];
        if (slot->entry == NULL) {
            if (!slot->tombstone) {
                *found = false;
                return first_tombstone == SIZE_MAX ? index : first_tombstone;
            }
            if (first_tombstone == SIZE_MAX) {
                first_tombstone = index;
            }
        } else if (ruda_map_key_equal(slot->entry, hash, key, key_length)) {
            *found = true;
            return index;
        }
        index = (index + 1) & mask;
    }
}

static bool ruda_map_rehash(RudaMap *map, size_t new_capacity) {
    size_t allocation_size = 0;
    RudaMapSlot *new_slots = NULL;
    size_t index = 0;
    if (new_capacity < 16 || (new_capacity & (new_capacity - 1)) != 0
        || !ruda_size_multiply(new_capacity, sizeof(RudaMapSlot), &allocation_size)) {
        return false;
    }
    new_slots = ruda_allocate(&map->allocator, allocation_size);
    if (new_slots == NULL) {
        return false;
    }
    memset(new_slots, 0, allocation_size);
    for (index = 0; index < map->capacity; index += 1) {
        RudaMapEntry *entry = map->slots[index].entry;
        if (entry != NULL) {
            size_t mask = new_capacity - 1;
            size_t target = (size_t)entry->hash & mask;
            while (new_slots[target].entry != NULL) {
                target = (target + 1) & mask;
            }
            new_slots[target].entry = entry;
        }
    }
    if (map->slots != NULL) {
        size_t old_size = 0;
        if (ruda_size_multiply(map->capacity, sizeof(RudaMapSlot), &old_size)) {
            ruda_deallocate(&map->allocator, map->slots, old_size);
        }
    }
    map->slots = new_slots;
    map->capacity = new_capacity;
    map->tombstones = 0;
    return true;
}

static bool ruda_map_ensure_insert_capacity(RudaMap *map) {
    size_t occupied = 0;
    size_t threshold = 0;
    size_t new_capacity = 0;
    if (map->capacity == 0) {
        return ruda_map_rehash(map, 16);
    }
    if (!ruda_size_add(map->length, map->tombstones, &occupied)
        || !ruda_size_add(occupied, 1, &occupied)) {
        return false;
    }
    threshold = map->capacity - map->capacity / 4;
    if (occupied < threshold) {
        return true;
    }
    if (map->length < map->capacity / 2) {
        new_capacity = map->capacity;
    } else if (!ruda_size_multiply(map->capacity, 2, &new_capacity)) {
        return false;
    }
    return ruda_map_rehash(map, new_capacity);
}

bool ruda_map_init(
    RudaMap *map,
    RudaAllocator allocator,
    size_t value_size,
    uint64_t hash_key0,
    uint64_t hash_key1
) {
    if (map == NULL || value_size == 0 || allocator.allocate == NULL
        || allocator.deallocate == NULL) {
        return false;
    }
    map->allocator = allocator;
    map->slots = NULL;
    map->capacity = 0;
    map->length = 0;
    map->tombstones = 0;
    map->value_size = value_size;
    map->hash_key0 = hash_key0;
    map->hash_key1 = hash_key1;
    return true;
}

void ruda_map_dispose(RudaMap *map) {
    size_t index = 0;
    size_t allocation_size = 0;
    if (map == NULL) {
        return;
    }
    for (index = 0; index < map->capacity; index += 1) {
        ruda_map_entry_destroy(map->slots[index].entry);
    }
    if (map->slots != NULL
        && ruda_size_multiply(map->capacity, sizeof(RudaMapSlot), &allocation_size)) {
        ruda_deallocate(&map->allocator, map->slots, allocation_size);
    }
    map->slots = NULL;
    map->capacity = 0;
    map->length = 0;
    map->tombstones = 0;
}

bool ruda_map_put(RudaMap *map, const void *key, size_t key_length, const void *value) {
    uint64_t hash = 0;
    bool found = false;
    size_t index = 0;
    RudaMapEntry *entry = NULL;
    if (map == NULL || value == NULL || (key == NULL && key_length != 0)) {
        return false;
    }
    hash = ruda_hash_sip24(key, key_length, map->hash_key0, map->hash_key1);
    if (map->capacity != 0) {
        index = ruda_map_find_slot(map, hash, key, key_length, &found);
        if (found) {
            memcpy(map->slots[index].entry->value, value, map->value_size);
            return true;
        }
    }
    entry = ruda_map_entry_create(map, hash, key, key_length, value);
    if (entry == NULL) {
        return false;
    }
    if (!ruda_map_ensure_insert_capacity(map)) {
        ruda_map_entry_destroy(entry);
        return false;
    }
    index = ruda_map_find_slot(map, hash, key, key_length, &found);
    if (found) {
        ruda_map_entry_destroy(entry);
        memcpy(map->slots[index].entry->value, value, map->value_size);
        return true;
    }
    if (map->slots[index].tombstone) {
        map->tombstones -= 1;
    }
    map->slots[index].entry = entry;
    map->slots[index].tombstone = false;
    map->length += 1;
    return true;
}

void *ruda_map_get(RudaMap *map, const void *key, size_t key_length) {
    uint64_t hash = 0;
    bool found = false;
    size_t index = 0;
    if (map == NULL || map->capacity == 0 || (key == NULL && key_length != 0)) {
        return NULL;
    }
    hash = ruda_hash_sip24(key, key_length, map->hash_key0, map->hash_key1);
    index = ruda_map_find_slot(map, hash, key, key_length, &found);
    return found ? map->slots[index].entry->value : NULL;
}

const void *ruda_map_get_const(const RudaMap *map, const void *key, size_t key_length) {
    uint64_t hash = 0;
    bool found = false;
    size_t index = 0;
    if (map == NULL || map->capacity == 0 || (key == NULL && key_length != 0)) {
        return NULL;
    }
    hash = ruda_hash_sip24(key, key_length, map->hash_key0, map->hash_key1);
    index = ruda_map_find_slot(map, hash, key, key_length, &found);
    return found ? map->slots[index].entry->value : NULL;
}

bool ruda_map_remove(RudaMap *map, const void *key, size_t key_length) {
    uint64_t hash = 0;
    bool found = false;
    size_t index = 0;
    if (map == NULL || map->capacity == 0 || (key == NULL && key_length != 0)) {
        return false;
    }
    hash = ruda_hash_sip24(key, key_length, map->hash_key0, map->hash_key1);
    index = ruda_map_find_slot(map, hash, key, key_length, &found);
    if (!found) {
        return false;
    }
    ruda_map_entry_destroy(map->slots[index].entry);
    map->slots[index].entry = NULL;
    map->slots[index].tombstone = true;
    map->length -= 1;
    map->tombstones += 1;
    return true;
}
