#include "vm_internal.h"

#include <string.h>

#include "program_internal.h"

typedef struct RudaBytecodeArrayDescriptorEntry {
    const RudaBytecodeFunction *function;
    uint32_t layout;
    RudaManagedArrayDescriptor *descriptor;
    bool building;
    bool complete;
} RudaBytecodeArrayDescriptorEntry;

bool ruda_bytecode_array_cache_init(
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    return execution != NULL && ruda_vector_init(
        &execution->array_descriptors,
        allocator,
        sizeof(RudaBytecodeArrayDescriptorEntry)
    );
}

void ruda_bytecode_array_cache_rollback(
    RudaBytecodeExecution *execution,
    size_t length
) {
    if (execution == NULL) {
        return;
    }
    while (execution->array_descriptors.length > length) {
        size_t index = execution->array_descriptors.length - 1;
        RudaBytecodeArrayDescriptorEntry *entry = ruda_vector_at(
            &execution->array_descriptors, index
        );
        if (entry != NULL) {
            ruda_deallocate(
                &execution->allocator,
                entry->descriptor,
                entry->descriptor == NULL ? 0 : sizeof(*entry->descriptor)
            );
            memset(entry, 0, sizeof(*entry));
        }
        execution->array_descriptors.length = index;
    }
}

void ruda_bytecode_array_cache_dispose(RudaBytecodeExecution *execution) {
    if (execution == NULL) {
        return;
    }
    ruda_bytecode_array_cache_rollback(execution, 0);
    ruda_vector_dispose(&execution->array_descriptors);
}

const RudaManagedArrayDescriptor *ruda_bytecode_array_descriptor(
    RudaBytecodeType type
) {
    if (type == RUDA_BYTECODE_TYPE_ARRAY_F64) {
        return &ruda_managed_array_f64_descriptor;
    }
    if (type == RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64) {
        return &ruda_managed_array_array_f64_descriptor;
    }
    return NULL;
}

bool ruda_bytecode_managed_value_valid(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue value
) {
    const RudaManagedArrayDescriptor *actual = NULL;
    const RudaManagedArrayDescriptor *expected =
        ruda_bytecode_array_descriptor(value.type);
    return execution != NULL && execution->initialized && expected != NULL
        && ruda_managed_array_describe(
            &execution->managed_heap, value.as.managed_reference, &actual
        ) == RUDA_MANAGED_OK && actual == expected;
}

static RudaBytecodeArrayDescriptorEntry *ruda_bytecode_find_array_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    uint32_t layout
) {
    size_t index = 0;
    for (index = 0; index < execution->array_descriptors.length; index += 1) {
        RudaBytecodeArrayDescriptorEntry *entry = ruda_vector_at(
            &execution->array_descriptors, index
        );
        if (entry != NULL && ruda_bytecode_program_same_type(
                entry->function,
                RUDA_BYTECODE_TYPE_ARRAY,
                entry->layout,
                function,
                RUDA_BYTECODE_TYPE_ARRAY,
                layout
            )) {
            return entry;
        }
    }
    return NULL;
}

