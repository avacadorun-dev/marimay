#include "mir_internal.h"
#include "execution_collections_internal.h"
#include "mir_collections_internal.h"

#include <limits.h>
#include <string.h>

enum { MMAY_MIR_COLLECTION_EQUALITY_DEPTH = 64 };

static MarimayMirManagedCollection **mmay_mir_collection_slot(
    MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    return execution != NULL && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_COLLECTION_KIND,
                &index
            )
        ? mmay_vector_at(&execution->collections, index) : NULL;
}

static MarimayMirManagedCollection *mmay_mir_collection_value(
    MarimayMirExecution *execution,
    MarimayMirValue value
) {
    MarimayMirManagedCollection **slot = mmay_mir_collection_slot(
        execution, value
    );
    return slot != NULL && *slot != NULL && (*slot)->type == value.type
        ? *slot : NULL;
}

static bool mmay_mir_collection_equal(
    MarimayMirExecution *execution,
    MarimayMirValue left,
    MarimayMirValue right,
    size_t depth,
    bool *valid
) {
    size_t index = 0;
    if (valid == NULL || !*valid || left.type != right.type
        || depth > MMAY_MIR_COLLECTION_EQUALITY_DEPTH) {
        if (valid != NULL) *valid = false;
        return false;
    }
    if (left.type == MMAY_TYPE_BOOL) {
        return left.as.boolean == right.as.boolean;
    }
    if (left.type >= MMAY_TYPE_I8 && left.type <= MMAY_TYPE_RUNE) {
        return left.as.integer_bits == right.as.integer_bits;
    }
    if (left.type == MMAY_TYPE_STRING) {
        const MarimayMirManagedString *left_string =
            mmay_mir_execution_string_value(execution, left);
        const MarimayMirManagedString *right_string =
            mmay_mir_execution_string_value(execution, right);
        if (left_string == NULL || right_string == NULL) {
            *valid = false;
            return false;
        }
        return left_string->length == right_string->length
            && (left_string->length == 0 || memcmp(
                left_string->bytes,
                right_string->bytes,
                left_string->length
            ) == 0);
    }
    {
        MarimayMirManagedRecord *left_record = mmay_mir_execution_record(
            execution, left.as.reference
        );
        MarimayMirManagedRecord *right_record = mmay_mir_execution_record(
            execution, right.as.reference
        );
        if (left_record != NULL || right_record != NULL) {
            if (left_record == NULL || right_record == NULL
                || left_record->type != left.type
                || right_record->type != right.type
                || left_record->field_count != right_record->field_count) {
                *valid = false;
                return false;
            }
            for (index = 0; index < left_record->field_count; index += 1) {
                if (!mmay_mir_collection_equal(
                        execution,
                        left_record->fields[index],
                        right_record->fields[index],
                        depth + 1,
                        valid
                    )) {
                    return false;
                }
            }
            return true;
        }
    }
    {
        const MarimayMirManagedEnum *left_enum =
            mmay_mir_execution_enum_value(execution, left);
        const MarimayMirManagedEnum *right_enum =
            mmay_mir_execution_enum_value(execution, right);
        if (left_enum != NULL || right_enum != NULL) {
            if (left_enum == NULL || right_enum == NULL
                || left_enum->type != left.type || right_enum->type != right.type
                || left_enum->variant != right_enum->variant
                || left_enum->selected_payload_count
                    != right_enum->selected_payload_count) {
                *valid = false;
                return false;
            }
            for (index = 0; index < left_enum->selected_payload_count;
                index += 1) {
                if (!mmay_mir_collection_equal(
                        execution,
                        left_enum->payload[index],
                        right_enum->payload[index],
                        depth + 1,
                        valid
                    )) {
                    return false;
                }
            }
            return true;
        }
    }
    *valid = false;
    return false;
}

