#include "vm_internal.h"

#include <string.h>

typedef enum RudaBytecodeInterfaceOwnerKind {
    RUDA_BYTECODE_INTERFACE_OWNER_DESCRIPTOR,
    RUDA_BYTECODE_INTERFACE_OWNER_TABLE
} RudaBytecodeInterfaceOwnerKind;

typedef struct RudaBytecodeInterfaceOwner {
    RudaBytecodeInterfaceOwnerKind kind;
    size_t allocation_bytes;
} RudaBytecodeInterfaceOwner;

typedef struct RudaBytecodeInterfaceDescriptorOwner {
    RudaBytecodeInterfaceOwner owner;
    RudaSymbolId interface_symbol;
    RudaManagedInterfaceDescriptor descriptor;
} RudaBytecodeInterfaceDescriptorOwner;

typedef struct RudaBytecodeInterfaceTable {
    RudaBytecodeInterfaceOwner owner;
    RudaBytecodeInterfaceDescriptorOwner *interface_type;
    RudaBytecodeType payload_type;
    uint32_t payload_layout;
    const void *payload_descriptor;
    RudaSymbolId methods[];
} RudaBytecodeInterfaceTable;

static RudaBytecodeRunStatus ruda_bytecode_interface_managed_status(
    RudaManagedStatus status
) {
    if (status == RUDA_MANAGED_OK) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (status == RUDA_MANAGED_ALLOCATION_FAILED) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == RUDA_MANAGED_LIMIT_REACHED) {
        return RUDA_BYTECODE_RUN_LIMIT_REACHED;
    }
    if (status == RUDA_MANAGED_INVALID_REFERENCE) {
        return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

static const RudaBytecodeInterfaceTable *ruda_bytecode_interface_owned_table(
    const RudaBytecodeExecution *execution,
    const void *candidate
) {
    size_t index = 0;
    if (execution == NULL || candidate == NULL) {
        return NULL;
    }
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        RudaBytecodeInterfaceOwner *const *owner = ruda_vector_at_const(
            &execution->owned_interface_tables,
            index
        );
        if (owner != NULL && *owner == candidate
            && (*owner)->kind == RUDA_BYTECODE_INTERFACE_OWNER_TABLE) {
            return (const RudaBytecodeInterfaceTable *)*owner;
        }
    }
    return NULL;
}

static bool ruda_bytecode_interface_table_matches(
    const RudaBytecodeInterfaceTable *table,
    const RudaBytecodeInterfaceDescriptorOwner *interface_type,
    const void *payload_descriptor,
    const RudaBytecodeFunction *function,
    const RudaBytecodeInterfaceConstruction *construction
) {
    size_t index = 0;
    if (table == NULL || function == NULL || construction == NULL
        || interface_type == NULL || table->interface_type != interface_type
        || table->payload_type != construction->payload_type
        || table->payload_layout != construction->payload_layout
        || table->payload_descriptor != payload_descriptor
        || interface_type->descriptor.method_count
            != construction->method_count) {
        return false;
    }
    for (index = 0; index < construction->method_count; index += 1) {
        const RudaSymbolId *method = ruda_vector_at_const(
            &function->interface_methods,
            construction->method_start + index
        );
        if (method == NULL
            || !ruda_symbol_id_equal(table->methods[index], *method)) {
            return false;
        }
    }
    return true;
}

static bool ruda_bytecode_interface_payload_valid(
    RudaBytecodeExecution *execution,
    const RudaBytecodeInterfaceTable *table,
    RudaManagedReference payload
) {
    const void *actual = NULL;
    size_t variant = 0;
    if (execution == NULL || table == NULL
        || table->payload_descriptor == NULL) {
        return false;
    }
    if (table->payload_type == RUDA_BYTECODE_TYPE_RECORD) {
        const RudaManagedRecordDescriptor *record = NULL;
        if (ruda_managed_record_describe(
                &execution->managed_heap, payload, &record
            ) == RUDA_MANAGED_OK) {
            actual = record;
        }
    } else if (table->payload_type == RUDA_BYTECODE_TYPE_ENUM) {
        const RudaManagedEnumDescriptor *enumeration = NULL;
        if (ruda_managed_enum_describe(
                &execution->managed_heap,
                payload,
                &enumeration,
                &variant
            ) == RUDA_MANAGED_OK) {
            actual = enumeration;
        }
    }
    return actual == table->payload_descriptor;
}

static RudaBytecodeInterfaceDescriptorOwner *
ruda_bytecode_find_interface_descriptor(
    RudaBytecodeExecution *execution,
    RudaSymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        RudaBytecodeInterfaceOwner *const *owner = ruda_vector_at_const(
            &execution->owned_interface_tables,
            index
        );
        RudaBytecodeInterfaceDescriptorOwner *descriptor =
            owner != NULL && *owner != NULL
                && (*owner)->kind
                    == RUDA_BYTECODE_INTERFACE_OWNER_DESCRIPTOR
            ? (RudaBytecodeInterfaceDescriptorOwner *)*owner
            : NULL;
        if (descriptor != NULL && ruda_symbol_id_equal(
                descriptor->interface_symbol,
                symbol
            )) {
            return descriptor;
        }
    }
    return NULL;
}

