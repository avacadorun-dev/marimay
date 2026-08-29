#ifndef MMAY_RUNTIME_MAP_H
#define MMAY_RUNTIME_MAP_H

#include "mmay/runtime/heap.h"

/*
 * Exact Map<K,V> identity is the descriptor address. Keys use the closed
 * D079 equality/hash set; values may use any valid managed field layout.
 * Descriptors are immutable and outlive every map created from them.
 */
typedef struct MarimayManagedMapDescriptor {
    MarimayManagedFieldDescriptor key;
    MarimayManagedFieldDescriptor value;
    bool is_set;
} MarimayManagedMapDescriptor;

MarimayManagedStatus mmay_managed_map_new(
    MarimayManagedHeap *heap,
    const MarimayManagedMapDescriptor *descriptor,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_map_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    size_t *length
);
MarimayManagedStatus mmay_managed_map_contains(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    MarimayManagedCell key,
    bool *contains
);
MarimayManagedStatus mmay_managed_map_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    MarimayManagedCell key,
    MarimayManagedCell *value,
    bool *found
);
/* inserted is false for an update; every successful put invalidates iteration. */
MarimayManagedStatus mmay_managed_map_put(
    MarimayManagedHeap *heap,
    MarimayManagedReference map,
    MarimayManagedCell key,
    MarimayManagedCell value,
    bool *inserted
);
MarimayManagedStatus mmay_managed_map_remove(
    MarimayManagedHeap *heap,
    MarimayManagedReference map,
    MarimayManagedCell key,
    MarimayManagedCell *value,
    bool *removed
);
MarimayManagedStatus mmay_managed_map_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference map
);
MarimayManagedStatus mmay_managed_map_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    uint64_t *generation
);
MarimayManagedStatus mmay_managed_map_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    uint64_t expected_generation
);
/* Active insertion-order ordinal; removed entries are skipped. */
MarimayManagedStatus mmay_managed_map_entry(
    const MarimayManagedHeap *heap,
    MarimayManagedReference map,
    size_t ordinal,
    MarimayManagedCell *key,
    MarimayManagedCell *value
);

#endif
