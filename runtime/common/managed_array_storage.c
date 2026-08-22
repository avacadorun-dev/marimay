#include "managed_internal.h"
#include "managed_array_storage_internal.h"

#include <string.h>

const MarimayManagedArrayDescriptor mmay_managed_array_f64_descriptor = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW64,
    sizeof(uint64_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

const MarimayManagedArrayDescriptor mmay_managed_array_array_f64_descriptor = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_managed_array_f64_descriptor
};

static size_t mmay_managed_array_kind_stride(
    MarimayManagedArrayStorageKind kind
) {
    switch (kind) {
        case MMAY_MANAGED_ARRAY_STORAGE_RAW8:
            return sizeof(uint8_t);
        case MMAY_MANAGED_ARRAY_STORAGE_RAW16:
            return sizeof(uint16_t);
        case MMAY_MANAGED_ARRAY_STORAGE_RAW32:
            return sizeof(uint32_t);
        case MMAY_MANAGED_ARRAY_STORAGE_RAW64:
            return sizeof(uint64_t);
        case MMAY_MANAGED_ARRAY_STORAGE_CELL:
            return sizeof(MarimayManagedCell);
        case MMAY_MANAGED_ARRAY_STORAGE_INVALID:
            break;
    }
    return 0;
}

bool mmay_managed_array_descriptor_valid(
    const MarimayManagedArrayDescriptor *descriptor
) {
    size_t stride = descriptor != NULL
        ? mmay_managed_array_kind_stride(descriptor->storage_kind) : 0;
    bool raw = descriptor != NULL
        && descriptor->storage_kind >= MMAY_MANAGED_ARRAY_STORAGE_RAW8
        && descriptor->storage_kind <= MMAY_MANAGED_ARRAY_STORAGE_RAW64;
    if (descriptor == NULL || stride == 0
        || descriptor->storage_stride != stride) {
        return false;
    }
    if (raw) {
        return descriptor->cell_kind == MMAY_MANAGED_CELL_RAW64
            && descriptor->reference_kind == MMAY_MANAGED_OBJECT_INVALID
            && descriptor->reference_descriptor == NULL;
    }
    if (descriptor->storage_kind != MMAY_MANAGED_ARRAY_STORAGE_CELL
        || descriptor->cell_kind != MMAY_MANAGED_CELL_REFERENCE
        || descriptor->reference_kind == MMAY_MANAGED_OBJECT_INVALID) {
        return false;
    }
    if (descriptor->reference_kind == MMAY_MANAGED_OBJECT_STRING) {
        return descriptor->reference_descriptor == NULL;
    }
    return descriptor->reference_descriptor != NULL;
}

size_t mmay_managed_array_storage_alignment(
    const MarimayManagedArrayDescriptor *descriptor
) {
    if (!mmay_managed_array_descriptor_valid(descriptor)) {
        return 0;
    }
    return descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_CELL
        ? _Alignof(MarimayManagedCell) : 1;
}

bool mmay_managed_array_storage_bytes_for_capacity(
    const MarimayManagedArrayDescriptor *descriptor,
    size_t capacity,
    size_t *bytes
) {
    if (bytes != NULL) {
        *bytes = 0;
    }
    return bytes != NULL && mmay_managed_array_descriptor_valid(descriptor)
        && mmay_size_multiply(capacity, descriptor->storage_stride, bytes);
}

size_t mmay_managed_array_storage_bytes(const MarimayManagedArray *array) {
    size_t bytes = 0;
    return array != NULL && mmay_managed_array_storage_bytes_for_capacity(
            array->descriptor, array->capacity, &bytes
        ) ? bytes : 0;
}

size_t mmay_managed_array_child_cache_bytes(const MarimayManagedArray *array) {
    size_t object_bytes = 0, bytes = 0;
    return array != NULL && array->generated_children != NULL
            && mmay_size_multiply(
                array->generated_children->capacity,
                sizeof(*array->generated_children->entries), &object_bytes
            ) && mmay_size_add(
                sizeof(*array->generated_children), object_bytes, &bytes
            )
        ? bytes : 0;
}

static bool mmay_managed_array_raw_value_valid(
    const MarimayManagedArrayDescriptor *descriptor,
    MarimayManagedCell value
) {
    if (descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW8) {
        return value.f64_bits <= UINT8_MAX;
    }
    if (descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW16) {
        return value.f64_bits <= UINT16_MAX;
    }
    if (descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW32) {
        return value.f64_bits <= UINT32_MAX;
    }
    return descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64;
}

