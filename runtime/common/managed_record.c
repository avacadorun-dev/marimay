#include "managed_internal.h"

#include <limits.h>
#include <string.h>

MarimayManagedStatus mmay_managed_record_view_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedRecordDescriptor *expected,
    MarimayManagedRecordView *view
) {
    MarimayManagedSlot *slot = NULL;
    if (view == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    memset(view, 0, sizeof(*view));
    if (!mmay_managed_heap_valid(heap) || expected == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor != expected) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    view->reference = reference;
    view->descriptor = expected;
    view->object = slot->as.record;
    view->fields = slot->as.record->fields;
    heap->cached_record_reference = reference;
    heap->cached_record_descriptor = expected;
    heap->cached_record_object = view->object;
    heap->cached_record_fields = view->fields;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_record_new(
    MarimayManagedHeap *heap,
    const MarimayManagedRecordDescriptor *descriptor,
    const MarimayManagedCell *fields,
    size_t field_count,
    MarimayManagedReference *result
) {
    MarimayManagedRecord *record = NULL;
    MarimayManagedStatus status;
    size_t field_bytes = 0;
    size_t object_bytes = 0;
    size_t index = 0;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL || descriptor == NULL
        || descriptor->field_count != field_count
        || (descriptor->fields == NULL && field_count != 0)
        || (fields == NULL && field_count != 0)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    for (index = 0; index < field_count; index += 1) {
        status = mmay_managed_validate_field(
            heap,
            &descriptor->fields[index],
            fields[index]
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    if (!mmay_size_multiply(field_count, sizeof(MarimayManagedCell), &field_bytes)
        || !mmay_size_add(sizeof(*record), field_bytes, &object_bytes)) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    record = mmay_allocate(&heap->allocator, object_bytes);
    if (record == NULL) {
        return mmay_managed_resource_host_failure(heap, object_bytes);
    }
    memset(record, 0, sizeof(*record));
    if (field_bytes != 0) {
        /* D055 already budgets header and fields as one object; store them
         * in one allocation so the physical layout matches the budget. */
        record->fields = (MarimayManagedCell *)(void *)(
            (unsigned char *)record + sizeof(*record)
        );
        memcpy(record->fields, fields, field_bytes);
    }
    record->descriptor = descriptor;
    {
        size_t root_count = mmay_managed_count_reference_fields(
            descriptor->fields,
            field_count
        );
        MarimayManagedReference local_roots[8];
        MarimayManagedReference *roots = local_roots;
        size_t root_bytes = 0;
        if (root_count > sizeof(local_roots) / sizeof(*local_roots)
            && mmay_size_multiply(
                root_count,
                sizeof(*roots),
                &root_bytes
            )) {
            roots = mmay_allocate(&heap->allocator, root_bytes);
        }
        if (root_count != 0 && (roots == NULL
                || !mmay_managed_collect_reference_roots(
                    descriptor->fields,
                    fields,
                    field_count,
                    roots,
                    root_count,
                    &root_count
                ))) {
            if (roots != local_roots) {
                mmay_deallocate(&heap->allocator, roots, root_bytes);
            }
            mmay_managed_record_dispose(heap, record);
            return mmay_managed_resource_host_failure(
                heap, roots == NULL ? root_bytes : 0
            );
        }
        status = mmay_managed_publish_object(
            heap,
            MMAY_MANAGED_OBJECT_RECORD,
            record,
            object_bytes,
            roots,
            root_count,
            NULL,
            0,
            result
        );
        if (roots != local_roots) {
            mmay_deallocate(&heap->allocator, roots, root_bytes);
        }
    }
    if (status != MMAY_MANAGED_OK) {
        mmay_managed_record_dispose(heap, record);
    }
    return status;
}
MarimayManagedStatus mmay_managed_record_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference record,
    const MarimayManagedRecordDescriptor **descriptor
) {
    const MarimayManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || descriptor == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, record);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_RECORD || slot->as.record == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.record->descriptor;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_record_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference record,
    size_t field,
    MarimayManagedCell *result
) {
    const MarimayManagedSlot *slot = NULL;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, record);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (field >= slot->as.record->descriptor->field_count) {
        return MMAY_MANAGED_BOUNDS;
    }
    *result = slot->as.record->fields[field];
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_record_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference record,
    size_t field,
    MarimayManagedCell value
) {
    MarimayManagedSlot *slot = NULL;
    const MarimayManagedFieldDescriptor *descriptor = NULL;
    MarimayManagedStatus status;
    if (!mmay_managed_heap_valid(heap)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve(heap, record);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_RECORD || slot->as.record == NULL
        || slot->as.record->descriptor == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (field >= slot->as.record->descriptor->field_count) {
        return MMAY_MANAGED_BOUNDS;
    }
    descriptor = &slot->as.record->descriptor->fields[field];
    if (!descriptor->is_mutable) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    status = mmay_managed_validate_field(heap, descriptor, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    slot->as.record->fields[field] = value;
    return MMAY_MANAGED_OK;
}
