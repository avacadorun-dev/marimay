#ifndef RUDA_BASE_MAP_H
#define RUDA_BASE_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"

typedef struct RudaMapSlot RudaMapSlot;

typedef struct RudaMap {
    RudaAllocator allocator;
    RudaMapSlot *slots;
    size_t capacity;
    size_t length;
    size_t tombstones;
    size_t value_size;
    uint64_t hash_key0;
    uint64_t hash_key1;
} RudaMap;

/* Keys and fixed-size values are copied. Hash seeds are caller/session owned. */
bool ruda_map_init(
    RudaMap *map,
    RudaAllocator allocator,
    size_t value_size,
    uint64_t hash_key0,
    uint64_t hash_key1
);
void ruda_map_dispose(RudaMap *map);

/* Insertion is failure-atomic; updating an existing value does not allocate. */
bool ruda_map_put(RudaMap *map, const void *key, size_t key_length, const void *value);
void *ruda_map_get(RudaMap *map, const void *key, size_t key_length);
const void *ruda_map_get_const(const RudaMap *map, const void *key, size_t key_length);
bool ruda_map_remove(RudaMap *map, const void *key, size_t key_length);

#endif