MarimayManagedStatus mmay_managed_array_storage_load(
    const MarimayManagedArray *array,
    size_t index,
    MarimayManagedCell *result
) {
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (array == NULL || result == NULL
        || !mmay_managed_array_descriptor_valid(array->descriptor)
        || index >= array->length || array->storage == NULL) {
        return array == NULL || result == NULL
            ? MMAY_MANAGED_INVALID_ARGUMENT : MMAY_MANAGED_INVALID_REFERENCE;
    }
    mmay_managed_array_storage_read_unchecked(array, index, result);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_storage_store(
    MarimayManagedArray *array,
    size_t index,
    MarimayManagedCell value
) {
    if (array == NULL || !mmay_managed_array_descriptor_valid(array->descriptor)
        || index >= array->capacity || array->storage == NULL) {
        return array == NULL
            ? MMAY_MANAGED_INVALID_ARGUMENT : MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (array->descriptor->cell_kind == MMAY_MANAGED_CELL_RAW64
        && !mmay_managed_array_raw_value_valid(array->descriptor, value)) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    mmay_managed_array_storage_write_unchecked(array, index, value);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_storage_clear(
    MarimayManagedArray *array,
    size_t start,
    size_t count
) {
    size_t end = 0;
    size_t bytes = 0;
    if (array == NULL || !mmay_managed_array_descriptor_valid(array->descriptor)
        || !mmay_size_add(start, count, &end) || end > array->capacity
        || !mmay_size_multiply(
            count, array->descriptor->storage_stride, &bytes
        ) || (bytes != 0 && array->storage == NULL)) {
        return array == NULL
            ? MMAY_MANAGED_INVALID_ARGUMENT : MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (bytes != 0) {
        memset(
            array->storage + start * array->descriptor->storage_stride,
            0,
            bytes
        );
    }
    return MMAY_MANAGED_OK;
}

void mmay_managed_array_dispose(
    MarimayManagedHeap *heap,
    MarimayManagedArray *array
) {
    size_t storage_bytes = mmay_managed_array_storage_bytes(array);
    if (heap == NULL || array == NULL) {
        return;
    }
    mmay_deallocate(&heap->allocator, array->storage, storage_bytes);
    mmay_deallocate(&heap->allocator, array->generated_children,
        mmay_managed_array_child_cache_bytes(array));
    mmay_deallocate(&heap->allocator, array, sizeof(*array));
}

MarimayManagedStatus mmay_managed_array_resolve_mutable(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedArray **array
) {
    MarimayManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || array == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (!mmay_managed_array_descriptor_valid(slot->as.array->descriptor)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    *array = slot->as.array;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_validate_value(
    const MarimayManagedHeap *heap,
    const MarimayManagedArray *array,
    MarimayManagedCell value
) {
    MarimayManagedFieldDescriptor element;
    if (array == NULL || !mmay_managed_array_descriptor_valid(
            array->descriptor
        )) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (array->descriptor->cell_kind == MMAY_MANAGED_CELL_RAW64) {
        return mmay_managed_array_raw_value_valid(array->descriptor, value)
            ? MMAY_MANAGED_OK : MMAY_MANAGED_TYPE_MISMATCH;
    }
    element.cell_kind = MMAY_MANAGED_CELL_REFERENCE;
    element.is_mutable = true;
    element.reference_kind = array->descriptor->reference_kind;
    element.reference_descriptor = array->descriptor->reference_descriptor;
    return mmay_managed_validate_field(heap, &element, value);
}

static MarimayManagedStatus mmay_managed_array_validate_cells(
    const MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    const MarimayManagedCell *cells,
    size_t length
) {
    MarimayManagedArray array;
    size_t index = 0;
    if (!mmay_managed_array_descriptor_valid(descriptor)
        || (cells == NULL && length != 0)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    memset(&array, 0, sizeof(array));
    array.descriptor = descriptor;
    for (index = 0; index < length; index += 1) {
        MarimayManagedStatus status = mmay_managed_array_validate_value(
            heap, &array, cells[index]
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_array_collect_for_cells(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    const MarimayManagedCell *cells,
    size_t length
) {
    if (descriptor->cell_kind != MMAY_MANAGED_CELL_REFERENCE || length == 0) {
        return mmay_managed_collect_internal(heap, NULL, 0, NULL, 0);
    }
    return mmay_managed_collect_internal(heap, NULL, 0, cells, length);
}

MarimayManagedStatus mmay_managed_array_new(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    const MarimayManagedCell *initial_cells,
    size_t initial_length,
    MarimayManagedReference *result
) {
    MarimayManagedStatus status;
    MarimayManagedArray *array = NULL;
    MarimayManagedSlot *resized = NULL;
    size_t storage_bytes = 0;
    size_t slot_capacity = 0;
    size_t slot_added_bytes = 0;
    size_t object_bytes = 0;
    size_t added_bytes = 0;
    size_t next_bytes = 0;
    size_t index = 0;
    uint32_t slot_index = UINT32_MAX;
    bool collected = false;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    if (initial_length > mmay_managed_language_max_array_length()) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if (initial_length > heap->options.max_array_length) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    status = mmay_managed_array_validate_cells(
        heap, descriptor, initial_cells, initial_length
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
retry:
    if ((heap->options.collect_before_allocation && !collected)
        || heap->live_object_count >= heap->options.max_objects) {
        status = mmay_managed_array_collect_for_cells(
            heap, descriptor, initial_cells, initial_length
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
        collected = true;
    }
    if (heap->live_object_count >= heap->options.max_objects
        || !mmay_managed_new_slot_capacity(
            heap, &slot_capacity, &slot_added_bytes
        ) || !mmay_managed_array_storage_bytes_for_capacity(
            descriptor, initial_length, &storage_bytes
        ) || !mmay_size_add(
            sizeof(MarimayManagedArray), storage_bytes, &object_bytes
        ) || !mmay_size_add(object_bytes, slot_added_bytes, &added_bytes)) {
        if (!collected) {
            status = mmay_managed_array_collect_for_cells(
                heap, descriptor, initial_cells, initial_length
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
            collected = true;
            goto retry;
        }
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    if (!mmay_managed_add_within_limit(
            heap->live_byte_count, added_bytes,
            heap->options.max_live_bytes, &next_bytes
        )) {
        if (!collected) {
            status = mmay_managed_array_collect_for_cells(
                heap, descriptor, initial_cells, initial_length
            );
            if (status != MMAY_MANAGED_OK) {
                return status;
            }
            collected = true;
            goto retry;
        }
        return mmay_managed_resource_budget_failure(heap, added_bytes);
    }
    array = mmay_allocate(&heap->allocator, sizeof(*array));
    if (array == NULL) {
        return mmay_managed_resource_host_failure(heap, sizeof(*array));
    }
    memset(array, 0, sizeof(*array));
    array->descriptor = descriptor;
    array->capacity = initial_length;
    if (storage_bytes != 0) {
        array->storage = mmay_allocate(&heap->allocator, storage_bytes);
        if (array->storage == NULL) {
            mmay_deallocate(&heap->allocator, array, sizeof(*array));
            return mmay_managed_resource_host_failure(heap, storage_bytes);
        }
        memset(array->storage, 0, storage_bytes);
        for (index = 0; index < initial_length; index += 1) {
            mmay_managed_array_storage_write_unchecked(
                array, index, initial_cells[index]
            );
        }
    }
    if (slot_capacity != heap->slot_capacity) {
        size_t old_bytes = heap->slot_capacity * sizeof(MarimayManagedSlot);
        size_t new_bytes = slot_capacity * sizeof(MarimayManagedSlot);
        resized = mmay_resize(
            &heap->allocator, heap->slots, old_bytes, new_bytes
        );
        if (resized == NULL) {
            mmay_deallocate(&heap->allocator, array->storage, storage_bytes);
            mmay_deallocate(&heap->allocator, array, sizeof(*array));
            return mmay_managed_resource_host_failure(heap, new_bytes);
        }
        memset(
            resized + heap->slot_capacity, 0,
            (slot_capacity - heap->slot_capacity) * sizeof(*resized)
        );
        heap->slots = resized;
        heap->slot_capacity = slot_capacity;
    }
    if (heap->free_slot != UINT32_MAX) {
        MarimayManagedSlot *slot = &heap->slots[heap->free_slot];
        slot_index = heap->free_slot;
        heap->free_slot = slot->next_free;
        slot->next_free = UINT32_MAX;
    } else {
        slot_index = (uint32_t)heap->slot_count;
        heap->slot_count += 1;
        heap->slots[slot_index].generation = 1;
        heap->slots[slot_index].next_free = UINT32_MAX;
    }
    array->length = initial_length;
    heap->slots[slot_index].kind = MMAY_MANAGED_OBJECT_ARRAY;
    heap->slots[slot_index].as.array = array;
    heap->slots[slot_index].marked = false;
    heap->slots[slot_index].retired = false;
    heap->live_object_count += 1;
    heap->live_byte_count = next_bytes;
    result->heap_provenance = heap->provenance;
    result->slot = slot_index;
    result->generation = heap->slots[slot_index].generation;
    return MMAY_MANAGED_OK;
}
