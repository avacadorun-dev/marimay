#ifndef MMAY_RUNTIME_GENERATED_MAP_H
#define MMAY_RUNTIME_GENERATED_MAP_H

#include "mmay/runtime/map.h"

typedef struct MarimayManagedMapEntry {
    uint64_t hash;
    MarimayManagedCell key;
    MarimayManagedCell value;
    bool active;
} MarimayManagedMapEntry;

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
} MarimayManagedMap;

#endif
