#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "program_internal.h"

typedef struct MarimayBytecodeCollectionDescriptorEntry {
    const MarimayBytecodeFunction *function;
    uint32_t layout;
    MarimayManagedMapDescriptor *descriptor;
    bool building;
    bool complete;
} MarimayBytecodeCollectionDescriptorEntry;

bool mmay_bytecode_collection_cache_init(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    return execution != NULL && mmay_vector_init(
        &execution->collection_descriptors,
        allocator,
        sizeof(MarimayBytecodeCollectionDescriptorEntry)
    );
}

void mmay_bytecode_collection_cache_dispose(
    MarimayBytecodeExecution *execution
) {
    size_t index = 0;
    if (execution == NULL) return;
    for (index = 0; index < execution->collection_descriptors.length;
        index += 1) {
        MarimayBytecodeCollectionDescriptorEntry *entry = mmay_vector_at(
            &execution->collection_descriptors, index
        );
        if (entry != NULL) {
            mmay_deallocate(
                &execution->allocator,
                entry->descriptor,
                entry->descriptor == NULL ? 0 : sizeof(*entry->descriptor)
            );
        }
    }
    mmay_vector_dispose(&execution->collection_descriptors);
}

static MarimayBytecodeCollectionDescriptorEntry *
mmay_bytecode_find_collection_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t layout
) {
    size_t index = 0;
    MarimayBytecodeType type = MMAY_BYTECODE_TYPE_INVALID;
    const MarimayBytecodeCollectionType *collection =
        mmay_bytecode_collection_type(function, layout);
    if (collection == NULL) return NULL;
    type = collection->is_set
        ? MMAY_BYTECODE_TYPE_SET : MMAY_BYTECODE_TYPE_MAP;
    for (index = 0; index < execution->collection_descriptors.length;
        index += 1) {
        MarimayBytecodeCollectionDescriptorEntry *entry = mmay_vector_at(
            &execution->collection_descriptors, index
        );
        if (entry != NULL && mmay_bytecode_program_same_type(
                entry->function,
                type,
                entry->layout,
                function,
                type,
                layout
            )) return entry;
    }
    return NULL;
}

const MarimayManagedMapDescriptor *mmay_bytecode_collection_descriptor_for(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t layout
) {
    const MarimayBytecodeCollectionType *collection =
        mmay_bytecode_collection_type(function, layout);
    MarimayBytecodeCollectionDescriptorEntry *existing = NULL;
    MarimayBytecodeCollectionDescriptorEntry entry;
    MarimayManagedMapDescriptor *descriptor = NULL;
    if (execution == NULL || !execution->initialized || collection == NULL) {
        return NULL;
    }
    existing = mmay_bytecode_find_collection_descriptor(
        execution, function, layout
    );
    if (existing != NULL) {
        return existing->building || existing->complete
            ? existing->descriptor : NULL;
    }
    descriptor = mmay_allocate(&execution->allocator, sizeof(*descriptor));
    if (descriptor == NULL) {
        execution->collection_descriptor_allocation_failed = true;
        return NULL;
    }
    memset(descriptor, 0, sizeof(*descriptor));
    memset(&entry, 0, sizeof(entry));
    entry.function = function;
    entry.layout = layout;
    entry.descriptor = descriptor;
    entry.building = true;
    if (!mmay_vector_push(&execution->collection_descriptors, &entry)) {
        execution->collection_descriptor_allocation_failed = true;
        mmay_deallocate(
            &execution->allocator, descriptor, sizeof(*descriptor)
        );
        return NULL;
    }
    if (!mmay_bytecode_managed_field_descriptor(
            execution, function, collection->key_type,
            collection->key_layout, false, &descriptor->key
        ) || (!collection->is_set && !mmay_bytecode_managed_field_descriptor(
            execution, function, collection->value_type,
            collection->value_layout, false, &descriptor->value
        ))) {
        execution->collection_descriptor_allocation_failed = true;
        execution->collection_descriptors.length -= 1;
        mmay_deallocate(
            &execution->allocator, descriptor, sizeof(*descriptor)
        );
        return NULL;
    }
    descriptor->is_set = collection->is_set;
    existing = mmay_vector_at(
        &execution->collection_descriptors,
        execution->collection_descriptors.length - 1
    );
    if (existing == NULL) return NULL;
    existing->building = false;
    existing->complete = true;
    return descriptor;
}

