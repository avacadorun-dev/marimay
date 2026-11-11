#include "managed_internal.h"

#include <limits.h>
#include <string.h>

size_t mmay_managed_string_storage_bytes(const MarimayManagedString *value) {
    return value != NULL ? value->length : 0;
}
void mmay_managed_string_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedString *value
) {
    if (heap == NULL || value == NULL) {
        return;
    }
    mmay_deallocate(&heap->allocator, value, sizeof(*value) + value->length);
}

size_t mmay_managed_record_field_bytes(const MarimayManagedRecord *record) {
    size_t bytes = 0;
    return record != NULL && record->descriptor != NULL
            && mmay_size_multiply(
                record->descriptor->field_count,
                sizeof(MarimayManagedCell),
                &bytes
            )
        ? bytes
        : 0;
}

void mmay_managed_record_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedRecord *record
) {
    if (heap == NULL || record == NULL) {
        return;
    }
    /* record_new allocates the header and the fields as one object. */
    mmay_deallocate(
        &heap->allocator,
        record,
        sizeof(*record) + mmay_managed_record_field_bytes(record)
    );
}

size_t mmay_managed_enum_payload_bytes(const MarimayManagedEnum *value) {
    const MarimayManagedEnumVariantDescriptor *variant = NULL;
    size_t bytes = 0;
    if (value == NULL || value->descriptor == NULL
        || value->variant >= value->descriptor->variant_count) {
        return 0;
    }
    variant = &value->descriptor->variants[value->variant];
    return variant != NULL && mmay_size_multiply(
            variant->payload_count,
            sizeof(MarimayManagedCell),
            &bytes
        )
        ? bytes
        : 0;
}

void mmay_managed_enum_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedEnum *value
) {
    size_t payload_bytes = mmay_managed_enum_payload_bytes(value);
    if (heap == NULL || value == NULL) {
        return;
    }
    mmay_deallocate(
        &heap->allocator, value, sizeof(*value) + payload_bytes
    );
}

void mmay_managed_interface_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedInterface *value
) {
    if (heap == NULL || value == NULL) {
        return;
    }
    mmay_deallocate(&heap->allocator, value, sizeof(*value));
}

size_t mmay_managed_slot_object_bytes(const MarimayManagedSlot *slot) {
    if (slot == NULL) {
        return 0;
    }
    switch (slot->kind) {
        case MMAY_MANAGED_OBJECT_ARRAY:
            return slot->as.array != NULL
                ? sizeof(MarimayManagedArray)
                    + mmay_managed_array_storage_bytes(slot->as.array)
                    + mmay_managed_array_child_cache_bytes(slot->as.array)
                : 0;
        case MMAY_MANAGED_OBJECT_STRING:
            return slot->as.string != NULL
                ? sizeof(MarimayManagedString)
                    + mmay_managed_string_storage_bytes(slot->as.string)
                : 0;
        case MMAY_MANAGED_OBJECT_RECORD:
            return slot->as.record != NULL
                ? sizeof(MarimayManagedRecord)
                    + mmay_managed_record_field_bytes(slot->as.record)
                : 0;
        case MMAY_MANAGED_OBJECT_ENUM:
            return slot->as.enumeration != NULL
                ? sizeof(MarimayManagedEnum)
                    + mmay_managed_enum_payload_bytes(slot->as.enumeration)
                : 0;
        case MMAY_MANAGED_OBJECT_INTERFACE:
            return slot->as.iface != NULL ? sizeof(MarimayManagedInterface) : 0;
        case MMAY_MANAGED_OBJECT_MAP:
            return slot->as.map != NULL
                ? sizeof(MarimayManagedMap)
                    + mmay_managed_map_storage_bytes(slot->as.map)
                : 0;
        case MMAY_MANAGED_OBJECT_INVALID:
            break;
    }
    return 0;
}