static MarimayMirInterpretStatus mmay_mir_collection_find(
    MarimayMirExecution *execution,
    MarimayMirManagedCollection *collection,
    MarimayMirValue key,
    size_t *entry,
    bool *found
) {
    size_t index = 0;
    if (entry == NULL || found == NULL || key.type != collection->key_type) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (mmay_mir_type_is_managed(key.type)
        && !mmay_mir_execution_owns_reference(execution, key)) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    *found = false;
    *entry = SIZE_MAX;
    for (index = 0; index < collection->entry_count; index += 1) {
        bool valid = true;
        if (!collection->entries[index].active) continue;
        if (mmay_mir_collection_equal(
                execution,
                collection->entries[index].key,
                key,
                0,
                &valid
            )) {
            *found = true;
            *entry = index;
            return MMAY_MIR_INTERPRET_OK;
        }
        if (!valid) return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_mir_collection_new(
    MarimayMirExecution *execution,
    const MarimayMirCollection *layout,
    MarimayMirValue *target
) {
    const size_t allocation_size = offsetof(
        MarimayMirManagedCollection, entries
    );
    MarimayMirManagedCollection *collection = NULL;
    if (execution->collections.length >= execution->options.max_collections
        || execution->collections.length >= UINT32_MAX
        || !mmay_vector_reserve(
            &execution->collections, execution->collections.length + 1
        )) {
        return execution->collections.length >= execution->options.max_collections
            ? MMAY_MIR_INTERPRET_OBJECT_LIMIT
            : MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    collection = mmay_allocate(&execution->allocator, allocation_size);
    if (collection == NULL) return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    memset(collection, 0, allocation_size);
    collection->type = layout->type;
    collection->key_type = layout->key_type;
    collection->value_type = layout->value_type;
    collection->is_set = layout->is_set;
    collection->allocation_size = allocation_size;
    if (!mmay_vector_push(&execution->collections, &collection)) {
        mmay_deallocate(&execution->allocator, collection, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_COLLECTION_KIND,
            execution->collections.length - 1,
            &target->as.reference
        )
        ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}

static MarimayMirInterpretStatus mmay_mir_collection_reserve(
    MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t required
) {
    MarimayMirManagedCollection **slot = mmay_mir_collection_slot(
        execution, value
    );
    MarimayMirManagedCollection *old = slot != NULL ? *slot : NULL;
    MarimayMirManagedCollection *replacement = NULL;
    size_t capacity = old != NULL ? old->capacity : 0;
    size_t entries_bytes = 0;
    size_t allocation_size = 0;
    size_t index = 0;
    size_t cursor = 0;
    if (old == NULL) return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    if (required <= old->capacity && old->entry_count < old->capacity) {
        return MMAY_MIR_INTERPRET_OK;
    }
    if (capacity < 8) capacity = 8;
    while (capacity < required) {
        if (capacity > execution->options.max_collection_length / 2) {
            capacity = execution->options.max_collection_length;
            break;
        }
        capacity *= 2;
    }
    if (capacity < required || !mmay_size_multiply(
            capacity, sizeof(MarimayMirManagedCollectionEntry), &entries_bytes
        ) || !mmay_size_add(
            offsetof(MarimayMirManagedCollection, entries),
            entries_bytes,
            &allocation_size
        )) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    replacement = mmay_allocate(&execution->allocator, allocation_size);
    if (replacement == NULL) return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    memcpy(replacement, old, offsetof(MarimayMirManagedCollection, entries));
    memset(replacement->entries, 0, entries_bytes);
    for (index = 0; index < old->entry_count; index += 1) {
        if (old->entries[index].active) {
            replacement->entries[cursor] = old->entries[index];
            cursor += 1;
        }
    }
    replacement->entry_count = cursor;
    replacement->capacity = capacity;
    replacement->allocation_size = allocation_size;
    *slot = replacement;
    mmay_deallocate(&execution->allocator, old, old->allocation_size);
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_mir_collection_put(
    MarimayMirExecution *execution,
    MarimayMirValue collection_value,
    MarimayMirValue key,
    const MarimayMirValue *value,
    bool *inserted,
    bool *length_limit
) {
    MarimayMirManagedCollection *collection = mmay_mir_collection_value(
        execution, collection_value
    );
    size_t entry = SIZE_MAX;
    bool found = false;
    MarimayMirInterpretStatus status;
    if (collection == NULL || inserted == NULL || length_limit == NULL
        || (collection->is_set != (value == NULL))) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    *inserted = false;
    *length_limit = false;
    if (value != NULL && (value->type != collection->value_type
        || (mmay_mir_type_is_managed(value->type)
            && !mmay_mir_execution_owns_reference(execution, *value)))) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    status = mmay_mir_collection_find(
        execution, collection, key, &entry, &found
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    if (found) {
        if (collection->is_set) {
            return MMAY_MIR_INTERPRET_OK;
        }
        if (collection->mutation_generation == UINT64_MAX) {
            return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
        collection->entries[entry].value = *value;
        collection->mutation_generation += 1;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (collection->length >= (size_t)INT64_MAX) {
        *length_limit = true;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (collection->length >= execution->options.max_collection_length
        || execution->total_collection_entries
            >= execution->options.max_total_collection_entries) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (collection->mutation_generation == UINT64_MAX) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    status = mmay_mir_collection_reserve(
        execution, collection_value, collection->length + 1
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    collection = mmay_mir_collection_value(execution, collection_value);
    entry = collection->entry_count;
    collection->entries[entry].key = key;
    if (value != NULL) collection->entries[entry].value = *value;
    collection->entries[entry].active = true;
    collection->entry_count += 1;
    collection->length += 1;
    collection->mutation_generation += 1;
    execution->total_collection_entries += 1;
    *inserted = true;
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_mir_collection_generation_panic(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    bool is_set,
    const MarimayAllocator *allocator,
    MarimayMirPanic *panic
) {
    static const unsigned char map_message[] =
        "map iterator invalidated by mutation";
    static const unsigned char set_message[] =
        "set iterator invalidated by mutation";
    const unsigned char *message = is_set ? set_message : map_message;
    const size_t length = is_set
        ? sizeof(set_message) - 1 : sizeof(map_message) - 1;
    if (panic == NULL) return MMAY_MIR_INTERPRET_PANIC;
    if (!mmay_owned_bytes_copy(&panic->message, *allocator, message, length)) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    panic->code = "G4005";
    panic->symbol = function->symbol;
    panic->span = instruction->span;
    return MMAY_MIR_INTERPRET_PANIC;
}

static MarimayMirInterpretStatus mmay_mir_collection_option(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayMirManagedCollection *collection,
    MarimayMirValue key,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const MarimayMirEnum *option = mmay_mir_enum(
        function, instruction->enum_id
    );
    MarimayMirValue payload;
    size_t entry = SIZE_MAX;
    bool found = false;
    MarimayMirInterpretStatus status = mmay_mir_collection_find(
        options.execution, collection, key, &entry, &found
    );
    if (option == NULL) return MMAY_MIR_INTERPRET_INVALID_MIR;
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    memset(&payload, 0, sizeof(payload));
    if (found) payload = collection->entries[entry].value;
    if (found && instruction->kind == MMAY_MIR_COLLECTION_REMOVE_OPTION) {
        if (collection->mutation_generation == UINT64_MAX) {
            return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
        memset(
            &collection->entries[entry], 0, sizeof(collection->entries[entry])
        );
        collection->length -= 1;
        collection->mutation_generation += 1;
        options.execution->total_collection_entries -= 1;
    }
    return mmay_mir_execution_new_enum(
        options.execution, function, option, found ? 0U : 1U,
        found ? &payload : NULL, found ? 1U : 0U,
        &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_collection_update_result(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *collection,
    const MarimayMirValue *key,
    const MarimayMirValue *value,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
) {
    const MarimayMirEnum *result = mmay_mir_enum(
        function, instruction->enum_id
    );
    const MarimayMirEnum *error = mmay_mir_enum(
        function, instruction->option_enum_id
    );
    MarimayMirValue payload;
    MarimayMirValue error_value;
    bool inserted = false;
    bool length_limit = false;
    MarimayMirInterpretStatus status;
    if (result == NULL || error == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    status = mmay_mir_collection_put(
        options.execution, *collection, *key,
        instruction->kind == MMAY_MIR_COLLECTION_PUT_RESULT ? value : NULL,
        &inserted, &length_limit
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    memset(&payload, 0, sizeof(payload));
    if (!length_limit) {
        payload.type = MMAY_TYPE_BOOL;
        payload.as.boolean = inserted;
        return mmay_mir_execution_new_enum(
            options.execution, function, result, 0, &payload, 1,
            &target->as.reference
        );
    }
    payload.type = MMAY_TYPE_I64;
    payload.as.i64 = INT64_MAX;
    memset(&error_value, 0, sizeof(error_value));
    error_value.type = error->type;
    status = mmay_mir_execution_new_enum(
        options.execution, function, error, 0, &payload, 1,
        &error_value.as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 1, &error_value, 1,
        &target->as.reference
    );
}

MarimayMirInterpretStatus mmay_mir_execute_collection_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    const MarimayMirValue *third,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    const MarimayMirCollection *layout = mmay_mir_collection(
        function, instruction->collection
    );
    MarimayMirManagedCollection *collection = NULL;
    size_t entry = SIZE_MAX;
    bool found = false;
    MarimayMirInterpretStatus status;
    if (layout == NULL || options.execution == NULL) {
        return options.execution == NULL
            ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
            : MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_NEW) {
        return mmay_mir_collection_new(options.execution, layout, target);
    }
    collection = mmay_mir_collection_value(options.execution, *first);
    if (collection == NULL || collection->key_type != layout->key_type
        || collection->value_type != layout->value_type
        || collection->is_set != layout->is_set) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_LENGTH) {
        target->as.i64 = (int64_t)collection->length;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_CLEAR) {
        if (collection->length != 0) {
            if (collection->mutation_generation == UINT64_MAX) {
                return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
            }
            options.execution->total_collection_entries -= collection->length;
            memset(
                collection->entries,
                0,
                collection->capacity * sizeof(*collection->entries)
            );
            collection->length = 0;
            collection->entry_count = 0;
            collection->mutation_generation += 1;
        }
        *target = *first;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_GENERATION) {
        target->as.integer_bits = collection->mutation_generation;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_REQUIRE_GENERATION) {
        if (collection->mutation_generation != second->as.integer_bits) {
            return mmay_mir_collection_generation_panic(
                function, instruction, collection->is_set,
                allocator, options.panic
            );
        }
        *target = *first;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
        || instruction->kind == MMAY_MIR_COLLECTION_ENTRY_VALUE) {
        size_t index = 0;
        size_t active = 0;
        if (second->as.i64 < 0
            || (uint64_t)second->as.i64 >= collection->length) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        for (index = 0; index < collection->entry_count; index += 1) {
            if (!collection->entries[index].active) continue;
            if (active == (size_t)second->as.i64) {
                *target = instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
                    ? collection->entries[index].key
                    : collection->entries[index].value;
                return MMAY_MIR_INTERPRET_OK;
            }
            active += 1;
        }
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_PUT
        || instruction->kind == MMAY_MIR_COLLECTION_ADD) {
        bool inserted = false;
        bool length_limit = false;
        status = mmay_mir_collection_put(
            options.execution,
            *first,
            *second,
            instruction->kind == MMAY_MIR_COLLECTION_PUT ? third : NULL,
            &inserted,
            &length_limit
        );
        if (status == MMAY_MIR_INTERPRET_OK && length_limit) {
            status = MMAY_MIR_INTERPRET_OBJECT_LIMIT;
        }
        if (status == MMAY_MIR_INTERPRET_OK) *target = *first;
        return status;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_PUT_RESULT
        || instruction->kind == MMAY_MIR_COLLECTION_ADD_RESULT) {
        return mmay_mir_collection_update_result(
            function, instruction, first, second, third,
            target, options
        );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_GET_OPTION
        || instruction->kind == MMAY_MIR_COLLECTION_REMOVE_OPTION) {
        return mmay_mir_collection_option(
            function, instruction, collection, *second, target, options
        );
    }
    status = mmay_mir_collection_find(
        options.execution, collection, *second, &entry, &found
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    if (instruction->kind == MMAY_MIR_COLLECTION_CONTAINS) {
        target->as.boolean = found;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_GET) {
        if (!found || collection->is_set) return MMAY_MIR_INTERPRET_INVALID_MIR;
        *target = collection->entries[entry].value;
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->kind != MMAY_MIR_COLLECTION_REMOVE) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    target->as.boolean = found;
    if (!found) return MMAY_MIR_INTERPRET_OK;
    if (collection->mutation_generation == UINT64_MAX) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    memset(&collection->entries[entry], 0, sizeof(collection->entries[entry]));
    collection->length -= 1;
    collection->mutation_generation += 1;
    options.execution->total_collection_entries -= 1;
    return MMAY_MIR_INTERPRET_OK;
}
