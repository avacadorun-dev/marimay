#include "managed_internal.h"

#include <limits.h>
#include <string.h>

size_t ruda_managed_string_storage_bytes(const RudaManagedString *value) {
    return value != NULL ? value->length : 0;
}
void ruda_managed_string_dispose(
    RudaManagedHeap *heap,
    RudaManagedString *value
) {
    if (heap == NULL || value == NULL) {
        return;
    }
    ruda_deallocate(&heap->allocator, value->bytes, value->length);
    ruda_deallocate(&heap->allocator, value, sizeof(*value));
}

size_t ruda_managed_record_field_bytes(const RudaManagedRecord *record) {
    size_t bytes = 0;
    return record != NULL && record->descriptor != NULL
            && ruda_size_multiply(
                record->descriptor->field_count,
                sizeof(RudaManagedCell),
                &bytes
            )
        ? bytes
        : 0;
}

void ruda_managed_record_dispose(
    RudaManagedHeap *heap,
    RudaManagedRecord *record
) {
    size_t field_bytes = ruda_managed_record_field_bytes(record);
    if (heap == NULL || record == NULL) {
        return;
    }
    ruda_deallocate(&heap->allocator, record->fields, field_bytes);
    ruda_deallocate(&heap->allocator, record, sizeof(*record));
}

size_t ruda_managed_enum_payload_bytes(const RudaManagedEnum *value) {
    const RudaManagedEnumVariantDescriptor *variant = NULL;
    size_t bytes = 0;
    if (value == NULL || value->descriptor == NULL
        || value->variant >= value->descriptor->variant_count) {
        return 0;
    }
    variant = &value->descriptor->variants[value->variant];
    return variant != NULL && ruda_size_multiply(
            variant->payload_count,
            sizeof(RudaManagedCell),
            &bytes
        )
        ? bytes
        : 0;
}

void ruda_managed_enum_dispose(
    RudaManagedHeap *heap,
    RudaManagedEnum *value
) {
    size_t payload_bytes = ruda_managed_enum_payload_bytes(value);
    if (heap == NULL || value == NULL) {
        return;
    }
    ruda_deallocate(&heap->allocator, value->payload, payload_bytes);
    ruda_deallocate(&heap->allocator, value, sizeof(*value));
}

void ruda_managed_interface_dispose(
    RudaManagedHeap *heap,
    RudaManagedInterface *value
) {
    if (heap == NULL || value == NULL) {
        return;
    }
    ruda_deallocate(&heap->allocator, value, sizeof(*value));
}

bool ruda_managed_slot_occupied(const RudaManagedSlot *slot) {
    if (slot == NULL) {
        return false;
    }
    switch (slot->kind) {
        case RUDA_MANAGED_OBJECT_ARRAY:
            return slot->as.array != NULL;
        case RUDA_MANAGED_OBJECT_STRING:
            return slot->as.string != NULL;
        case RUDA_MANAGED_OBJECT_RECORD:
            return slot->as.record != NULL;
        case RUDA_MANAGED_OBJECT_ENUM:
            return slot->as.enumeration != NULL;
        case RUDA_MANAGED_OBJECT_INTERFACE:
            return slot->as.iface != NULL;
        case RUDA_MANAGED_OBJECT_INVALID:
            break;
    }
    return false;
}

size_t ruda_managed_slot_object_bytes(const RudaManagedSlot *slot) {
    if (slot == NULL) {
        return 0;
    }
    switch (slot->kind) {
        case RUDA_MANAGED_OBJECT_ARRAY:
            return slot->as.array != NULL
                ? sizeof(RudaManagedArray)
                    + ruda_managed_array_storage_bytes(slot->as.array)
                    + ruda_managed_array_child_cache_bytes(slot->as.array)
                : 0;
        case RUDA_MANAGED_OBJECT_STRING:
            return slot->as.string != NULL
                ? sizeof(RudaManagedString)
                    + ruda_managed_string_storage_bytes(slot->as.string)
                : 0;
        case RUDA_MANAGED_OBJECT_RECORD:
            return slot->as.record != NULL
                ? sizeof(RudaManagedRecord)
                    + ruda_managed_record_field_bytes(slot->as.record)
                : 0;
        case RUDA_MANAGED_OBJECT_ENUM:
            return slot->as.enumeration != NULL
                ? sizeof(RudaManagedEnum)
                    + ruda_managed_enum_payload_bytes(slot->as.enumeration)
                : 0;
        case RUDA_MANAGED_OBJECT_INTERFACE:
            return slot->as.iface != NULL ? sizeof(RudaManagedInterface) : 0;
        case RUDA_MANAGED_OBJECT_INVALID:
            break;
    }
    return 0;
}