void mmay_managed_slot_dispose_object(
    MarimayManagedHeap *heap,
    MarimayManagedSlot *slot
) {
    if (heap == NULL || slot == NULL) {
        return;
    }
    switch (slot->kind) {
        case MMAY_MANAGED_OBJECT_ARRAY:
            mmay_managed_array_dispose(heap, slot->as.array);
            slot->as.array = NULL;
            break;
        case MMAY_MANAGED_OBJECT_STRING:
            mmay_managed_string_dispose(heap, slot->as.string);
            slot->as.string = NULL;
            break;
        case MMAY_MANAGED_OBJECT_RECORD:
            mmay_managed_record_dispose(heap, slot->as.record);
            slot->as.record = NULL;
            break;
        case MMAY_MANAGED_OBJECT_ENUM:
            mmay_managed_enum_dispose(heap, slot->as.enumeration);
            slot->as.enumeration = NULL;
            break;
        case MMAY_MANAGED_OBJECT_INTERFACE:
            mmay_managed_interface_dispose(heap, slot->as.iface);
            slot->as.iface = NULL;
            break;
        case MMAY_MANAGED_OBJECT_MAP:
            mmay_managed_map_dispose(heap, slot->as.map);
            slot->as.map = NULL;
            break;
        case MMAY_MANAGED_OBJECT_INVALID:
            break;
    }
    slot->kind = MMAY_MANAGED_OBJECT_INVALID;
}


