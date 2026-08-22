#include "vm_internal.h"

#include <string.h>

typedef enum MarimayBytecodeInterfaceOwnerKind {
    MMAY_BYTECODE_INTERFACE_OWNER_DESCRIPTOR,
    MMAY_BYTECODE_INTERFACE_OWNER_TABLE
} MarimayBytecodeInterfaceOwnerKind;

typedef struct MarimayBytecodeInterfaceOwner {
    MarimayBytecodeInterfaceOwnerKind kind;
    size_t allocation_bytes;
} MarimayBytecodeInterfaceOwner;

typedef struct MarimayBytecodeInterfaceDescriptorOwner {
    MarimayBytecodeInterfaceOwner owner;
    MarimaySymbolId interface_symbol;
    MarimayManagedInterfaceDescriptor descriptor;
} MarimayBytecodeInterfaceDescriptorOwner;

typedef struct MarimayBytecodeInterfaceTable {
    MarimayBytecodeInterfaceOwner owner;
    MarimayBytecodeInterfaceDescriptorOwner *interface_type;
    MarimayBytecodeType payload_type;
    uint32_t payload_layout;
    const void *payload_descriptor;
    MarimaySymbolId methods[];
} MarimayBytecodeInterfaceTable;

static MarimayBytecodeRunStatus mmay_bytecode_interface_managed_status(
    MarimayManagedStatus status
) {
    if (status == MMAY_MANAGED_OK) {
        return MMAY_BYTECODE_RUN_OK;
    }
    if (status == MMAY_MANAGED_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == MMAY_MANAGED_LIMIT_REACHED) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    if (status == MMAY_MANAGED_INVALID_REFERENCE) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}

static const MarimayBytecodeInterfaceTable *mmay_bytecode_interface_owned_table(
    const MarimayBytecodeExecution *execution,
    const void *candidate
) {
    size_t index = 0;
    if (execution == NULL || candidate == NULL) {
        return NULL;
    }
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        MarimayBytecodeInterfaceOwner *const *owner = mmay_vector_at_const(
            &execution->owned_interface_tables,
            index
        );
        if (owner != NULL && *owner == candidate
            && (*owner)->kind == MMAY_BYTECODE_INTERFACE_OWNER_TABLE) {
            return (const MarimayBytecodeInterfaceTable *)*owner;
        }
    }
    return NULL;
}

static bool mmay_bytecode_interface_table_matches(
    const MarimayBytecodeInterfaceTable *table,
    const MarimayBytecodeInterfaceDescriptorOwner *interface_type,
    const void *payload_descriptor,
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInterfaceConstruction *construction
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
        const MarimaySymbolId *method = mmay_vector_at_const(
            &function->interface_methods,
            construction->method_start + index
        );
        if (method == NULL
            || !mmay_symbol_id_equal(table->methods[index], *method)) {
            return false;
        }
    }
    return true;
}

static bool mmay_bytecode_interface_payload_valid(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeInterfaceTable *table,
    MarimayManagedReference payload
) {
    const void *actual = NULL;
    size_t variant = 0;
    if (execution == NULL || table == NULL
        || table->payload_descriptor == NULL) {
        return false;
    }
    if (table->payload_type == MMAY_BYTECODE_TYPE_RECORD) {
        const MarimayManagedRecordDescriptor *record = NULL;
        if (mmay_managed_record_describe(
                &execution->managed_heap, payload, &record
            ) == MMAY_MANAGED_OK) {
            actual = record;
        }
    } else if (table->payload_type == MMAY_BYTECODE_TYPE_ENUM) {
        const MarimayManagedEnumDescriptor *enumeration = NULL;
        if (mmay_managed_enum_describe(
                &execution->managed_heap,
                payload,
                &enumeration,
                &variant
            ) == MMAY_MANAGED_OK) {
            actual = enumeration;
        }
    }
    return actual == table->payload_descriptor;
}

static MarimayBytecodeInterfaceDescriptorOwner *
mmay_bytecode_find_interface_descriptor(
    MarimayBytecodeExecution *execution,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        MarimayBytecodeInterfaceOwner *const *owner = mmay_vector_at_const(
            &execution->owned_interface_tables,
            index
        );
        MarimayBytecodeInterfaceDescriptorOwner *descriptor =
            owner != NULL && *owner != NULL
                && (*owner)->kind
                    == MMAY_BYTECODE_INTERFACE_OWNER_DESCRIPTOR
            ? (MarimayBytecodeInterfaceDescriptorOwner *)*owner
            : NULL;
        if (descriptor != NULL && mmay_symbol_id_equal(
                descriptor->interface_symbol,
                symbol
            )) {
            return descriptor;
        }
    }
    return NULL;
}

