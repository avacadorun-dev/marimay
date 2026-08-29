#include "mir_internal.h"
#include "execution_collections_internal.h"
#include "mmay/base/provenance.h"
#include <limits.h>
#include <string.h>
enum {
    MMAY_MIR_REFERENCE_KIND_SHIFT = 29
};
#define MMAY_MIR_REFERENCE_INDEX_MASK UINT32_C(0x1fffffff)
typedef enum MarimayMirReferenceKind {
    MMAY_MIR_REFERENCE_RECORD = 0,
    MMAY_MIR_REFERENCE_ENUM = 1,
    MMAY_MIR_REFERENCE_STRING = 2,
    MMAY_MIR_REFERENCE_ARRAY = 3,
    MMAY_MIR_REFERENCE_INTERFACE = 4,
    MMAY_MIR_REFERENCE_COLLECTION = MMAY_MIR_REFERENCE_COLLECTION_KIND
} MarimayMirReferenceKind;
bool mmay_mir_reference_encode(
    const MarimayMirExecution *execution,
    unsigned int raw_kind,
    size_t index,
    MarimayMirReference *result
) {
    const MarimayMirReferenceKind kind = (MarimayMirReferenceKind)raw_kind;
    uint32_t encoded = 0;
    if (execution == NULL || !execution->initialized
        || execution->provenance == 0
        || execution->provenance == UINT64_MAX || result == NULL
        || index >= MMAY_MIR_REFERENCE_INDEX_MASK) {
        return false;
    }
    encoded = ((uint32_t)kind << MMAY_MIR_REFERENCE_KIND_SHIFT)
        | (uint32_t)index;
    if (encoded == UINT32_MAX) {
        return false;
    }
    result->execution_provenance = execution->provenance;
    result->slot = encoded;
    return true;
}
bool mmay_mir_reference_decode(
    const MarimayMirExecution *execution,
    MarimayMirReference reference,
    unsigned int raw_expected,
    size_t *index
) {
    const MarimayMirReferenceKind expected =
        (MarimayMirReferenceKind)raw_expected;
    MarimayMirReferenceKind actual;
    if (execution == NULL || !execution->initialized
        || execution->provenance == 0
        || execution->provenance == UINT64_MAX
        || reference.execution_provenance != execution->provenance
        || reference.slot == UINT32_MAX || index == NULL) {
        return false;
    }
    actual = (MarimayMirReferenceKind)(
        reference.slot >> MMAY_MIR_REFERENCE_KIND_SHIFT
    );
    if (actual != expected) {
        return false;
    }
    *index = (size_t)(reference.slot & MMAY_MIR_REFERENCE_INDEX_MASK);
    return true;
}
static MarimayMirRecordField *mmay_mir_managed_record_layout(
    MarimayMirManagedRecord *record
) {
    return record != NULL ? (MarimayMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
}
static const MarimayMirRecordField *mmay_mir_managed_record_layout_const(
    const MarimayMirManagedRecord *record
) {
    return record != NULL ? (const MarimayMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
}
MarimayMirExecutionOptions mmay_mir_execution_default_options(void) {
    MarimayMirExecutionOptions options;
    options.max_objects = 1000000;
    options.max_fields_per_object = 100000;
    options.max_total_fields = 10000000;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
    options.max_arrays = 1000000;
    options.max_array_length = 1000000;
    options.max_total_array_elements = 10000000;
    options.max_collections = 1000000;
    options.max_collection_length = 1000000;
    options.max_total_collection_entries = 10000000;
    options.env = NULL;
    options.fs = NULL;
    options.process_context = NULL;
    return options;
}
bool mmay_mir_execution_init(
    MarimayMirExecution *execution,
    MarimayAllocator allocator,
    MarimayMirExecutionOptions options
) {
    uint64_t provenance = 0;
    if (execution == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options.max_objects == 0
        || options.max_objects > MMAY_MIR_REFERENCE_INDEX_MASK
        || options.max_fields_per_object == 0
        || options.max_total_fields == 0 || options.max_strings == 0
        || options.max_strings > MMAY_MIR_REFERENCE_INDEX_MASK
        || options.max_total_string_bytes == 0
        || options.max_arrays == 0
        || options.max_arrays > MMAY_MIR_REFERENCE_INDEX_MASK
        || options.max_array_length == 0
        || options.max_total_array_elements == 0
        || options.max_collections == 0
        || options.max_collections > MMAY_MIR_REFERENCE_INDEX_MASK
        || options.max_collection_length == 0
        || options.max_total_collection_entries == 0
        || !mmay_new_provenance(&provenance)) {
        return false;
    }
    memset(execution, 0, sizeof(*execution));
    if (!mmay_vector_init(
            &execution->objects,
            allocator,
            sizeof(MarimayMirManagedRecord *)
        ) || !mmay_vector_init(
            &execution->enums,
            allocator,
            sizeof(MarimayMirManagedEnum *)
        ) || !mmay_vector_init(
            &execution->strings,
            allocator,
            sizeof(MarimayMirManagedString *)
        ) || !mmay_vector_init(
            &execution->arrays,
            allocator,
            sizeof(MarimayMirManagedArray *)
        ) || !mmay_vector_init(
            &execution->collections,
            allocator,
            sizeof(MarimayMirManagedCollection *)
        ) || !mmay_vector_init(
            &execution->interface_values,
            allocator,
            sizeof(MarimayMirManagedInterface *)
        )) {
        mmay_vector_dispose(&execution->interface_values);
        mmay_vector_dispose(&execution->collections);
        mmay_vector_dispose(&execution->arrays);
        mmay_vector_dispose(&execution->strings);
        mmay_vector_dispose(&execution->enums);
        mmay_vector_dispose(&execution->objects);
        return false;
    }
    if (!mmay_env_init(&execution->env, allocator, options.env)) {
        mmay_vector_dispose(&execution->interface_values);
        mmay_vector_dispose(&execution->collections);
        mmay_vector_dispose(&execution->arrays);
        mmay_vector_dispose(&execution->strings);
        mmay_vector_dispose(&execution->enums);
        mmay_vector_dispose(&execution->objects);
        return false;
    }
    if (options.fs != NULL && !mmay_fs_init(
            &execution->fs, allocator, options.fs
        )) {
        mmay_env_dispose(&execution->env);
        mmay_vector_dispose(&execution->interface_values);
        mmay_vector_dispose(&execution->collections);
        mmay_vector_dispose(&execution->arrays);
        mmay_vector_dispose(&execution->strings);
        mmay_vector_dispose(&execution->enums);
        mmay_vector_dispose(&execution->objects);
        return false;
    }
    execution->allocator = allocator;
    execution->options = options;
    execution->provenance = provenance;
    execution->initialized = true;
    execution->env_initialized = true;
    execution->fs_initialized = options.fs != NULL;
    execution->process_context = options.process_context != NULL
        ? *options.process_context : mmay_process_context_default_options();
    return true;
}
void mmay_mir_execution_dispose(MarimayMirExecution *execution) {
    size_t index = 0;
    if (execution == NULL || !execution->initialized) {
        return;
    }
    if (execution->fs_initialized) mmay_fs_dispose(&execution->fs);
    if (execution->env_initialized) mmay_env_dispose(&execution->env);
    for (index = 0; index < execution->objects.length; index += 1) {
        MarimayMirManagedRecord **record = mmay_vector_at(
            &execution->objects,
            index
        );
        if (record != NULL && *record != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *record,
                (*record)->allocation_size
            );
            *record = NULL;
        }
    }
    for (index = 0; index < execution->enums.length; index += 1) {
        MarimayMirManagedEnum **value = mmay_vector_at(
            &execution->enums,
            index
        );
        if (value != NULL && *value != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->strings.length; index += 1) {
        MarimayMirManagedString **value = mmay_vector_at(
            &execution->strings,
            index
        );
        if (value != NULL && *value != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->arrays.length; index += 1) {
        MarimayMirManagedArray **value = mmay_vector_at(
            &execution->arrays,
            index
        );
        if (value != NULL && *value != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->interface_values.length; index += 1) {
        MarimayMirManagedInterface **value = mmay_vector_at(
            &execution->interface_values,
            index
        );
        if (value != NULL && *value != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->collections.length; index += 1) {
        MarimayMirManagedCollection **value = mmay_vector_at(
            &execution->collections, index
        );
        if (value != NULL && *value != NULL) {
            mmay_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    mmay_vector_dispose(&execution->interface_values);
    mmay_vector_dispose(&execution->collections);
    mmay_vector_dispose(&execution->arrays);
    mmay_vector_dispose(&execution->strings);
    mmay_vector_dispose(&execution->enums);
    mmay_vector_dispose(&execution->objects);
    mmay_test_output_dispose(&execution->test_output);
    memset(execution, 0, sizeof(*execution));
}
const MarimayMirManagedEnum *mmay_mir_execution_enum_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedEnum *const *stored = execution != NULL
            && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_ENUM,
                &index
            )
        ? mmay_vector_at_const(&execution->enums, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
bool mmay_mir_execution_inspect_enum(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t *variant,
    size_t *payload_count
) {
    const MarimayMirManagedEnum *stored = mmay_mir_execution_enum_value(
        execution,
        value
    );
    if (stored == NULL || variant == NULL || payload_count == NULL) {
        return false;
    }
    *variant = stored->variant;
    *payload_count = stored->selected_payload_count;
    return true;
}
bool mmay_mir_execution_enum_payload(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t index,
    MarimayMirValue *result
) {
    const MarimayMirManagedEnum *stored = mmay_mir_execution_enum_value(
        execution,
        value
    );
    if (stored == NULL || result == NULL
        || index >= stored->selected_payload_count) {
        return false;
    }
    *result = stored->payload[index];
    return true;
}
const MarimayMirManagedString *mmay_mir_execution_string_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedString *const *stored = execution != NULL
            && execution->initialized && value.type == MMAY_TYPE_STRING
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_STRING,
                &index
            )
        ? mmay_vector_at_const(&execution->strings, index)
        : NULL;
    return stored != NULL ? *stored : NULL;
}
MarimayMirManagedArray *mmay_mir_execution_array_value_mutable(
    MarimayMirExecution *execution,
    MarimayMirValue value
) {
    MarimayMirManagedArray **stored = mmay_mir_execution_array_slot(
        execution, value
    );
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
MarimayMirManagedArray **mmay_mir_execution_array_slot(
    MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedArray **stored = execution != NULL && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_ARRAY,
                &index
            )
        ? mmay_vector_at(&execution->arrays, index)
        : NULL;
    return stored;
}
const MarimayMirManagedArray *mmay_mir_execution_array_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedArray *const *stored = execution != NULL
            && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_ARRAY,
                &index
            )
        ? mmay_vector_at_const(&execution->arrays, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
const MarimayMirManagedInterface *mmay_mir_execution_interface_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedInterface *const *stored = execution != NULL
            && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_INTERFACE,
                &index
            )
        ? mmay_vector_at_const(&execution->interface_values, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
bool mmay_mir_execution_string_bytes(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    const unsigned char **bytes,
    size_t *length
) {
    const MarimayMirManagedString *stored = mmay_mir_execution_string_value(
        execution,
        value
    );
    if (stored == NULL || bytes == NULL || length == NULL) {
        return false;
    }
    *bytes = stored->bytes;
    *length = stored->length;
    return true;
}
bool mmay_mir_execution_array_length(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t *length
) {
    const MarimayMirManagedArray *stored = mmay_mir_execution_array_value(
        execution,
        value
    );
    if (stored == NULL || length == NULL) {
        return false;
    }
    *length = stored->length;
    return true;
}
bool mmay_mir_execution_array_element(
    const MarimayMirExecution *execution,
    MarimayMirValue value,
    size_t index,
    MarimayMirValue *result
) {
    const MarimayMirManagedArray *stored = mmay_mir_execution_array_value(
        execution,
        value
    );
    if (stored == NULL || result == NULL || index >= stored->length) {
        return false;
    }
    *result = stored->elements[index];
    return true;
}
bool mmay_mir_execution_array_at(
    const MarimayMirExecution *execution,
    size_t ordinal,
    MarimayMirValue *result
) {
    MarimayMirManagedArray *const *stored = execution != NULL
            && execution->initialized
        ? mmay_vector_at_const(&execution->arrays, ordinal)
        : NULL;
    if (stored == NULL || *stored == NULL || result == NULL
        || !mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_ARRAY,
            ordinal,
            &result->as.reference
        )) {
        return false;
    }
    result->type = (*stored)->type;
    return true;
}
MarimayMirManagedRecord *mmay_mir_execution_record(
    MarimayMirExecution *execution,
    MarimayMirReference reference
) {
    size_t index = 0;
    MarimayMirManagedRecord **record = execution != NULL
            && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                reference,
                MMAY_MIR_REFERENCE_RECORD,
                &index
            )
        ? mmay_vector_at(&execution->objects, index)
        : NULL;
    return record != NULL ? *record : NULL;
}
bool mmay_mir_managed_record_matches_layout(
    const MarimayMirFunction *function,
    const MarimayMirRecord *layout,
    const MarimayMirManagedRecord *record
) {
    const MarimayMirRecordField *stored_layout =
        mmay_mir_managed_record_layout_const(record);
    size_t field_index = 0;
    if (function == NULL || layout == NULL || record == NULL
        || record->type != layout->type
        || record->field_count != layout->field_count) {
        return false;
    }
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const MarimayMirRecordField *expected = mmay_mir_record_field(
            function,
            layout,
            field_index
        );
        if (expected == NULL || stored_layout == NULL
            || stored_layout[field_index].type != expected->type
            || stored_layout[field_index].is_mutable != expected->is_mutable
            || record->fields[field_index].type != expected->type) {
            return false;
        }
    }
    return true;
}
bool mmay_mir_execution_owns_reference(
    const MarimayMirExecution *execution,
    MarimayMirValue value
) {
    size_t index = 0;
    MarimayMirManagedRecord *const *record = execution != NULL
            && execution->initialized
            && mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_RECORD,
                &index
            )
        ? mmay_vector_at_const(&execution->objects, index)
        : NULL;
    return mmay_mir_execution_string_value(execution, value) != NULL
        || mmay_mir_execution_enum_value(execution, value) != NULL
        || mmay_mir_execution_array_value(execution, value) != NULL
        || (mmay_mir_reference_decode(
                execution,
                value.as.reference,
                MMAY_MIR_REFERENCE_COLLECTION,
                &index
            ) && mmay_vector_at_const(&execution->collections, index) != NULL)
        || mmay_mir_execution_interface_value(execution, value) != NULL
        || (record != NULL && *record != NULL && (*record)->type == value.type);
}
bool mmay_mir_type_is_managed(MarimayTypeId type) {
    return type == MMAY_TYPE_STRING || type >= MMAY_TYPE_FIRST_DYNAMIC;
}
bool mmay_mir_managed_array_matches_layout(
    const MarimayMirArray *layout,
    const MarimayMirManagedArray *value
) {
    return layout != NULL && value != NULL && value->type == layout->type
        && value->element_type == layout->element_type;
}
static MarimayMirEnumVariant *mmay_mir_managed_enum_variants(
    MarimayMirManagedEnum *value
) {
    return value != NULL ? (MarimayMirEnumVariant *)(
        value->payload + value->selected_payload_count
    ) : NULL;
}
static MarimayMirEnumPayload *mmay_mir_managed_enum_payloads(
    MarimayMirManagedEnum *value
) {
    MarimayMirEnumVariant *variants = mmay_mir_managed_enum_variants(value);
    return variants != NULL ? (MarimayMirEnumPayload *)(
        variants + value->variant_count
    ) : NULL;
}
bool mmay_mir_managed_enum_matches_layout(
    const MarimayMirFunction *function,
    const MarimayMirEnum *layout,
    const MarimayMirManagedEnum *value
) {
    const MarimayMirEnumVariant *stored_variants = NULL;
    const MarimayMirEnumPayload *stored_payloads = NULL;
    size_t variant_index = 0;
    size_t payload_index = 0;
    if (function == NULL || layout == NULL || value == NULL
        || value->type != layout->type
        || value->variant_count != layout->variant_count
        || value->layout_payload_count != layout->payload_count
        || value->variant >= value->variant_count) {
        return false;
    }
    stored_variants = (const MarimayMirEnumVariant *)(
        value->payload + value->selected_payload_count
    );
    stored_payloads = (const MarimayMirEnumPayload *)(
        stored_variants + value->variant_count
    );
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const MarimayMirEnumVariant *expected = mmay_mir_enum_variant(
            function,
            layout,
            variant_index
        );
        if (expected == NULL || expected->payload_start < layout->payload_start
            || stored_variants[variant_index].kind != expected->kind
            || stored_variants[variant_index].payload_start
                != expected->payload_start - layout->payload_start
            || stored_variants[variant_index].payload_count
                != expected->payload_count) {
            return false;
        }
    }
    for (payload_index = 0; payload_index < layout->payload_count;
        payload_index += 1) {
        const MarimayMirEnumPayload *expected = mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + payload_index
        );
        if (expected == NULL
            || stored_payloads[payload_index].type != expected->type) {
            return false;
        }
    }
    {
        const MarimayMirEnumVariant *selected = &stored_variants[value->variant];
        if (value->selected_payload_count != selected->payload_count) {
            return false;
        }
        for (payload_index = 0;
            payload_index < value->selected_payload_count;
            payload_index += 1) {
            size_t layout_index = selected->payload_start + payload_index;
            if (layout_index >= value->layout_payload_count
                || value->payload[payload_index].type
                    != stored_payloads[layout_index].type) {
                return false;
            }
        }
    }
    return true;
}
MarimayMirInterpretStatus mmay_mir_execution_new_record(
    MarimayMirExecution *execution,
    MarimayTypeId type,
    const MarimayMirValue *fields,
    const MarimayMirRecordField *layout,
    size_t field_count,
    MarimayMirReference *result
) {
    MarimayMirManagedRecord *record = NULL;
    size_t field_bytes = 0;
    size_t layout_bytes = 0;
    size_t payload_bytes = 0;
    size_t allocation_size = 0;
    size_t new_total_fields = 0;
    size_t object_count = 0;
    size_t field_index = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || type == MMAY_TYPE_INVALID) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (field_count != 0 && (fields == NULL || layout == NULL)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!mmay_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !mmay_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || object_count >= execution->options.max_objects
        || execution->objects.length >= MMAY_MIR_REFERENCE_INDEX_MASK
        || field_count > execution->options.max_fields_per_object
        || !mmay_size_add(
            execution->total_fields,
            field_count,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (!mmay_size_multiply(field_count, sizeof(MarimayMirValue), &field_bytes)
        || !mmay_size_multiply(
            field_count,
            sizeof(MarimayMirRecordField),
            &layout_bytes
        ) || !mmay_size_add(field_bytes, layout_bytes, &payload_bytes)
        || !mmay_size_add(
            offsetof(MarimayMirManagedRecord, fields),
            payload_bytes,
            &allocation_size
        )) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (field_index = 0; field_index < field_count; field_index += 1) {
        if (layout[field_index].type == MMAY_TYPE_INVALID
            || fields[field_index].type != layout[field_index].type) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!mmay_vector_reserve(
            &execution->objects,
            execution->objects.length + 1
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record = mmay_allocate(&execution->allocator, allocation_size);
    if (record == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record->type = type;
    record->field_count = field_count;
    record->allocation_size = allocation_size;
    if (field_count != 0 && fields != NULL && layout != NULL) {
        memcpy(record->fields, fields, field_bytes);
        memcpy(
            mmay_mir_managed_record_layout(record),
            layout,
            layout_bytes
        );
    }
    if (!mmay_vector_push(&execution->objects, &record)) {
        mmay_deallocate(&execution->allocator, record, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_RECORD,
            execution->objects.length - 1,
            result
        )
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}
MarimayMirInterpretStatus mmay_mir_execution_new_string(
    MarimayMirExecution *execution,
    const unsigned char *bytes,
    size_t length,
    MarimayMirReference *result
) {
    MarimayMirManagedString *value = NULL;
    size_t allocation_size = 0;
    size_t new_total = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || execution->strings.length >= execution->options.max_strings
        || execution->strings.length >= MMAY_MIR_REFERENCE_INDEX_MASK
        || !mmay_size_add(sizeof(*value), length, &allocation_size)
        || !mmay_size_add(
            execution->total_string_bytes,
            length,
            &new_total
        ) || new_total > execution->options.max_total_string_bytes) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    value = mmay_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->allocation_size = allocation_size;
    value->length = length;
    if (length != 0) {
        memcpy(value->bytes, bytes, length);
    }
    if (!mmay_vector_push(&execution->strings, &value)) {
        mmay_deallocate(&execution->allocator, value, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_string_bytes = new_total;
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_STRING,
            execution->strings.length - 1,
            result
        )
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}
MarimayMirInterpretStatus mmay_mir_execution_new_enum(
    MarimayMirExecution *execution,
    const MarimayMirFunction *function,
    const MarimayMirEnum *enum_layout,
    size_t selected_variant,
    const MarimayMirValue *payload_values,
    size_t payload_value_count,
    MarimayMirReference *result
) {
    const MarimayMirEnumVariant *selected = mmay_mir_enum_variant(
        function,
        enum_layout,
        selected_variant
    );
    MarimayMirManagedEnum *value = NULL;
    size_t value_bytes = 0;
    size_t variant_bytes = 0;
    size_t payload_layout_bytes = 0;
    size_t allocation_size = 0;
    size_t object_slots = 0;
    size_t object_count = 0;
    size_t new_total_fields = 0;
    size_t index = 0;
    if (execution == NULL || !execution->initialized || function == NULL
        || enum_layout == NULL || selected == NULL || result == NULL
        || selected->payload_count != payload_value_count
        || (payload_values == NULL && payload_value_count != 0)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!mmay_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !mmay_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || !mmay_size_add(
            payload_value_count,
            enum_layout->variant_count,
            &object_slots
        ) || !mmay_size_add(
            object_slots,
            enum_layout->payload_count,
            &object_slots
        ) || object_count >= execution->options.max_objects
        || execution->enums.length >= MMAY_MIR_REFERENCE_INDEX_MASK
        || object_slots > execution->options.max_fields_per_object
        || !mmay_size_add(
            execution->total_fields,
            object_slots,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields
        || !mmay_size_multiply(
            payload_value_count,
            sizeof(MarimayMirValue),
            &value_bytes
        ) || !mmay_size_multiply(
            enum_layout->variant_count,
            sizeof(MarimayMirEnumVariant),
            &variant_bytes
        ) || !mmay_size_multiply(
            enum_layout->payload_count,
            sizeof(MarimayMirEnumPayload),
            &payload_layout_bytes
        ) || !mmay_size_add(
            offsetof(MarimayMirManagedEnum, payload),
            value_bytes,
            &allocation_size
        ) || !mmay_size_add(
            allocation_size,
            variant_bytes,
            &allocation_size
        ) || !mmay_size_add(
            allocation_size,
            payload_layout_bytes,
            &allocation_size
        )) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (index = 0; index < payload_value_count; index += 1) {
        const MarimayMirEnumPayload *payload = mmay_mir_enum_payload(
            function,
            selected,
            index
        );
        if (payload == NULL || payload_values[index].type != payload->type) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!mmay_vector_reserve(
            &execution->enums,
            execution->enums.length + 1
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = mmay_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = enum_layout->type;
    value->variant = selected_variant;
    value->variant_count = enum_layout->variant_count;
    value->selected_payload_count = payload_value_count;
    value->layout_payload_count = enum_layout->payload_count;
    value->allocation_size = allocation_size;
    if (payload_value_count != 0 && payload_values != NULL) {
        memcpy(value->payload, payload_values, value_bytes);
    }
    for (index = 0; index < enum_layout->variant_count; index += 1) {
        const MarimayMirEnumVariant *variant = mmay_mir_enum_variant(
            function,
            enum_layout,
            index
        );
        MarimayMirEnumVariant *stored = mmay_mir_managed_enum_variants(value);
        if (variant == NULL || stored == NULL
            || variant->payload_start < enum_layout->payload_start) {
            mmay_deallocate(&execution->allocator, value, allocation_size);
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        stored[index] = *variant;
        stored[index].payload_start -= enum_layout->payload_start;
    }
    if (enum_layout->payload_count != 0) {
        const MarimayMirEnumPayload *layout_payloads = mmay_vector_at_const(
            &function->enum_payloads,
            enum_layout->payload_start
        );
        MarimayMirEnumPayload *stored_payloads =
            mmay_mir_managed_enum_payloads(value);
        if (layout_payloads == NULL || stored_payloads == NULL) {
            mmay_deallocate(&execution->allocator, value, allocation_size);
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        memcpy(
            stored_payloads,
            layout_payloads,
            payload_layout_bytes
        );
    }
    if (!mmay_vector_push(&execution->enums, &value)) {
        mmay_deallocate(&execution->allocator, value, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_ENUM,
            execution->enums.length - 1,
            result
        )
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}
MarimayMirInterpretStatus mmay_mir_execution_array_preflight(
    MarimayMirExecution *execution,
    size_t length,
    size_t *new_total,
    size_t *element_bytes,
    size_t *allocation_size
) {
    size_t checked_total = 0;
    size_t checked_element_bytes = 0;
    size_t checked_allocation_size = 0;
    if (execution == NULL || !execution->initialized) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (execution->arrays.length >= execution->options.max_arrays
        || execution->arrays.length >= MMAY_MIR_REFERENCE_INDEX_MASK
        || length > execution->options.max_array_length
        || (uintmax_t)length > (uintmax_t)INT64_MAX
        || !mmay_size_add(
            execution->total_array_elements,
            length,
            &checked_total
        ) || checked_total > execution->options.max_total_array_elements
        || !mmay_size_multiply(
            length,
            sizeof(MarimayMirValue),
            &checked_element_bytes
        ) || !mmay_size_add(
            offsetof(MarimayMirManagedArray, elements),
            checked_element_bytes,
            &checked_allocation_size
        )) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (new_total != NULL) {
        *new_total = checked_total;
    }
    if (element_bytes != NULL) {
        *element_bytes = checked_element_bytes;
    }
    if (allocation_size != NULL) {
        *allocation_size = checked_allocation_size;
    }
    return MMAY_MIR_INTERPRET_OK;
}
MarimayMirInterpretStatus mmay_mir_execution_new_array(
    MarimayMirExecution *execution,
    const MarimayMirArray *layout,
    const MarimayMirValue *elements,
    size_t length,
    MarimayMirReference *result
) {
    MarimayMirManagedArray *value = NULL;
    size_t element_bytes = 0;
    size_t allocation_size = 0;
    size_t new_total = 0;
    size_t index = 0;
    MarimayMirInterpretStatus preflight_status;
    if (execution == NULL || !execution->initialized || layout == NULL
        || result == NULL || layout->type < MMAY_TYPE_FIRST_DYNAMIC
        || layout->element_type == MMAY_TYPE_INVALID
        || (elements == NULL && length != 0)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    preflight_status = mmay_mir_execution_array_preflight(
        execution,
        length,
        &new_total,
        &element_bytes,
        &allocation_size
    );
    if (preflight_status != MMAY_MIR_INTERPRET_OK) {
        return preflight_status;
    }
    for (index = 0; index < length; index += 1) {
        if (elements[index].type != layout->element_type) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        if (mmay_mir_type_is_managed(elements[index].type)
            && !mmay_mir_execution_owns_reference(
                execution,
                elements[index]
            )) {
            return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        }
    }
    if (!mmay_vector_reserve(
            &execution->arrays,
            execution->arrays.length + 1
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = mmay_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = layout->type;
    value->element_type = layout->element_type;
    value->allocation_size = allocation_size;
    value->length = length;
    value->capacity = length;
    value->mutation_generation = 0;
    if (length != 0) {
        memcpy(value->elements, elements, element_bytes);
    }
    if (!mmay_vector_push(&execution->arrays, &value)) {
        mmay_deallocate(&execution->allocator, value, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_array_elements = new_total;
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_ARRAY,
            execution->arrays.length - 1,
            result
        )
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}

MarimayMirInterpretStatus mmay_mir_execution_new_interface(
    MarimayMirExecution *execution,
    MarimayTypeId type,
    MarimayMirValue payload,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayMirReference *result
) {
    MarimayMirManagedInterface *value = NULL;
    size_t method_bytes = 0;
    size_t allocation_size = 0;
    size_t object_count = 0;
    size_t object_slots = 0;
    size_t new_total_fields = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || type < MMAY_TYPE_FIRST_DYNAMIC
        || (methods == NULL && method_count != 0)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!mmay_mir_type_is_managed(payload.type)
        || !mmay_mir_execution_owns_reference(execution, payload)) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (!mmay_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !mmay_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || !mmay_size_add(method_count, 1, &object_slots)
        || object_count >= execution->options.max_objects
        || execution->interface_values.length >= MMAY_MIR_REFERENCE_INDEX_MASK
        || object_slots > execution->options.max_fields_per_object
        || !mmay_size_add(
            execution->total_fields,
            object_slots,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields
        || !mmay_size_multiply(
            method_count,
            sizeof(MarimaySymbolId),
            &method_bytes
        ) || !mmay_size_add(
            offsetof(MarimayMirManagedInterface, methods),
            method_bytes,
            &allocation_size
        )) {
        return MMAY_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (!mmay_vector_reserve(
            &execution->interface_values,
            execution->interface_values.length + 1
        )) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = mmay_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = type;
    value->payload = payload;
    value->method_count = method_count;
    value->allocation_size = allocation_size;
    if (method_count != 0) {
        memcpy(value->methods, methods, method_bytes);
    }
    if (!mmay_vector_push(&execution->interface_values, &value)) {
        mmay_deallocate(&execution->allocator, value, allocation_size);
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return mmay_mir_reference_encode(
            execution,
            MMAY_MIR_REFERENCE_INTERFACE,
            execution->interface_values.length - 1,
            result
        )
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_OBJECT_LIMIT;
}