static RudaBytecodeInterfaceDescriptorOwner *
ruda_bytecode_intern_interface_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeInterfaceLayout *layout
) {
    RudaBytecodeInterfaceDescriptorOwner *owned = NULL;
    RudaBytecodeInterfaceOwner *base = NULL;
    if (execution == NULL || layout == NULL) {
        return NULL;
    }
    owned = ruda_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    if (owned != NULL) {
        return owned->descriptor.method_count == layout->method_count
            ? owned
            : NULL;
    }
    owned = ruda_allocate(&execution->allocator, sizeof(*owned));
    if (owned == NULL) {
        return NULL;
    }
    memset(owned, 0, sizeof(*owned));
    owned->owner.kind = RUDA_BYTECODE_INTERFACE_OWNER_DESCRIPTOR;
    owned->owner.allocation_bytes = sizeof(*owned);
    owned->interface_symbol = layout->symbol;
    owned->descriptor.method_count = layout->method_count;
    base = &owned->owner;
    if (!ruda_vector_push(&execution->owned_interface_tables, &base)) {
        ruda_deallocate(&execution->allocator, owned, sizeof(*owned));
        return NULL;
    }
    return owned;
}

const RudaManagedInterfaceDescriptor *ruda_bytecode_run_interface_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeInterfaceId interface_id
) {
    const RudaBytecodeInterfaceLayout *layout =
        ruda_bytecode_interface_layout(function, interface_id);
    RudaBytecodeInterfaceDescriptorOwner *owned =
        ruda_bytecode_intern_interface_descriptor(execution, layout);
    return owned != NULL ? &owned->descriptor : NULL;
}

