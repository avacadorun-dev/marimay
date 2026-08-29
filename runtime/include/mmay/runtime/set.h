#ifndef MMAY_RUNTIME_SET_H
#define MMAY_RUNTIME_SET_H

#include "mmay/runtime/map.h"

MarimayManagedStatus mmay_managed_set_new(
    MarimayManagedHeap *heap,
    const MarimayManagedMapDescriptor *descriptor,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_set_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    size_t *length
);
MarimayManagedStatus mmay_managed_set_contains(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *contains
);
/* inserted is false for a duplicate, which does not invalidate iteration. */
MarimayManagedStatus mmay_managed_set_add(
    MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *inserted
);
MarimayManagedStatus mmay_managed_set_remove(
    MarimayManagedHeap *heap,
    MarimayManagedReference set,
    MarimayManagedCell value,
    bool *removed
);
MarimayManagedStatus mmay_managed_set_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference set
);
MarimayManagedStatus mmay_managed_set_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    uint64_t *generation
);
MarimayManagedStatus mmay_managed_set_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    uint64_t expected_generation
);
MarimayManagedStatus mmay_managed_set_entry(
    const MarimayManagedHeap *heap,
    MarimayManagedReference set,
    size_t ordinal,
    MarimayManagedCell *value
);

#endif