static MarimayBytecodeInterfaceDescriptorOwner *
mmay_bytecode_intern_interface_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeInterfaceLayout *layout
) {
    MarimayBytecodeInterfaceDescriptorOwner *owned = NULL;
    MarimayBytecodeInterfaceOwner *base = NULL;
    if (execution == NULL || layout == NULL) {
        return NULL;
    }
    owned = mmay_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    if (owned != NULL) {
        return owned->descriptor.method_count == layout->method_count
            ? owned
            : NULL;
    }
    owned = mmay_allocate(&execution->allocator, sizeof(*owned));
    if (owned == NULL) {
        return NULL;
    }
    memset(owned, 0, sizeof(*owned));
    owned->owner.kind = MMAY_BYTECODE_INTERFACE_OWNER_DESCRIPTOR;
    owned->owner.allocation_bytes = sizeof(*owned);
    owned->interface_symbol = layout->symbol;
    owned->descriptor.method_count = layout->method_count;
    base = &owned->owner;
    if (!mmay_vector_push(&execution->owned_interface_tables, &base)) {
        mmay_deallocate(&execution->allocator, owned, sizeof(*owned));
        return NULL;
    }
    return owned;
}

const MarimayManagedInterfaceDescriptor *mmay_bytecode_run_interface_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id
) {
    const MarimayBytecodeInterfaceLayout *layout =
        mmay_bytecode_interface_layout(function, interface_id);
    MarimayBytecodeInterfaceDescriptorOwner *owned =
        mmay_bytecode_intern_interface_descriptor(execution, layout);
    return owned != NULL ? &owned->descriptor : NULL;
}

