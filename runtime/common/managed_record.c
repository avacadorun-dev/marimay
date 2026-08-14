#include "managed_internal.h"

#include <limits.h>
#include <string.h>

RudaManagedStatus ruda_managed_record_new(
    RudaManagedHeap *heap,
    const RudaManagedRecordDescriptor *descriptor,
    const RudaManagedCell *fields,
    size_t field_count,
    RudaManagedReference *result
) {
    RudaManagedRecord *record = NULL;
    RudaManagedStatus status;
    size_t field_bytes = 0;
    size_t object_bytes = 0;
    size_t index = 0;
    if (result != NULL) {
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL || descriptor == NULL
        || descriptor->field_count != field_count
        || (descriptor->fields == NULL && field_count != 0)
        || (fields == NULL && field_count != 0)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    for (index = 0; index < field_count; index += 1) {
        status = ruda_managed_validate_field(
            heap,
            &descriptor->fields[index],
            fields[index]
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    if (!ruda_size_multiply(field_count, sizeof(RudaManagedCell), &field_bytes)
        || !ruda_size_add(sizeof(*record), field_bytes, &object_bytes)) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    record = ruda_allocate(&heap->allocator, sizeof(*record));
    if (record == NULL) {
        return RUDA_MANAGED_ALLOCATION_FAILED;
    }
    memset(record, 0, sizeof(*record));
    if (field_bytes != 0) {
        record->fields = ruda_allocate(&heap->allocator, field_bytes);
        if (record->fields == NULL) {
            ruda_deallocate(&heap->allocator, record, sizeof(*record));
            return RUDA_MANAGED_ALLOCATION_FAILED;
        }
        memcpy(record->fields, fields, field_bytes);
    }
    record->descriptor = descriptor;
    {
        size_t root_count = ruda_managed_count_reference_fields(
            descriptor->fields,
            field_count
        );
        size_t root_bytes = 0;
        RudaManagedReference *roots = NULL;
        if (root_count != 0
            && ruda_size_multiply(
                root_count,
                sizeof(RudaManagedReference),
                &root_bytes
            )) {
            roots = ruda_allocate(&heap->allocator, root_bytes);
        }
        if (root_count != 0 && (roots == NULL
                || !ruda_managed_collect_reference_roots(
                    descriptor->fields,
                    fields,
                    field_count,
                    roots,
                    root_count,
                    &root_count
                ))) {
            ruda_deallocate(&heap->allocator, roots, root_bytes);
            ruda_managed_record_dispose(heap, record);
            return RUDA_MANAGED_ALLOCATION_FAILED;
        }
        status = ruda_managed_publish_object(
            heap,
            RUDA_MANAGED_OBJECT_RECORD,
            record,
            object_bytes,
            roots,
            root_count,
            NULL,
            0,
            result
        );
        ruda_deallocate(&heap->allocator, roots, root_bytes);
    }
    if (status != RUDA_MANAGED_OK) {
        ruda_managed_record_dispose(heap, record);
    }
    return status;
}
RudaManagedStatus ruda_managed_record_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference record,
    const RudaManagedRecordDescriptor **descriptor
) {
    const RudaManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || descriptor == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, record);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_RECORD || slot->as.record == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.record->descriptor;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_record_get(
    const RudaManagedHeap *heap,
    RudaManagedReference record,
    size_t field,
    RudaManagedCell *result
) {
    const RudaManagedSlot *slot = NULL;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, record);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (field >= slot->as.record->descriptor->field_count) {
        return RUDA_MANAGED_BOUNDS;
    }
    *result = slot->as.record->fields[field];
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_record_set(
    RudaManagedHeap *heap,
    RudaManagedReference record,
    size_t field,
    RudaManagedCell value
) {
    RudaManagedSlot *slot = NULL;
    const RudaManagedFieldDescriptor *descriptor = NULL;
    RudaManagedStatus status;
    if (!ruda_managed_heap_valid(heap)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve(heap, record);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (field >= slot->as.record->descriptor->field_count) {
        return RUDA_MANAGED_BOUNDS;
    }
    descriptor = &slot->as.record->descriptor->fields[field];
    if (!descriptor->is_mutable) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    status = ruda_managed_validate_field(heap, descriptor, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    slot->as.record->fields[field] = value;
    return RUDA_MANAGED_OK;
}