void ruda_managed_slot_dispose_object(
    RudaManagedHeap *heap,
    RudaManagedSlot *slot
) {
    if (heap == NULL || slot == NULL) {
        return;
    }
    switch (slot->kind) {
        case RUDA_MANAGED_OBJECT_ARRAY:
            ruda_managed_array_dispose(heap, slot->as.array);
            slot->as.array = NULL;
            break;
        case RUDA_MANAGED_OBJECT_STRING:
            ruda_managed_string_dispose(heap, slot->as.string);
            slot->as.string = NULL;
            break;
        case RUDA_MANAGED_OBJECT_RECORD:
            ruda_managed_record_dispose(heap, slot->as.record);
            slot->as.record = NULL;
            break;
        case RUDA_MANAGED_OBJECT_ENUM:
            ruda_managed_enum_dispose(heap, slot->as.enumeration);
            slot->as.enumeration = NULL;
            break;
        case RUDA_MANAGED_OBJECT_INTERFACE:
            ruda_managed_interface_dispose(heap, slot->as.iface);
            slot->as.iface = NULL;
            break;
        case RUDA_MANAGED_OBJECT_INVALID:
            break;
    }
    slot->kind = RUDA_MANAGED_OBJECT_INVALID;
}


static RudaManagedStatus ruda_managed_trace_array(
    RudaManagedHeap *heap,
    const RudaManagedArray *array,
    uint32_t *gray_head
) {
    size_t index = 0;
    for (index = 0; index < array->length; index += 1) {
        RudaManagedCell cell;
        RudaManagedStatus status = ruda_managed_array_storage_load(
            array, index, &cell
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
        status = ruda_managed_mark_reference(
            heap, cell.reference, gray_head
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_trace_slot(
    RudaManagedHeap *heap,
    RudaManagedSlot *slot,
    uint32_t *gray_head
) {
    size_t index = 0;
    if (heap == NULL || slot == NULL || gray_head == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    switch (slot->kind) {
        case RUDA_MANAGED_OBJECT_ARRAY:
            if (slot->as.array == NULL
                || !ruda_managed_array_descriptor_valid(
                    slot->as.array->descriptor
                )) {
                return RUDA_MANAGED_INVALID_REFERENCE;
            }
            if (slot->as.array->descriptor->cell_kind
                != RUDA_MANAGED_CELL_REFERENCE) {
                return RUDA_MANAGED_OK;
            }
            return ruda_managed_trace_array(
                heap, slot->as.array, gray_head
            );
        case RUDA_MANAGED_OBJECT_STRING:
            return RUDA_MANAGED_OK;
        case RUDA_MANAGED_OBJECT_RECORD:
            if (slot->as.record == NULL
                || slot->as.record->descriptor == NULL) {
                return RUDA_MANAGED_INVALID_REFERENCE;
            }
            for (index = 0; index < slot->as.record->descriptor->field_count;
                index += 1) {
                const RudaManagedFieldDescriptor *field =
                    &slot->as.record->descriptor->fields[index];
                if (field->cell_kind == RUDA_MANAGED_CELL_REFERENCE) {
                    RudaManagedStatus status = ruda_managed_mark_reference(
                        heap,
                        slot->as.record->fields[index].reference,
                        gray_head
                    );
                    if (status != RUDA_MANAGED_OK) {
                        return status;
                    }
                }
            }
            return RUDA_MANAGED_OK;
        case RUDA_MANAGED_OBJECT_ENUM: {
            const RudaManagedEnumVariantDescriptor *variant = NULL;
            if (slot->as.enumeration == NULL
                || slot->as.enumeration->descriptor == NULL
                || slot->as.enumeration->variant
                    >= slot->as.enumeration->descriptor->variant_count) {
                return RUDA_MANAGED_INVALID_REFERENCE;
            }
            variant = &slot->as.enumeration->descriptor->variants[
                slot->as.enumeration->variant
            ];
            for (index = 0; index < variant->payload_count; index += 1) {
                if (variant->payloads[index].cell_kind
                    == RUDA_MANAGED_CELL_REFERENCE) {
                    RudaManagedStatus status = ruda_managed_mark_reference(
                        heap,
                        slot->as.enumeration->payload[index].reference,
                        gray_head
                    );
                    if (status != RUDA_MANAGED_OK) {
                        return status;
                    }
                }
            }
            return RUDA_MANAGED_OK;
        }
        case RUDA_MANAGED_OBJECT_INTERFACE:
            if (slot->as.iface == NULL) {
                return RUDA_MANAGED_INVALID_REFERENCE;
            }
            return ruda_managed_mark_reference(
                heap,
                slot->as.iface->payload,
                gray_head
            );
        case RUDA_MANAGED_OBJECT_INVALID:
            break;
    }
    return RUDA_MANAGED_INVALID_REFERENCE;
}


RudaManagedStatus ruda_managed_publish_object(
    RudaManagedHeap *heap,
    RudaManagedObjectKind kind,
    void *object,
    size_t object_bytes,
    const RudaManagedReference *temporary_roots,
    size_t temporary_root_count,
    const RudaManagedCell *temporary_cells,
    size_t temporary_cell_count,
    RudaManagedReference *result
) {
    RudaManagedStatus status;
    RudaManagedSlot *resized = NULL;
    size_t slot_capacity = 0;
    size_t slot_added_bytes = 0;
    size_t added_bytes = 0;
    size_t next_bytes = 0;
    uint32_t slot_index = UINT32_MAX;
    bool collected = false;
    if (result != NULL) {
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL || object == NULL
        || kind == RUDA_MANAGED_OBJECT_INVALID) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
retry:
    if ((heap->options.collect_before_allocation && !collected)
        || heap->live_object_count >= heap->options.max_objects) {
        status = ruda_managed_collect_internal(
            heap,
            temporary_roots,
            temporary_root_count,
            temporary_cells,
            temporary_cell_count
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
        collected = true;
    }
    if (heap->live_object_count >= heap->options.max_objects
        || !ruda_managed_new_slot_capacity(
            heap,
            &slot_capacity,
            &slot_added_bytes
        ) || !ruda_size_add(object_bytes, slot_added_bytes, &added_bytes)
        ) {
        if (!collected) {
            status = ruda_managed_collect_internal(
                heap,
                temporary_roots,
                temporary_root_count,
                temporary_cells,
                temporary_cell_count
            );
            if (status != RUDA_MANAGED_OK) {
                return status;
            }
            collected = true;
            goto retry;
        }
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    if (!ruda_managed_add_within_limit(
            heap->live_byte_count,
            added_bytes,
            heap->options.max_live_bytes,
            &next_bytes
        )) {
        if (!collected) {
            status = ruda_managed_collect_internal(
                heap,
                temporary_roots,
                temporary_root_count,
                temporary_cells,
                temporary_cell_count
            );
            if (status != RUDA_MANAGED_OK) {
                return status;
            }
            collected = true;
            goto retry;
        }
        return ruda_managed_resource_budget_failure(heap, added_bytes);
    }
    if (slot_capacity != heap->slot_capacity) {
        size_t old_bytes = heap->slot_capacity * sizeof(RudaManagedSlot);
        size_t new_bytes = slot_capacity * sizeof(RudaManagedSlot);
        resized = ruda_resize(
            &heap->allocator,
            heap->slots,
            old_bytes,
            new_bytes
        );
        if (resized == NULL) {
            return ruda_managed_resource_host_failure(heap, new_bytes);
        }
        memset(
            resized + heap->slot_capacity,
            0,
            (slot_capacity - heap->slot_capacity) * sizeof(*resized)
        );
        heap->slots = resized;
        heap->slot_capacity = slot_capacity;
    }
    if (heap->free_slot != UINT32_MAX) {
        RudaManagedSlot *slot = &heap->slots[heap->free_slot];
        slot_index = heap->free_slot;
        heap->free_slot = slot->next_free;
        slot->next_free = UINT32_MAX;
    } else {
        slot_index = (uint32_t)heap->slot_count;
        heap->slot_count += 1;
        heap->slots[slot_index].generation = 1;
        heap->slots[slot_index].next_free = UINT32_MAX;
    }
    heap->slots[slot_index].kind = kind;
    switch (kind) {
        case RUDA_MANAGED_OBJECT_STRING:
            heap->slots[slot_index].as.string = object;
            break;
        case RUDA_MANAGED_OBJECT_RECORD:
            heap->slots[slot_index].as.record = object;
            break;
        case RUDA_MANAGED_OBJECT_ENUM:
            heap->slots[slot_index].as.enumeration = object;
            break;
        case RUDA_MANAGED_OBJECT_INTERFACE:
            heap->slots[slot_index].as.iface = object;
            break;
        case RUDA_MANAGED_OBJECT_ARRAY:
        case RUDA_MANAGED_OBJECT_INVALID:
            return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    heap->slots[slot_index].marked = false;
    heap->slots[slot_index].retired = false;
    heap->live_object_count += 1;
    heap->live_byte_count = next_bytes;
    result->heap_provenance = heap->provenance;
    result->slot = slot_index;
    result->generation = heap->slots[slot_index].generation;
    return RUDA_MANAGED_OK;
}

size_t ruda_managed_count_reference_fields(
    const RudaManagedFieldDescriptor *fields,
    size_t count
) {
    size_t index = 0;
    size_t references = 0;
    if (fields == NULL) {
        return 0;
    }
    for (index = 0; index < count; index += 1) {
        if (fields[index].cell_kind == RUDA_MANAGED_CELL_REFERENCE) {
            references += 1;
        }
    }
    return references;
}

bool ruda_managed_collect_reference_roots(
    const RudaManagedFieldDescriptor *fields,
    const RudaManagedCell *cells,
    size_t count,
    RudaManagedReference *roots,
    size_t root_capacity,
    size_t *root_count
) {
    size_t index = 0;
    size_t written = 0;
    if (root_count == NULL || (roots == NULL && root_capacity != 0)
        || (fields == NULL && count != 0)
        || (cells == NULL && count != 0)) {
        return false;
    }
    for (index = 0; index < count; index += 1) {
        if (fields[index].cell_kind != RUDA_MANAGED_CELL_REFERENCE) {
            continue;
        }
        if (written >= root_capacity) {
            return false;
        }
        roots[written] = cells[index].reference;
        written += 1;
    }
    *root_count = written;
    return true;
}

RudaManagedStatus ruda_managed_validate_field(
    const RudaManagedHeap *heap,
    const RudaManagedFieldDescriptor *field,
    RudaManagedCell value
) {
    const RudaManagedSlot *slot = NULL;
    if (field == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind == RUDA_MANAGED_CELL_RAW64) {
        return field->reference_kind == RUDA_MANAGED_OBJECT_INVALID
                && field->reference_descriptor == NULL
            ? RUDA_MANAGED_OK
            : RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind != RUDA_MANAGED_CELL_REFERENCE) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (ruda_managed_reference_is_invalid(value.reference)) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (field->reference_kind == RUDA_MANAGED_OBJECT_ENUM) {
        const RudaManagedEnumDescriptor *descriptor = NULL;
        if (ruda_managed_immediate_enum_decode(
                heap, value.reference, &descriptor, NULL, NULL
            )) {
            return field->reference_descriptor == descriptor
                ? RUDA_MANAGED_OK
                : RUDA_MANAGED_TYPE_MISMATCH;
        }
    }
    slot = ruda_managed_resolve_const(heap, value.reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != field->reference_kind) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (field->reference_kind == RUDA_MANAGED_OBJECT_STRING) {
        return field->reference_descriptor == NULL
            ? RUDA_MANAGED_OK
            : RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (field->reference_descriptor == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    switch (field->reference_kind) {
        case RUDA_MANAGED_OBJECT_ARRAY:
            return slot->as.array != NULL
                    && slot->as.array->descriptor == field->reference_descriptor
                ? RUDA_MANAGED_OK
                : RUDA_MANAGED_TYPE_MISMATCH;
        case RUDA_MANAGED_OBJECT_RECORD:
            return slot->as.record != NULL
                    && slot->as.record->descriptor
                        == field->reference_descriptor
                ? RUDA_MANAGED_OK
                : RUDA_MANAGED_TYPE_MISMATCH;
        case RUDA_MANAGED_OBJECT_ENUM:
            return slot->as.enumeration != NULL
                    && slot->as.enumeration->descriptor
                        == field->reference_descriptor
                ? RUDA_MANAGED_OK
                : RUDA_MANAGED_TYPE_MISMATCH;
        case RUDA_MANAGED_OBJECT_INTERFACE:
            return slot->as.iface != NULL
                    && slot->as.iface->descriptor
                        == field->reference_descriptor
                ? RUDA_MANAGED_OK
                : RUDA_MANAGED_TYPE_MISMATCH;
        case RUDA_MANAGED_OBJECT_STRING:
        case RUDA_MANAGED_OBJECT_INVALID:
            break;
    }
    return RUDA_MANAGED_TYPE_MISMATCH;
}

RudaManagedStatus ruda_managed_object_kind(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    RudaManagedObjectKind *kind
) {
    const RudaManagedSlot *slot = NULL;
    if (kind != NULL) {
        *kind = RUDA_MANAGED_OBJECT_INVALID;
    }
    if (!ruda_managed_heap_valid(heap) || kind == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        if (ruda_managed_immediate_enum_decode(
                heap, reference, NULL, NULL, NULL
            )) {
            *kind = RUDA_MANAGED_OBJECT_ENUM;
            return RUDA_MANAGED_OK;
        }
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    *kind = slot->kind;
    return RUDA_MANAGED_OK;
}
