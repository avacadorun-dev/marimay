#ifndef MMAY_MIR_EXECUTION_COLLECTIONS_INTERNAL_H
#define MMAY_MIR_EXECUTION_COLLECTIONS_INTERNAL_H

#include "mmay/mir/execution.h"

enum {
    MMAY_MIR_REFERENCE_COLLECTION_KIND = 5
};

typedef struct MarimayMirManagedCollectionEntry {
    MarimayMirValue key;
    MarimayMirValue value;
    bool active;
} MarimayMirManagedCollectionEntry;

typedef struct MarimayMirManagedCollection {
    MarimayTypeId type, key_type, value_type;
    size_t allocation_size, length, entry_count, capacity;
    uint64_t mutation_generation;
    bool is_set;
    MarimayMirManagedCollectionEntry entries[];
} MarimayMirManagedCollection;

bool mmay_mir_reference_encode(
    const MarimayMirExecution *execution,
    unsigned int kind,
    size_t index,
    MarimayMirReference *result
);
bool mmay_mir_reference_decode(
    const MarimayMirExecution *execution,
    MarimayMirReference reference,
    unsigned int expected,
    size_t *index
);
MarimayMirInterpretStatus mmay_mir_execute_collection_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    const MarimayMirValue *third,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);

#endif