RudaBytecodeRunStatus ruda_bytecode_run_interface_new(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function
) {
    const RudaBytecodeInterfaceConstruction *construction = NULL;
    const RudaBytecodeInterfaceLayout *layout = NULL;
    RudaBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    RudaBytecodeInterfaceTable *table = NULL;
    RudaBytecodeValue payload_value;
    const void *payload_descriptor = NULL;
    RudaManagedReference payload;
    RudaManagedStatus managed_status;
    size_t method_bytes = 0;
    size_t allocation_bytes = 0;
    size_t method_index = 0;
    size_t table_index = 0;
    size_t target = 0;
    bool created = false;
    if (instruction == NULL || registers == NULL || execution == NULL
        || !execution->initialized || function == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    construction = ruda_bytecode_interface_construction(
        function,
        (RudaBytecodeInterfaceConstructionId)
            instruction->operands[1].unsigned_value
    );
    layout = construction != NULL
        ? ruda_bytecode_interface_layout(function, construction->interface_id)
        : NULL;
    if (construction == NULL || layout == NULL
        || (construction->payload_type != RUDA_BYTECODE_TYPE_RECORD
            && construction->payload_type != RUDA_BYTECODE_TYPE_ENUM)
        || !ruda_size_multiply(
            construction->method_count, sizeof(RudaSymbolId), &method_bytes
        ) || !ruda_size_add(
            sizeof(*table), method_bytes, &allocation_bytes
        )) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    interface_type = ruda_bytecode_intern_interface_descriptor(
        execution,
        layout
    );
    if (interface_type == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    payload_value.type = construction->payload_type;
    payload_value.as.managed_reference =
        registers[construction->payload_register].as.managed_reference;
    {
        RudaBytecodeRunStatus payload_status =
            ruda_bytecode_run_nominal_value_validate(
                execution,
                function,
                payload_value,
                construction->payload_type,
                construction->payload_layout
            );
        if (payload_status != RUDA_BYTECODE_RUN_OK) {
            return payload_status;
        }
    }
    {
        RudaBytecodeRunStatus descriptor_status =
            ruda_bytecode_run_nominal_descriptor(
                execution,
                function,
                construction->payload_type,
                construction->payload_layout,
                &payload_descriptor
            );
        if (descriptor_status != RUDA_BYTECODE_RUN_OK) {
            return descriptor_status;
        }
    }
    for (table_index = 0;
        table_index < execution->owned_interface_tables.length;
        table_index += 1) {
        RudaBytecodeInterfaceOwner *const *owner = ruda_vector_at_const(
            &execution->owned_interface_tables,
            table_index
        );
        RudaBytecodeInterfaceTable *candidate = owner != NULL
                && *owner != NULL
                && (*owner)->kind == RUDA_BYTECODE_INTERFACE_OWNER_TABLE
            ? (RudaBytecodeInterfaceTable *)*owner
            : NULL;
        if (candidate != NULL && ruda_bytecode_interface_table_matches(
                candidate,
                interface_type,
                payload_descriptor,
                function,
                construction
            )) {
            table = candidate;
            break;
        }
    }
    if (table == NULL) {
        table = ruda_allocate(&execution->allocator, allocation_bytes);
        if (table == NULL) {
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        memset(table, 0, allocation_bytes);
        table->owner.kind = RUDA_BYTECODE_INTERFACE_OWNER_TABLE;
        table->owner.allocation_bytes = allocation_bytes;
        table->interface_type = interface_type;
        table->payload_type = construction->payload_type;
        table->payload_layout = construction->payload_layout;
        table->payload_descriptor = payload_descriptor;
        for (method_index = 0; method_index < construction->method_count;
            method_index += 1) {
            const RudaSymbolId *method = ruda_vector_at_const(
                &function->interface_methods,
                construction->method_start + method_index
            );
            if (method == NULL) {
                ruda_deallocate(
                    &execution->allocator,
                    table,
                    allocation_bytes
                );
                return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            }
            table->methods[method_index] = *method;
        }
        if (!ruda_vector_push(&execution->owned_interface_tables, &table)) {
            ruda_deallocate(&execution->allocator, table, allocation_bytes);
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        created = true;
    }
    payload = registers[construction->payload_register].as.managed_reference;
    target = (size_t)instruction->operands[0].unsigned_value;
    managed_status = ruda_managed_interface_new(
        &execution->managed_heap,
        &interface_type->descriptor,
        payload,
        table,
        &registers[target].as.managed_reference
    );
    if (managed_status != RUDA_MANAGED_OK && created) {
        execution->owned_interface_tables.length -= 1;
        ruda_deallocate(&execution->allocator, table, allocation_bytes);
    }
    return ruda_bytecode_interface_managed_status(managed_status);
}

RudaBytecodeRunStatus ruda_bytecode_run_interface_resolve(
    const RudaBytecodeInstruction *instruction,
    const RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaSymbolId *method,
    RudaManagedReference *payload
) {
    const RudaBytecodeCall *call = NULL;
    const RudaBytecodeCallArgument *receiver = NULL;
    const RudaBytecodeInterfaceLayout *layout = NULL;
    const RudaManagedInterfaceDescriptor *descriptor = NULL;
    RudaBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    const RudaBytecodeInterfaceTable *table = NULL;
    const void *method_table = NULL;
    RudaManagedStatus managed_status;
    if (instruction == NULL || registers == NULL || execution == NULL
        || function == NULL || method == NULL || payload == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    call = ruda_bytecode_call(
        function,
        (RudaBytecodeCallId)instruction->operands[1].unsigned_value
    );
    receiver = call != NULL && call->argument_count != 0
        ? ruda_bytecode_call_argument(function, call->argument_start)
        : NULL;
    layout = receiver != NULL
        ? ruda_bytecode_interface_layout(function, receiver->layout)
        : NULL;
    if (call == NULL || receiver == NULL || layout == NULL
        || call->interface_slot >= layout->method_count) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    interface_type = ruda_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    managed_status = ruda_managed_interface_describe(
        &execution->managed_heap,
        registers[receiver->register_id].as.managed_reference,
        &descriptor,
        payload,
        &method_table
    );
    if (managed_status != RUDA_MANAGED_OK) {
        return ruda_bytecode_interface_managed_status(managed_status);
    }
    table = ruda_bytecode_interface_owned_table(execution, method_table);
    if (table == NULL || interface_type == NULL
        || table->interface_type != interface_type
        || descriptor != &interface_type->descriptor
        || interface_type->descriptor.method_count != layout->method_count
        || call->interface_slot >= interface_type->descriptor.method_count
        || !ruda_bytecode_interface_payload_valid(
            execution, table, *payload
        )) {
        return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
    }
    *method = table->methods[call->interface_slot];
    return RUDA_BYTECODE_RUN_OK;
}

bool ruda_bytecode_run_interface_value_valid(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue value,
    uint32_t layout_id
) {
    const RudaBytecodeInterfaceLayout *layout =
        ruda_bytecode_interface_layout(function, layout_id);
    const RudaManagedInterfaceDescriptor *descriptor = NULL;
    RudaBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    RudaManagedReference payload = ruda_managed_invalid_reference();
    const void *method_table = NULL;
    const RudaBytecodeInterfaceTable *table = NULL;
    if (execution == NULL || value.type != RUDA_BYTECODE_TYPE_INTERFACE
        || layout == NULL || ruda_managed_interface_describe(
            &execution->managed_heap,
            value.as.managed_reference,
            &descriptor,
            &payload,
            &method_table
        ) != RUDA_MANAGED_OK) {
        return false;
    }
    table = ruda_bytecode_interface_owned_table(execution, method_table);
    interface_type = ruda_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    return table != NULL && interface_type != NULL
        && table->interface_type == interface_type
        && descriptor == &interface_type->descriptor
        && interface_type->descriptor.method_count == layout->method_count
        && ruda_bytecode_interface_payload_valid(
            execution, table, payload
        );
}

void ruda_bytecode_run_interface_tables_dispose(
    RudaBytecodeExecution *execution
) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        RudaBytecodeInterfaceOwner **owner = ruda_vector_at(
            &execution->owned_interface_tables,
            index
        );
        if (owner != NULL && *owner != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *owner,
                (*owner)->allocation_bytes
            );
        }
    }
    ruda_vector_dispose(&execution->owned_interface_tables);
}