MarimayBytecodeRunStatus mmay_bytecode_run_interface_new(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    const MarimayBytecodeInterfaceConstruction *construction = NULL;
    const MarimayBytecodeInterfaceLayout *layout = NULL;
    MarimayBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    MarimayBytecodeInterfaceTable *table = NULL;
    MarimayBytecodeValue payload_value;
    const void *payload_descriptor = NULL;
    MarimayManagedReference payload;
    MarimayManagedStatus managed_status;
    size_t method_bytes = 0;
    size_t allocation_bytes = 0;
    size_t method_index = 0;
    size_t table_index = 0;
    size_t target = 0;
    bool created = false;
    if (instruction == NULL || registers == NULL || execution == NULL
        || !execution->initialized || function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    construction = mmay_bytecode_interface_construction(
        function,
        (MarimayBytecodeInterfaceConstructionId)
            instruction->operands[1].unsigned_value
    );
    layout = construction != NULL
        ? mmay_bytecode_interface_layout(function, construction->interface_id)
        : NULL;
    if (construction == NULL || layout == NULL
        || (construction->payload_type != MMAY_BYTECODE_TYPE_RECORD
            && construction->payload_type != MMAY_BYTECODE_TYPE_ENUM)
        || !mmay_size_multiply(
            construction->method_count, sizeof(MarimaySymbolId), &method_bytes
        ) || !mmay_size_add(
            sizeof(*table), method_bytes, &allocation_bytes
        )) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    interface_type = mmay_bytecode_intern_interface_descriptor(
        execution,
        layout
    );
    if (interface_type == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    payload_value.type = construction->payload_type;
    payload_value.as.managed_reference =
        registers[construction->payload_register].as.managed_reference;
    {
        MarimayBytecodeRunStatus payload_status =
            mmay_bytecode_run_nominal_value_validate(
                execution,
                function,
                payload_value,
                construction->payload_type,
                construction->payload_layout
            );
        if (payload_status != MMAY_BYTECODE_RUN_OK) {
            return payload_status;
        }
    }
    {
        MarimayBytecodeRunStatus descriptor_status =
            mmay_bytecode_run_nominal_descriptor(
                execution,
                function,
                construction->payload_type,
                construction->payload_layout,
                &payload_descriptor
            );
        if (descriptor_status != MMAY_BYTECODE_RUN_OK) {
            return descriptor_status;
        }
    }
    for (table_index = 0;
        table_index < execution->owned_interface_tables.length;
        table_index += 1) {
        MarimayBytecodeInterfaceOwner *const *owner = mmay_vector_at_const(
            &execution->owned_interface_tables,
            table_index
        );
        MarimayBytecodeInterfaceTable *candidate = owner != NULL
                && *owner != NULL
                && (*owner)->kind == MMAY_BYTECODE_INTERFACE_OWNER_TABLE
            ? (MarimayBytecodeInterfaceTable *)*owner
            : NULL;
        if (candidate != NULL && mmay_bytecode_interface_table_matches(
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
        table = mmay_allocate(&execution->allocator, allocation_bytes);
        if (table == NULL) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        memset(table, 0, allocation_bytes);
        table->owner.kind = MMAY_BYTECODE_INTERFACE_OWNER_TABLE;
        table->owner.allocation_bytes = allocation_bytes;
        table->interface_type = interface_type;
        table->payload_type = construction->payload_type;
        table->payload_layout = construction->payload_layout;
        table->payload_descriptor = payload_descriptor;
        for (method_index = 0; method_index < construction->method_count;
            method_index += 1) {
            const MarimaySymbolId *method = mmay_vector_at_const(
                &function->interface_methods,
                construction->method_start + method_index
            );
            if (method == NULL) {
                mmay_deallocate(
                    &execution->allocator,
                    table,
                    allocation_bytes
                );
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            table->methods[method_index] = *method;
        }
        if (!mmay_vector_push(&execution->owned_interface_tables, &table)) {
            mmay_deallocate(&execution->allocator, table, allocation_bytes);
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        created = true;
    }
    payload = registers[construction->payload_register].as.managed_reference;
    target = (size_t)instruction->operands[0].unsigned_value;
    managed_status = mmay_managed_interface_new(
        &execution->managed_heap,
        &interface_type->descriptor,
        payload,
        table,
        &registers[target].as.managed_reference
    );
    if (managed_status != MMAY_MANAGED_OK && created) {
        execution->owned_interface_tables.length -= 1;
        mmay_deallocate(&execution->allocator, table, allocation_bytes);
    }
    return mmay_bytecode_interface_managed_status(managed_status);
}

MarimayBytecodeRunStatus mmay_bytecode_run_interface_resolve(
    const MarimayBytecodeInstruction *instruction,
    const MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimaySymbolId *method,
    MarimayManagedReference *payload
) {
    const MarimayBytecodeCall *call = NULL;
    const MarimayBytecodeCallArgument *receiver = NULL;
    const MarimayBytecodeInterfaceLayout *layout = NULL;
    const MarimayManagedInterfaceDescriptor *descriptor = NULL;
    MarimayBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    const MarimayBytecodeInterfaceTable *table = NULL;
    const void *method_table = NULL;
    MarimayManagedStatus managed_status;
    if (instruction == NULL || registers == NULL || execution == NULL
        || function == NULL || method == NULL || payload == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    call = mmay_bytecode_call(
        function,
        (MarimayBytecodeCallId)instruction->operands[1].unsigned_value
    );
    receiver = call != NULL && call->argument_count != 0
        ? mmay_bytecode_call_argument(function, call->argument_start)
        : NULL;
    layout = receiver != NULL
        ? mmay_bytecode_interface_layout(function, receiver->layout)
        : NULL;
    if (call == NULL || receiver == NULL || layout == NULL
        || call->interface_slot >= layout->method_count) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    interface_type = mmay_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    managed_status = mmay_managed_interface_describe(
        &execution->managed_heap,
        registers[receiver->register_id].as.managed_reference,
        &descriptor,
        payload,
        &method_table
    );
    if (managed_status != MMAY_MANAGED_OK) {
        return mmay_bytecode_interface_managed_status(managed_status);
    }
    table = mmay_bytecode_interface_owned_table(execution, method_table);
    if (table == NULL || interface_type == NULL
        || table->interface_type != interface_type
        || descriptor != &interface_type->descriptor
        || interface_type->descriptor.method_count != layout->method_count
        || call->interface_slot >= interface_type->descriptor.method_count
        || !mmay_bytecode_interface_payload_valid(
            execution, table, *payload
        )) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    *method = table->methods[call->interface_slot];
    return MMAY_BYTECODE_RUN_OK;
}

bool mmay_bytecode_run_interface_value_valid(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value,
    uint32_t layout_id
) {
    const MarimayBytecodeInterfaceLayout *layout =
        mmay_bytecode_interface_layout(function, layout_id);
    const MarimayManagedInterfaceDescriptor *descriptor = NULL;
    MarimayBytecodeInterfaceDescriptorOwner *interface_type = NULL;
    MarimayManagedReference payload = mmay_managed_invalid_reference();
    const void *method_table = NULL;
    const MarimayBytecodeInterfaceTable *table = NULL;
    if (execution == NULL || value.type != MMAY_BYTECODE_TYPE_INTERFACE
        || layout == NULL || mmay_managed_interface_describe(
            &execution->managed_heap,
            value.as.managed_reference,
            &descriptor,
            &payload,
            &method_table
        ) != MMAY_MANAGED_OK) {
        return false;
    }
    table = mmay_bytecode_interface_owned_table(execution, method_table);
    interface_type = mmay_bytecode_find_interface_descriptor(
        execution,
        layout->symbol
    );
    return table != NULL && interface_type != NULL
        && table->interface_type == interface_type
        && descriptor == &interface_type->descriptor
        && interface_type->descriptor.method_count == layout->method_count
        && mmay_bytecode_interface_payload_valid(
            execution, table, payload
        );
}

void mmay_bytecode_run_interface_tables_dispose(
    MarimayBytecodeExecution *execution
) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    for (index = 0; index < execution->owned_interface_tables.length;
        index += 1) {
        MarimayBytecodeInterfaceOwner **owner = mmay_vector_at(
            &execution->owned_interface_tables,
            index
        );
        if (owner != NULL && *owner != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *owner,
                (*owner)->allocation_bytes
            );
        }
    }
    mmay_vector_dispose(&execution->owned_interface_tables);
}