static MarimayBytecodeRunStatus mmay_collection_panic(
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span,
    bool is_set
) {
    static const unsigned char map_message[] =
        "map iterator invalidated by mutation";
    static const unsigned char set_message[] =
        "set iterator invalidated by mutation";
    const unsigned char *message = is_set ? set_message : map_message;
    size_t length = is_set
        ? sizeof(set_message) - 1 : sizeof(map_message) - 1;
    MarimayBytecodePanicFrame frame;
    if (panic == NULL) return MMAY_BYTECODE_RUN_PANIC;
    mmay_bytecode_run_reset_panic(panic);
    if (!mmay_owned_bytes_copy(&panic->message, allocator, message, length)) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4005";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

static MarimayBytecodeRunStatus mmay_collection_status(
    MarimayManagedStatus status,
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span,
    bool is_set
) {
    return status == MMAY_MANAGED_ITERATOR_INVALIDATED
        ? mmay_collection_panic(
            panic, allocator, symbol, span, is_set
        ) : mmay_bytecode_run_managed_status(status);
}

static MarimayBytecodeRunStatus mmay_collection_cells(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeValue *key,
    const MarimayBytecodeValue *value,
    MarimayManagedCell *key_cell,
    MarimayManagedCell *value_cell,
    MarimayManagedRootFrame *roots
) {
    MarimayManagedReference *locations[2] = {
        &key_cell->reference, &value_cell->reference
    };
    MarimayManagedStatus root_status;
    MarimayBytecodeRunStatus status;
    memset(key_cell, 0, sizeof(*key_cell));
    memset(value_cell, 0, sizeof(*value_cell));
    key_cell->reference = mmay_managed_invalid_reference();
    value_cell->reference = mmay_managed_invalid_reference();
    root_status = mmay_managed_root_push(
        &execution->managed_heap, roots, locations, 2
    );
    if (root_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(root_status);
    }
    status = mmay_bytecode_value_to_managed_cell(
        execution, key, key_cell
    );
    if (status == MMAY_BYTECODE_RUN_OK && value != NULL) {
        status = mmay_bytecode_value_to_managed_cell(
            execution, value, value_cell
        );
    }
    return status;
}

MarimayBytecodeRunStatus mmay_bytecode_run_collection_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol,
    bool *handled
) {
    size_t target = 0;
    size_t collection_register = 0;
    size_t key_register = 0;
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    const MarimayBytecodeCollectionType *collection = NULL;
    const MarimayManagedMapDescriptor *descriptor = NULL;
    MarimayManagedCell key_cell, value_cell, output_cell;
    MarimayManagedRootFrame roots;
    MarimayManagedStatus managed_status = MMAY_MANAGED_OK;
    MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_OK;
    bool flag = false;
    if (handled == NULL) return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    *handled = false;
    if (instruction == NULL || registers == NULL || execution == NULL
        || function == NULL || instruction->opcode < MMAY_OPCODE_COLLECTION_NEW
        || instruction->opcode > MMAY_OPCODE_COLLECTION_ENTRY_VALUE) {
        return MMAY_BYTECODE_RUN_OK;
    }
    *handled = true;
    target = (size_t)instruction->operands[0].unsigned_value;
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_NEW) {
        layout = mmay_bytecode_register_layout(function, (uint32_t)target);
        collection = mmay_bytecode_collection_type(function, layout);
        descriptor = mmay_bytecode_collection_descriptor_for(
            execution, function, layout
        );
        if (collection == NULL || descriptor == NULL) {
            return execution->collection_descriptor_allocation_failed
                ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        managed_status = collection->is_set
            ? mmay_managed_set_new(
                &execution->managed_heap, descriptor,
                &registers[target].as.managed_reference
            ) : mmay_managed_map_new(
                &execution->managed_heap, descriptor,
                &registers[target].as.managed_reference
            );
        return mmay_bytecode_run_managed_status(managed_status);
    }
    collection_register = (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT
            || instruction->opcode == MMAY_OPCODE_COLLECTION_ADD
            || instruction->opcode == MMAY_OPCODE_COLLECTION_CLEAR)
        ? target : (size_t)instruction->operands[1].unsigned_value;
    layout = mmay_bytecode_register_layout(
        function, (uint32_t)collection_register
    );
    collection = mmay_bytecode_collection_type(function, layout);
    if (collection == NULL) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_LENGTH) {
        size_t length = 0;
        managed_status = collection->is_set
            ? mmay_managed_set_length(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference, &length
            ) : mmay_managed_map_length(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference, &length
            );
        if (managed_status == MMAY_MANAGED_OK && length > (size_t)INT64_MAX) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        registers[target].as.i64 = (int64_t)length;
        return mmay_bytecode_run_managed_status(managed_status);
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_CLEAR) {
        managed_status = collection->is_set
            ? mmay_managed_set_clear(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference
            ) : mmay_managed_map_clear(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference
            );
        return mmay_bytecode_run_managed_status(managed_status);
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_GENERATION) {
        managed_status = collection->is_set
            ? mmay_managed_set_mutation_generation(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                &registers[target].as.integer_bits
            ) : mmay_managed_map_mutation_generation(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                &registers[target].as.integer_bits
            );
        return mmay_bytecode_run_managed_status(managed_status);
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_REQUIRE_GENERATION) {
        uint64_t generation = registers[
            (size_t)instruction->operands[2].unsigned_value
        ].as.integer_bits;
        managed_status = collection->is_set
            ? mmay_managed_set_check_mutation_generation(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference, generation
            ) : mmay_managed_map_check_mutation_generation(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference, generation
            );
        if (managed_status == MMAY_MANAGED_OK) {
            registers[target] = registers[collection_register];
        }
        return mmay_collection_status(
            managed_status, panic, allocator, symbol,
            instruction->span, collection->is_set
        );
    }
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_KEY
        || instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_VALUE) {
        int64_t ordinal = registers[
            (size_t)instruction->operands[2].unsigned_value
        ].as.i64;
        if (ordinal < 0) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        memset(&key_cell, 0, sizeof(key_cell));
        memset(&value_cell, 0, sizeof(value_cell));
        managed_status = collection->is_set
            ? mmay_managed_set_entry(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                (size_t)ordinal, &key_cell
            ) : mmay_managed_map_entry(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                (size_t)ordinal, &key_cell, &value_cell
            );
        if (managed_status != MMAY_MANAGED_OK) {
            return mmay_bytecode_run_managed_status(managed_status);
        }
        return mmay_bytecode_managed_cell_to_value(
            execution,
            instruction->opcode == MMAY_OPCODE_COLLECTION_ENTRY_KEY
                ? key_cell : value_cell,
            &registers[target]
        );
    }
    key_register = (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT
            || instruction->opcode == MMAY_OPCODE_COLLECTION_ADD)
        ? (size_t)instruction->operands[1].unsigned_value
        : (size_t)instruction->operands[2].unsigned_value;
    memset(&roots, 0, sizeof(roots));
    status = mmay_collection_cells(
        execution,
        &registers[key_register],
        instruction->opcode == MMAY_OPCODE_COLLECTION_PUT
            ? &registers[(size_t)instruction->operands[2].unsigned_value]
            : NULL,
        &key_cell,
        &value_cell,
        &roots
    );
    if (status != MMAY_BYTECODE_RUN_OK) goto cleanup;
    if (instruction->opcode == MMAY_OPCODE_COLLECTION_CONTAINS) {
        managed_status = collection->is_set
            ? mmay_managed_set_contains(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                key_cell, &flag
            ) : mmay_managed_map_contains(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                key_cell, &flag
            );
        registers[target].as.boolean = flag;
    } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_GET) {
        managed_status = mmay_managed_map_get(
            &execution->managed_heap,
            registers[collection_register].as.managed_reference,
            key_cell, &output_cell, &flag
        );
        if (managed_status == MMAY_MANAGED_OK && !flag) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        } else if (managed_status == MMAY_MANAGED_OK) {
            status = mmay_bytecode_managed_cell_to_value(
                execution, output_cell, &registers[target]
            );
        }
    } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_PUT) {
        managed_status = mmay_managed_map_put(
            &execution->managed_heap,
            registers[collection_register].as.managed_reference,
            key_cell, value_cell, &flag
        );
    } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_ADD) {
        managed_status = mmay_managed_set_add(
            &execution->managed_heap,
            registers[collection_register].as.managed_reference,
            key_cell, &flag
        );
    } else if (instruction->opcode == MMAY_OPCODE_COLLECTION_REMOVE) {
        managed_status = collection->is_set
            ? mmay_managed_set_remove(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                key_cell, &flag
            ) : mmay_managed_map_remove(
                &execution->managed_heap,
                registers[collection_register].as.managed_reference,
                key_cell, &output_cell, &flag
            );
        registers[target].as.boolean = flag;
    } else {
        status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (status == MMAY_BYTECODE_RUN_OK) {
        status = mmay_bytecode_run_managed_status(managed_status);
    }
cleanup:
    if (roots.linked) {
        MarimayManagedStatus pop_status = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (status == MMAY_BYTECODE_RUN_OK) {
            status = mmay_bytecode_run_managed_status(pop_status);
        }
    }
    return status;
}
