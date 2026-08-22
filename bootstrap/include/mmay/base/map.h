#ifndef MMAY_BASE_MAP_H
#define MMAY_BASE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"

typedef struct MarimayMapSlot MarimayMapSlot;

typedef struct MarimayMap {
    MarimayAllocator allocator;
    MarimayMapSlot *slots;
    size_t capacity;
    size_t length;
    size_t tombstones;
    size_t value_size;
    uint64_t hash_key0;
    uint64_t hash_key1;
} MarimayMap;

/* Keys and fixed-size values are copied. Hash seeds are caller/session owned. */
bool mmay_map_init(
    MarimayMap *map,
    MarimayAllocator allocator,
    size_t value_size,
    uint64_t hash_key0,
    uint64_t hash_key1
);
void mmay_map_dispose(MarimayMap *map);

/* Insertion is failure-atomic; updating an existing value does not allocate. */
bool mmay_map_put(MarimayMap *map, const void *key, size_t key_length, const void *value);
void *mmay_map_get(MarimayMap *map, const void *key, size_t key_length);
const void *mmay_map_get_const(const MarimayMap *map, const void *key, size_t key_length);
bool mmay_map_remove(MarimayMap *map, const void *key, size_t key_length);

#endif