static bool ruda_bytecode_build_array_element_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    const RudaBytecodeArrayType *array_type,
    RudaManagedArrayDescriptor *descriptor
) {
    RudaBytecodeType type = array_type->element_type;
    uint32_t layout = array_type->element_layout;
    if (type == RUDA_BYTECODE_TYPE_STRING) {
        descriptor->cell_kind = RUDA_MANAGED_CELL_REFERENCE;
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_CELL;
        descriptor->storage_stride = sizeof(RudaManagedCell);
        descriptor->reference_kind = RUDA_MANAGED_OBJECT_STRING;
        return true;
    }
    if (ruda_bytecode_type_is_array(type)
        || type == RUDA_BYTECODE_TYPE_RECORD
        || type == RUDA_BYTECODE_TYPE_ENUM
        || type == RUDA_BYTECODE_TYPE_INTERFACE) {
        descriptor->cell_kind = RUDA_MANAGED_CELL_REFERENCE;
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_CELL;
        descriptor->storage_stride = sizeof(RudaManagedCell);
        descriptor->reference_kind = ruda_bytecode_nominal_object_kind(type);
        descriptor->reference_descriptor =
            ruda_bytecode_nominal_reference_descriptor(
                execution, function, type, layout
            );
        return descriptor->reference_kind != RUDA_MANAGED_OBJECT_INVALID
            && descriptor->reference_descriptor != NULL;
    }
    descriptor->cell_kind = RUDA_MANAGED_CELL_RAW64;
    if (type == RUDA_BYTECODE_TYPE_BOOL || type == RUDA_BYTECODE_TYPE_I8
        || type == RUDA_BYTECODE_TYPE_U8) {
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_RAW8;
        descriptor->storage_stride = sizeof(uint8_t);
    } else if (type == RUDA_BYTECODE_TYPE_I16
        || type == RUDA_BYTECODE_TYPE_U16) {
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_RAW16;
        descriptor->storage_stride = sizeof(uint16_t);
    } else if (type == RUDA_BYTECODE_TYPE_I32
        || type == RUDA_BYTECODE_TYPE_U32
        || type == RUDA_BYTECODE_TYPE_F32) {
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_RAW32;
        descriptor->storage_stride = sizeof(uint32_t);
    } else if (type == RUDA_BYTECODE_TYPE_I64
        || type == RUDA_BYTECODE_TYPE_U64
        || type == RUDA_BYTECODE_TYPE_F64) {
        descriptor->storage_kind = RUDA_MANAGED_ARRAY_STORAGE_RAW64;
        descriptor->storage_stride = sizeof(uint64_t);
    } else {
        return false;
    }
    descriptor->reference_kind = RUDA_MANAGED_OBJECT_INVALID;
    return true;
}

const RudaManagedArrayDescriptor *ruda_bytecode_array_descriptor_for(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t layout
) {
    const RudaBytecodeArrayType *array_type = NULL;
    RudaBytecodeArrayDescriptorEntry *existing = NULL;
    RudaBytecodeArrayDescriptorEntry entry;
    RudaManagedArrayDescriptor *descriptor = NULL;
    size_t cache_start = 0;
    size_t entry_index = 0;
    if (type != RUDA_BYTECODE_TYPE_ARRAY) {
        return ruda_bytecode_array_descriptor(type);
    }
    if (execution == NULL || !execution->initialized || function == NULL) {
        return NULL;
    }
    array_type = ruda_bytecode_array_type(function, layout);
    if (array_type == NULL) {
        return NULL;
    }
    existing = ruda_bytecode_find_array_descriptor(
        execution, function, layout
    );
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor : NULL;
    }
    cache_start = execution->array_descriptors.length;
    descriptor = ruda_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        execution->array_descriptor_allocation_failed = true;
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    memset(&entry, 0, sizeof(entry));
    entry.function = function;
    entry.layout = layout;
    entry.descriptor = descriptor;
    entry.building = true;
    if (!ruda_vector_push(&execution->array_descriptors, &entry)) {
        execution->array_descriptor_allocation_failed = true;
        ruda_deallocate(&execution->allocator, descriptor, sizeof(*descriptor));
        return NULL;
    }
    entry_index = execution->array_descriptors.length - 1;
    if (!ruda_bytecode_build_array_element_descriptor(
            execution, function, array_type, descriptor
        )) {
        execution->array_descriptor_allocation_failed = true;
        ruda_bytecode_array_cache_rollback(execution, cache_start);
        return NULL;
    }
    existing = ruda_vector_at(&execution->array_descriptors, entry_index);
    if (existing == NULL) {
        execution->array_descriptor_allocation_failed = true;
        ruda_bytecode_array_cache_rollback(execution, cache_start);
        return NULL;
    }
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

bool ruda_bytecode_managed_value_valid_for(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue value,
    uint32_t layout
) {
    const RudaManagedArrayDescriptor *actual = NULL;
    const RudaManagedArrayDescriptor *expected =
        ruda_bytecode_array_descriptor_for(
            execution, function, value.type, layout
        );
    return execution != NULL && execution->initialized && expected != NULL
        && ruda_managed_array_describe(
            &execution->managed_heap, value.as.managed_reference, &actual
        ) == RUDA_MANAGED_OK && actual == expected;
}
