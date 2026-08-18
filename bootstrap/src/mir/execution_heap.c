#include "mir_internal.h"
#include "ruda/base/provenance.h"
#include <limits.h>
#include <string.h>
enum {
    RUDA_MIR_REFERENCE_KIND_SHIFT = 29
};
#define RUDA_MIR_REFERENCE_INDEX_MASK UINT32_C(0x1fffffff)
typedef enum RudaMirReferenceKind {
    RUDA_MIR_REFERENCE_RECORD = 0,
    RUDA_MIR_REFERENCE_ENUM = 1,
    RUDA_MIR_REFERENCE_STRING = 2,
    RUDA_MIR_REFERENCE_ARRAY = 3,
    RUDA_MIR_REFERENCE_INTERFACE = 4
} RudaMirReferenceKind;
static bool ruda_mir_reference_encode(
    const RudaMirExecution *execution,
    RudaMirReferenceKind kind,
    size_t index,
    RudaMirReference *result
) {
    uint32_t encoded = 0;
    if (execution == NULL || !execution->initialized
        || execution->provenance == 0
        || execution->provenance == UINT64_MAX || result == NULL
        || index >= RUDA_MIR_REFERENCE_INDEX_MASK) {
        return false;
    }
    encoded = ((uint32_t)kind << RUDA_MIR_REFERENCE_KIND_SHIFT)
        | (uint32_t)index;
    if (encoded == UINT32_MAX) {
        return false;
    }
    result->execution_provenance = execution->provenance;
    result->slot = encoded;
    return true;
}
static bool ruda_mir_reference_decode(
    const RudaMirExecution *execution,
    RudaMirReference reference,
    RudaMirReferenceKind expected,
    size_t *index
) {
    RudaMirReferenceKind actual;
    if (execution == NULL || !execution->initialized
        || execution->provenance == 0
        || execution->provenance == UINT64_MAX
        || reference.execution_provenance != execution->provenance
        || reference.slot == UINT32_MAX || index == NULL) {
        return false;
    }
    actual = (RudaMirReferenceKind)(
        reference.slot >> RUDA_MIR_REFERENCE_KIND_SHIFT
    );
    if (actual != expected) {
        return false;
    }
    *index = (size_t)(reference.slot & RUDA_MIR_REFERENCE_INDEX_MASK);
    return true;
}
static RudaMirRecordField *ruda_mir_managed_record_layout(
    RudaMirManagedRecord *record
) {
    return record != NULL ? (RudaMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
}
static const RudaMirRecordField *ruda_mir_managed_record_layout_const(
    const RudaMirManagedRecord *record
) {
    return record != NULL ? (const RudaMirRecordField *)(
        record->fields + record->field_count
    ) : NULL;
}
RudaMirExecutionOptions ruda_mir_execution_default_options(void) {
    RudaMirExecutionOptions options;
    options.max_objects = 1000000;
    options.max_fields_per_object = 100000;
    options.max_total_fields = 10000000;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
    options.max_arrays = 1000000;
    options.max_array_length = 1000000;
    options.max_total_array_elements = 10000000;
    return options;
}
bool ruda_mir_execution_init(
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaMirExecutionOptions options
) {
    uint64_t provenance = 0;
    if (execution == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || options.max_objects == 0
        || options.max_objects > RUDA_MIR_REFERENCE_INDEX_MASK
        || options.max_fields_per_object == 0
        || options.max_total_fields == 0 || options.max_strings == 0
        || options.max_strings > RUDA_MIR_REFERENCE_INDEX_MASK
        || options.max_total_string_bytes == 0
        || options.max_arrays == 0
        || options.max_arrays > RUDA_MIR_REFERENCE_INDEX_MASK
        || options.max_array_length == 0
        || options.max_total_array_elements == 0
        || !ruda_new_provenance(&provenance)) {
        return false;
    }
    memset(execution, 0, sizeof(*execution));
    if (!ruda_vector_init(
            &execution->objects,
            allocator,
            sizeof(RudaMirManagedRecord *)
        ) || !ruda_vector_init(
            &execution->enums,
            allocator,
            sizeof(RudaMirManagedEnum *)
        ) || !ruda_vector_init(
            &execution->strings,
            allocator,
            sizeof(RudaMirManagedString *)
        ) || !ruda_vector_init(
            &execution->arrays,
            allocator,
            sizeof(RudaMirManagedArray *)
        ) || !ruda_vector_init(
            &execution->interface_values,
            allocator,
            sizeof(RudaMirManagedInterface *)
        )) {
        ruda_vector_dispose(&execution->interface_values);
        ruda_vector_dispose(&execution->arrays);
        ruda_vector_dispose(&execution->strings);
        ruda_vector_dispose(&execution->enums);
        ruda_vector_dispose(&execution->objects);
        return false;
    }
    execution->allocator = allocator;
    execution->options = options;
    execution->provenance = provenance;
    execution->initialized = true;
    return true;
}
void ruda_mir_execution_dispose(RudaMirExecution *execution) {
    size_t index = 0;
    if (execution == NULL || !execution->initialized) {
        return;
    }
    for (index = 0; index < execution->objects.length; index += 1) {
        RudaMirManagedRecord **record = ruda_vector_at(
            &execution->objects,
            index
        );
        if (record != NULL && *record != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *record,
                (*record)->allocation_size
            );
            *record = NULL;
        }
    }
    for (index = 0; index < execution->enums.length; index += 1) {
        RudaMirManagedEnum **value = ruda_vector_at(
            &execution->enums,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->strings.length; index += 1) {
        RudaMirManagedString **value = ruda_vector_at(
            &execution->strings,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->arrays.length; index += 1) {
        RudaMirManagedArray **value = ruda_vector_at(
            &execution->arrays,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    for (index = 0; index < execution->interface_values.length; index += 1) {
        RudaMirManagedInterface **value = ruda_vector_at(
            &execution->interface_values,
            index
        );
        if (value != NULL && *value != NULL) {
            ruda_deallocate(
                &execution->allocator,
                *value,
                (*value)->allocation_size
            );
            *value = NULL;
        }
    }
    ruda_vector_dispose(&execution->interface_values);
    ruda_vector_dispose(&execution->arrays);
    ruda_vector_dispose(&execution->strings);
    ruda_vector_dispose(&execution->enums);
    ruda_vector_dispose(&execution->objects);
    ruda_test_output_dispose(&execution->test_output);
    memset(execution, 0, sizeof(*execution));
}
const RudaMirManagedEnum *ruda_mir_execution_enum_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedEnum *const *stored = execution != NULL
            && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_ENUM,
                &index
            )
        ? ruda_vector_at_const(&execution->enums, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
bool ruda_mir_execution_inspect_enum(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *variant,
    size_t *payload_count
) {
    const RudaMirManagedEnum *stored = ruda_mir_execution_enum_value(
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
bool ruda_mir_execution_enum_payload(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
) {
    const RudaMirManagedEnum *stored = ruda_mir_execution_enum_value(
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
const RudaMirManagedString *ruda_mir_execution_string_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedString *const *stored = execution != NULL
            && execution->initialized && value.type == RUDA_TYPE_STRING
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_STRING,
                &index
            )
        ? ruda_vector_at_const(&execution->strings, index)
        : NULL;
    return stored != NULL ? *stored : NULL;
}
RudaMirManagedArray *ruda_mir_execution_array_value_mutable(
    RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedArray **stored = execution != NULL && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_ARRAY,
                &index
            )
        ? ruda_vector_at(&execution->arrays, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
const RudaMirManagedArray *ruda_mir_execution_array_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedArray *const *stored = execution != NULL
            && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_ARRAY,
                &index
            )
        ? ruda_vector_at_const(&execution->arrays, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
const RudaMirManagedInterface *ruda_mir_execution_interface_value(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedInterface *const *stored = execution != NULL
            && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_INTERFACE,
                &index
            )
        ? ruda_vector_at_const(&execution->interface_values, index)
        : NULL;
    return stored != NULL && *stored != NULL && (*stored)->type == value.type
        ? *stored
        : NULL;
}
bool ruda_mir_execution_string_bytes(
    const RudaMirExecution *execution,
    RudaMirValue value,
    const unsigned char **bytes,
    size_t *length
) {
    const RudaMirManagedString *stored = ruda_mir_execution_string_value(
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
bool ruda_mir_execution_array_length(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *length
) {
    const RudaMirManagedArray *stored = ruda_mir_execution_array_value(
        execution,
        value
    );
    if (stored == NULL || length == NULL) {
        return false;
    }
    *length = stored->length;
    return true;
}
bool ruda_mir_execution_array_element(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
) {
    const RudaMirManagedArray *stored = ruda_mir_execution_array_value(
        execution,
        value
    );
    if (stored == NULL || result == NULL || index >= stored->length) {
        return false;
    }
    *result = stored->elements[index];
    return true;
}
bool ruda_mir_execution_array_at(
    const RudaMirExecution *execution,
    size_t ordinal,
    RudaMirValue *result
) {
    RudaMirManagedArray *const *stored = execution != NULL
            && execution->initialized
        ? ruda_vector_at_const(&execution->arrays, ordinal)
        : NULL;
    if (stored == NULL || *stored == NULL || result == NULL
        || !ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_ARRAY,
            ordinal,
            &result->as.reference
        )) {
        return false;
    }
    result->type = (*stored)->type;
    return true;
}
RudaMirManagedRecord *ruda_mir_execution_record(
    RudaMirExecution *execution,
    RudaMirReference reference
) {
    size_t index = 0;
    RudaMirManagedRecord **record = execution != NULL
            && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                reference,
                RUDA_MIR_REFERENCE_RECORD,
                &index
            )
        ? ruda_vector_at(&execution->objects, index)
        : NULL;
    return record != NULL ? *record : NULL;
}
bool ruda_mir_managed_record_matches_layout(
    const RudaMirFunction *function,
    const RudaMirRecord *layout,
    const RudaMirManagedRecord *record
) {
    const RudaMirRecordField *stored_layout =
        ruda_mir_managed_record_layout_const(record);
    size_t field_index = 0;
    if (function == NULL || layout == NULL || record == NULL
        || record->type != layout->type
        || record->field_count != layout->field_count) {
        return false;
    }
    for (field_index = 0; field_index < layout->field_count;
        field_index += 1) {
        const RudaMirRecordField *expected = ruda_mir_record_field(
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
bool ruda_mir_execution_owns_reference(
    const RudaMirExecution *execution,
    RudaMirValue value
) {
    size_t index = 0;
    RudaMirManagedRecord *const *record = execution != NULL
            && execution->initialized
            && ruda_mir_reference_decode(
                execution,
                value.as.reference,
                RUDA_MIR_REFERENCE_RECORD,
                &index
            )
        ? ruda_vector_at_const(&execution->objects, index)
        : NULL;
    return ruda_mir_execution_string_value(execution, value) != NULL
        || ruda_mir_execution_enum_value(execution, value) != NULL
        || ruda_mir_execution_array_value(execution, value) != NULL
        || ruda_mir_execution_interface_value(execution, value) != NULL
        || (record != NULL && *record != NULL && (*record)->type == value.type);
}
bool ruda_mir_type_is_managed(RudaTypeId type) {
    return type == RUDA_TYPE_STRING || type >= RUDA_TYPE_FIRST_DYNAMIC;
}
bool ruda_mir_managed_array_matches_layout(
    const RudaMirArray *layout,
    const RudaMirManagedArray *value
) {
    return layout != NULL && value != NULL && value->type == layout->type
        && value->element_type == layout->element_type;
}
static RudaMirEnumVariant *ruda_mir_managed_enum_variants(
    RudaMirManagedEnum *value
) {
    return value != NULL ? (RudaMirEnumVariant *)(
        value->payload + value->selected_payload_count
    ) : NULL;
}
static RudaMirEnumPayload *ruda_mir_managed_enum_payloads(
    RudaMirManagedEnum *value
) {
    RudaMirEnumVariant *variants = ruda_mir_managed_enum_variants(value);
    return variants != NULL ? (RudaMirEnumPayload *)(
        variants + value->variant_count
    ) : NULL;
}
bool ruda_mir_managed_enum_matches_layout(
    const RudaMirFunction *function,
    const RudaMirEnum *layout,
    const RudaMirManagedEnum *value
) {
    const RudaMirEnumVariant *stored_variants = NULL;
    const RudaMirEnumPayload *stored_payloads = NULL;
    size_t variant_index = 0;
    size_t payload_index = 0;
    if (function == NULL || layout == NULL || value == NULL
        || value->type != layout->type
        || value->variant_count != layout->variant_count
        || value->layout_payload_count != layout->payload_count
        || value->variant >= value->variant_count) {
        return false;
    }
    stored_variants = (const RudaMirEnumVariant *)(
        value->payload + value->selected_payload_count
    );
    stored_payloads = (const RudaMirEnumPayload *)(
        stored_variants + value->variant_count
    );
    for (variant_index = 0; variant_index < layout->variant_count;
        variant_index += 1) {
        const RudaMirEnumVariant *expected = ruda_mir_enum_variant(
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
        const RudaMirEnumPayload *expected = ruda_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + payload_index
        );
        if (expected == NULL
            || stored_payloads[payload_index].type != expected->type) {
            return false;
        }
    }
    {
        const RudaMirEnumVariant *selected = &stored_variants[value->variant];
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
RudaMirInterpretStatus ruda_mir_execution_new_record(
    RudaMirExecution *execution,
    RudaTypeId type,
    const RudaMirValue *fields,
    const RudaMirRecordField *layout,
    size_t field_count,
    RudaMirReference *result
) {
    RudaMirManagedRecord *record = NULL;
    size_t field_bytes = 0;
    size_t layout_bytes = 0;
    size_t payload_bytes = 0;
    size_t allocation_size = 0;
    size_t new_total_fields = 0;
    size_t object_count = 0;
    size_t field_index = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || type == RUDA_TYPE_INVALID) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (field_count != 0 && (fields == NULL || layout == NULL)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!ruda_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !ruda_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || object_count >= execution->options.max_objects
        || execution->objects.length >= RUDA_MIR_REFERENCE_INDEX_MASK
        || field_count > execution->options.max_fields_per_object
        || !ruda_size_add(
            execution->total_fields,
            field_count,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (!ruda_size_multiply(field_count, sizeof(RudaMirValue), &field_bytes)
        || !ruda_size_multiply(
            field_count,
            sizeof(RudaMirRecordField),
            &layout_bytes
        ) || !ruda_size_add(field_bytes, layout_bytes, &payload_bytes)
        || !ruda_size_add(
            offsetof(RudaMirManagedRecord, fields),
            payload_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (field_index = 0; field_index < field_count; field_index += 1) {
        if (layout[field_index].type == RUDA_TYPE_INVALID
            || fields[field_index].type != layout[field_index].type) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!ruda_vector_reserve(
            &execution->objects,
            execution->objects.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record = ruda_allocate(&execution->allocator, allocation_size);
    if (record == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    record->type = type;
    record->field_count = field_count;
    record->allocation_size = allocation_size;
    if (field_count != 0 && fields != NULL && layout != NULL) {
        memcpy(record->fields, fields, field_bytes);
        memcpy(
            ruda_mir_managed_record_layout(record),
            layout,
            layout_bytes
        );
    }
    if (!ruda_vector_push(&execution->objects, &record)) {
        ruda_deallocate(&execution->allocator, record, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_RECORD,
            execution->objects.length - 1,
            result
        )
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_OBJECT_LIMIT;
}
RudaMirInterpretStatus ruda_mir_execution_new_string(
    RudaMirExecution *execution,
    const unsigned char *bytes,
    size_t length,
    RudaMirReference *result
) {
    RudaMirManagedString *value = NULL;
    size_t allocation_size = 0;
    size_t new_total = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || execution->strings.length >= execution->options.max_strings
        || execution->strings.length >= RUDA_MIR_REFERENCE_INDEX_MASK
        || !ruda_size_add(sizeof(*value), length, &allocation_size)
        || !ruda_size_add(
            execution->total_string_bytes,
            length,
            &new_total
        ) || new_total > execution->options.max_total_string_bytes) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->allocation_size = allocation_size;
    value->length = length;
    if (length != 0) {
        memcpy(value->bytes, bytes, length);
    }
    if (!ruda_vector_push(&execution->strings, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_string_bytes = new_total;
    return ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_STRING,
            execution->strings.length - 1,
            result
        )
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_OBJECT_LIMIT;
}
RudaMirInterpretStatus ruda_mir_execution_new_enum(
    RudaMirExecution *execution,
    const RudaMirFunction *function,
    const RudaMirEnum *enum_layout,
    size_t selected_variant,
    const RudaMirValue *payload_values,
    size_t payload_value_count,
    RudaMirReference *result
) {
    const RudaMirEnumVariant *selected = ruda_mir_enum_variant(
        function,
        enum_layout,
        selected_variant
    );
    RudaMirManagedEnum *value = NULL;
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
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!ruda_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !ruda_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || !ruda_size_add(
            payload_value_count,
            enum_layout->variant_count,
            &object_slots
        ) || !ruda_size_add(
            object_slots,
            enum_layout->payload_count,
            &object_slots
        ) || object_count >= execution->options.max_objects
        || execution->enums.length >= RUDA_MIR_REFERENCE_INDEX_MASK
        || object_slots > execution->options.max_fields_per_object
        || !ruda_size_add(
            execution->total_fields,
            object_slots,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields
        || !ruda_size_multiply(
            payload_value_count,
            sizeof(RudaMirValue),
            &value_bytes
        ) || !ruda_size_multiply(
            enum_layout->variant_count,
            sizeof(RudaMirEnumVariant),
            &variant_bytes
        ) || !ruda_size_multiply(
            enum_layout->payload_count,
            sizeof(RudaMirEnumPayload),
            &payload_layout_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedEnum, payload),
            value_bytes,
            &allocation_size
        ) || !ruda_size_add(
            allocation_size,
            variant_bytes,
            &allocation_size
        ) || !ruda_size_add(
            allocation_size,
            payload_layout_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    for (index = 0; index < payload_value_count; index += 1) {
        const RudaMirEnumPayload *payload = ruda_mir_enum_payload(
            function,
            selected,
            index
        );
        if (payload == NULL || payload_values[index].type != payload->type) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
    }
    if (!ruda_vector_reserve(
            &execution->enums,
            execution->enums.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
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
        const RudaMirEnumVariant *variant = ruda_mir_enum_variant(
            function,
            enum_layout,
            index
        );
        RudaMirEnumVariant *stored = ruda_mir_managed_enum_variants(value);
        if (variant == NULL || stored == NULL
            || variant->payload_start < enum_layout->payload_start) {
            ruda_deallocate(&execution->allocator, value, allocation_size);
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        stored[index] = *variant;
        stored[index].payload_start -= enum_layout->payload_start;
    }
    if (enum_layout->payload_count != 0) {
        const RudaMirEnumPayload *layout_payloads = ruda_vector_at_const(
            &function->enum_payloads,
            enum_layout->payload_start
        );
        RudaMirEnumPayload *stored_payloads =
            ruda_mir_managed_enum_payloads(value);
        if (layout_payloads == NULL || stored_payloads == NULL) {
            ruda_deallocate(&execution->allocator, value, allocation_size);
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        memcpy(
            stored_payloads,
            layout_payloads,
            payload_layout_bytes
        );
    }
    if (!ruda_vector_push(&execution->enums, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_ENUM,
            execution->enums.length - 1,
            result
        )
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_OBJECT_LIMIT;
}
RudaMirInterpretStatus ruda_mir_execution_array_preflight(
    RudaMirExecution *execution,
    size_t length,
    size_t *new_total,
    size_t *element_bytes,
    size_t *allocation_size
) {
    size_t checked_total = 0;
    size_t checked_element_bytes = 0;
    size_t checked_allocation_size = 0;
    if (execution == NULL || !execution->initialized) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (execution->arrays.length >= execution->options.max_arrays
        || execution->arrays.length >= RUDA_MIR_REFERENCE_INDEX_MASK
        || length > execution->options.max_array_length
        || (uintmax_t)length > (uintmax_t)INT64_MAX
        || !ruda_size_add(
            execution->total_array_elements,
            length,
            &checked_total
        ) || checked_total > execution->options.max_total_array_elements
        || !ruda_size_multiply(
            length,
            sizeof(RudaMirValue),
            &checked_element_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedArray, elements),
            checked_element_bytes,
            &checked_allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
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
    return RUDA_MIR_INTERPRET_OK;
}
RudaMirInterpretStatus ruda_mir_execution_new_array(
    RudaMirExecution *execution,
    const RudaMirArray *layout,
    const RudaMirValue *elements,
    size_t length,
    RudaMirReference *result
) {
    RudaMirManagedArray *value = NULL;
    size_t element_bytes = 0;
    size_t allocation_size = 0;
    size_t new_total = 0;
    size_t index = 0;
    RudaMirInterpretStatus preflight_status;
    if (execution == NULL || !execution->initialized || layout == NULL
        || result == NULL || layout->type < RUDA_TYPE_FIRST_DYNAMIC
        || layout->element_type == RUDA_TYPE_INVALID
        || (elements == NULL && length != 0)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    preflight_status = ruda_mir_execution_array_preflight(
        execution,
        length,
        &new_total,
        &element_bytes,
        &allocation_size
    );
    if (preflight_status != RUDA_MIR_INTERPRET_OK) {
        return preflight_status;
    }
    for (index = 0; index < length; index += 1) {
        if (elements[index].type != layout->element_type) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        if (ruda_mir_type_is_managed(elements[index].type)
            && !ruda_mir_execution_owns_reference(
                execution,
                elements[index]
            )) {
            return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
        }
    }
    if (!ruda_vector_reserve(
            &execution->arrays,
            execution->arrays.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = layout->type;
    value->element_type = layout->element_type;
    value->allocation_size = allocation_size;
    value->length = length;
    if (length != 0) {
        memcpy(value->elements, elements, element_bytes);
    }
    if (!ruda_vector_push(&execution->arrays, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_array_elements = new_total;
    return ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_ARRAY,
            execution->arrays.length - 1,
            result
        )
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_OBJECT_LIMIT;
}

RudaMirInterpretStatus ruda_mir_execution_array_push(
    RudaMirExecution *execution,
    const RudaMirArray *layout,
    RudaMirValue array_value,
    RudaMirValue element
) {
    RudaMirManagedArray *array = NULL;
    size_t next_length = 0;
    size_t element_bytes = 0;
    size_t next_allocation = 0;
    size_t next_total = 0;
    RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_OK;
    if (execution == NULL || !execution->initialized || layout == NULL
        || element.type != layout->element_type) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    array = ruda_mir_execution_array_value_mutable(execution, array_value);
    if (array == NULL || !ruda_mir_managed_array_matches_layout(layout, array)) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (ruda_mir_type_is_managed(element.type)
        && !ruda_mir_execution_owns_reference(execution, element)) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (array->length >= execution->options.max_array_length
        || array->length >= (size_t)INT64_MAX
        || !ruda_size_add(array->length, 1, &next_length)
        || !ruda_size_add(
            execution->total_array_elements - array->length,
            next_length,
            &next_total
        ) || next_total > execution->options.max_total_array_elements
        || !ruda_size_multiply(
            next_length, sizeof(RudaMirValue), &element_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedArray, elements), element_bytes,
            &next_allocation
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (next_allocation > array->allocation_size) {
        RudaMirManagedArray *resized = ruda_resize(
            &execution->allocator, array, array->allocation_size,
            next_allocation
        );
        if (resized == NULL) {
            return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        array = resized;
        array->allocation_size = next_allocation;
        {
            RudaMirManagedArray **stored = ruda_vector_at(
                &execution->arrays, array_value.as.reference.slot
                    & RUDA_MIR_REFERENCE_INDEX_MASK
            );
            if (stored == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
            *stored = array;
        }
    }
    array->elements[array->length] = element;
    array->length = next_length;
    execution->total_array_elements = next_total;
    return status;
}
RudaMirInterpretStatus ruda_mir_execution_new_interface(
    RudaMirExecution *execution,
    RudaTypeId type,
    RudaMirValue payload,
    const RudaSymbolId *methods,
    size_t method_count,
    RudaMirReference *result
) {
    RudaMirManagedInterface *value = NULL;
    size_t method_bytes = 0;
    size_t allocation_size = 0;
    size_t object_count = 0;
    size_t object_slots = 0;
    size_t new_total_fields = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || type < RUDA_TYPE_FIRST_DYNAMIC
        || (methods == NULL && method_count != 0)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (!ruda_mir_type_is_managed(payload.type)
        || !ruda_mir_execution_owns_reference(execution, payload)) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (!ruda_size_add(
            execution->objects.length,
            execution->enums.length,
            &object_count
        ) || !ruda_size_add(
            object_count,
            execution->interface_values.length,
            &object_count
        ) || !ruda_size_add(method_count, 1, &object_slots)
        || object_count >= execution->options.max_objects
        || execution->interface_values.length >= RUDA_MIR_REFERENCE_INDEX_MASK
        || object_slots > execution->options.max_fields_per_object
        || !ruda_size_add(
            execution->total_fields,
            object_slots,
            &new_total_fields
        ) || new_total_fields > execution->options.max_total_fields
        || !ruda_size_multiply(
            method_count,
            sizeof(RudaSymbolId),
            &method_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedInterface, methods),
            method_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    if (!ruda_vector_reserve(
            &execution->interface_values,
            execution->interface_values.length + 1
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value = ruda_allocate(&execution->allocator, allocation_size);
    if (value == NULL) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    value->type = type;
    value->payload = payload;
    value->method_count = method_count;
    value->allocation_size = allocation_size;
    if (method_count != 0) {
        memcpy(value->methods, methods, method_bytes);
    }
    if (!ruda_vector_push(&execution->interface_values, &value)) {
        ruda_deallocate(&execution->allocator, value, allocation_size);
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    execution->total_fields = new_total_fields;
    return ruda_mir_reference_encode(
            execution,
            RUDA_MIR_REFERENCE_INTERFACE,
            execution->interface_values.length - 1,
            result
        )
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_OBJECT_LIMIT;
}
