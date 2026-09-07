#ifndef MMAY_RUNTIME_GENERATED_MAP_H
#define MMAY_RUNTIME_GENERATED_MAP_H

#include "mmay/runtime/map.h"

typedef struct MarimayManagedMapEntry {
    uint64_t hash;
    MarimayManagedCell key;
    MarimayManagedCell value;
    bool active;
} MarimayManagedMapEntry;

/* Last successful scalar-key get, including absence. Indices, not pointers or
 * references: invalidated by mutation generation or any storage rebuild. */
typedef struct MarimayManagedMapLookup {
    uint64_t key_bits;
    uint64_t hash;
    uint64_t generation;
    uint64_t seed0;
    uint64_t seed1;
    size_t bucket;
    size_t entry;
    bool found;
    bool valid;
} MarimayManagedMapLookup;

/* Exact non-moving map/set object layout reserved for compiler-generated C. */
typedef struct MarimayManagedMap {
    const MarimayManagedMapDescriptor *descriptor;
    size_t length;
    size_t entry_count;
    size_t entry_capacity;
    size_t bucket_capacity;
    uint64_t mutation_generation;
    MarimayManagedMapEntry *entries;
    size_t *buckets;
    MarimayManagedMapLookup lookup;
} MarimayManagedMap;

#endif