static MarimayManagedStatus mmay_managed_trace_array(
    MarimayManagedHeap *heap,
    const MarimayManagedArray *array,
    uint32_t *gray_head
) {
    size_t index = 0;
    for (index = 0; index < array->length; index += 1) {
        MarimayManagedCell cell;
        MarimayManagedStatus status = mmay_managed_array_storage_load(
            array, index, &cell
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        status = mmay_managed_mark_reference(
            heap, cell.reference, gray_head
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_trace_slot(
    MarimayManagedHeap *heap,
    MarimayManagedSlot *slot,
    uint32_t *gray_head
) {
    size_t index = 0;
    if (heap == NULL || slot == NULL || gray_head == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    switch (slot->kind) {
        case MMAY_MANAGED_OBJECT_ARRAY:
            if (slot->as.array == NULL
                || !mmay_managed_array_descriptor_valid(
                    slot->as.array->descriptor
                )) {
                return MMAY_MANAGED_INVALID_REFERENCE;
            }
            if (slot->as.array->descriptor->cell_kind
                != MMAY_MANAGED_CELL_REFERENCE) {
                return MMAY_MANAGED_OK;
            }
            return mmay_managed_trace_array(
                heap, slot->as.array, gray_head
            );
        case MMAY_MANAGED_OBJECT_STRING:
            return MMAY_MANAGED_OK;
        case MMAY_MANAGED_OBJECT_RECORD:
            if (slot->as.record == NULL
                || slot->as.record->descriptor == NULL) {
                return MMAY_MANAGED_INVALID_REFERENCE;
            }
            for (index = 0; index < slot->as.record->descriptor->field_count;
                index += 1) {
                const MarimayManagedFieldDescriptor *field =
                    &slot->as.record->descriptor->fields[index];
                if (field->cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
                    MarimayManagedStatus status = mmay_managed_mark_reference(
                        heap,
                        slot->as.record->fields[index].reference,
                        gray_head
                    );
                    if (status != MMAY_MANAGED_OK) {
                        return status;
                    }
                }
            }
            return MMAY_MANAGED_OK;
        case MMAY_MANAGED_OBJECT_ENUM: {
            const MarimayManagedEnumVariantDescriptor *variant = NULL;
            if (slot->as.enumeration == NULL
                || slot->as.enumeration->descriptor == NULL
                || slot->as.enumeration->variant
                    >= slot->as.enumeration->descriptor->variant_count) {
                return MMAY_MANAGED_INVALID_REFERENCE;
            }
            variant = &slot->as.enumeration->descriptor->variants[
                slot->as.enumeration->variant
            ];
            for (index = 0; index < variant->payload_count; index += 1) {
                if (variant->payloads[index].cell_kind
                    == MMAY_MANAGED_CELL_REFERENCE) {
                    MarimayManagedStatus status = mmay_managed_mark_reference(
                        heap,
                        slot->as.enumeration->payload[index].reference,
                        gray_head
                    );
                    if (status != MMAY_MANAGED_OK) {
                        return status;
                    }
                }
            }
            return MMAY_MANAGED_OK;
        }
        case MMAY_MANAGED_OBJECT_INTERFACE:
            if (slot->as.iface == NULL) {
                return MMAY_MANAGED_INVALID_REFERENCE;
            }
            return mmay_managed_mark_reference(
                heap,
                slot->as.iface->payload,
                gray_head
            );
        case MMAY_MANAGED_OBJECT_MAP:
            return slot->as.map != NULL
                ? mmay_managed_map_trace(heap, slot->as.map, gray_head)
                : MMAY_MANAGED_INVALID_REFERENCE;
        case MMAY_MANAGED_OBJECT_INVALID:
            break;
    }
    return MMAY_MANAGED_INVALID_REFERENCE;
}


size_t mmay_managed_count_reference_fields(
    const MarimayManagedFieldDescriptor *fields,
    size_t count
) {
    size_t index = 0;
    size_t references = 0;
    if (fields == NULL) {
        return 0;
    }
    for (index = 0; index < count; index += 1) {
        if (fields[index].cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
            references += 1;
        }
    }
    return references;
}

bool mmay_managed_collect_reference_roots(
    const MarimayManagedFieldDescriptor *fields,
    const MarimayManagedCell *cells,
    size_t count,
    MarimayManagedReference *roots,
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
        if (fields[index].cell_kind != MMAY_MANAGED_CELL_REFERENCE) {
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

MarimayManagedStatus mmay_managed_validate_field(
    const MarimayManagedHeap *heap,
    const MarimayManagedFieldDescriptor *field,
    MarimayManagedCell value
) {
    const MarimayManagedSlot *slot = NULL;
    if (field == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        return field->reference_kind == MMAY_MANAGED_OBJECT_INVALID
                && field->reference_descriptor == NULL
            ? MMAY_MANAGED_OK
            : MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->cell_kind != MMAY_MANAGED_CELL_REFERENCE) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (mmay_managed_reference_is_invalid(value.reference)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_ENUM) {
        const MarimayManagedEnumDescriptor *descriptor = NULL;
        if (mmay_managed_immediate_enum_decode(
                heap, value.reference, &descriptor, NULL, NULL
            )) {
            return field->reference_descriptor == descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        }
    }
    slot = mmay_managed_resolve_const(heap, value.reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != field->reference_kind) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (field->reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        return field->reference_descriptor == NULL
            ? MMAY_MANAGED_OK
            : MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (field->reference_descriptor == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    switch (field->reference_kind) {
        case MMAY_MANAGED_OBJECT_ARRAY:
            return slot->as.array != NULL
                    && slot->as.array->descriptor == field->reference_descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        case MMAY_MANAGED_OBJECT_RECORD:
            return slot->as.record != NULL
                    && slot->as.record->descriptor
                        == field->reference_descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        case MMAY_MANAGED_OBJECT_ENUM:
            return slot->as.enumeration != NULL
                    && slot->as.enumeration->descriptor
                        == field->reference_descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        case MMAY_MANAGED_OBJECT_INTERFACE:
            return slot->as.iface != NULL
                    && slot->as.iface->descriptor
                        == field->reference_descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        case MMAY_MANAGED_OBJECT_MAP:
            return slot->as.map != NULL
                    && slot->as.map->descriptor
                        == field->reference_descriptor
                ? MMAY_MANAGED_OK
                : MMAY_MANAGED_TYPE_MISMATCH;
        case MMAY_MANAGED_OBJECT_STRING:
        case MMAY_MANAGED_OBJECT_INVALID:
            break;
    }
    return MMAY_MANAGED_TYPE_MISMATCH;
}

MarimayManagedStatus mmay_managed_object_kind(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedObjectKind *kind
) {
    const MarimayManagedSlot *slot = NULL;
    if (kind != NULL) {
        *kind = MMAY_MANAGED_OBJECT_INVALID;
    }
    if (!mmay_managed_heap_valid(heap) || kind == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        if (mmay_managed_immediate_enum_decode(
                heap, reference, NULL, NULL, NULL
            )) {
            *kind = MMAY_MANAGED_OBJECT_ENUM;
            return MMAY_MANAGED_OK;
        }
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    *kind = slot->kind;
    return MMAY_MANAGED_OK;
}
