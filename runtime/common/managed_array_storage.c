#include "managed_internal.h"
#include "managed_array_storage_internal.h"

#include <string.h>

const RudaManagedArrayDescriptor ruda_managed_array_f64_descriptor = {
    RUDA_MANAGED_CELL_RAW64,
    RUDA_MANAGED_ARRAY_STORAGE_RAW64,
    sizeof(uint64_t),
    RUDA_MANAGED_OBJECT_INVALID,
    NULL
};

const RudaManagedArrayDescriptor ruda_managed_array_array_f64_descriptor = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(RudaManagedCell),
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_managed_array_f64_descriptor
};

static size_t ruda_managed_array_kind_stride(
    RudaManagedArrayStorageKind kind
) {
    switch (kind) {
        case RUDA_MANAGED_ARRAY_STORAGE_RAW8:
            return sizeof(uint8_t);
        case RUDA_MANAGED_ARRAY_STORAGE_RAW16:
            return sizeof(uint16_t);
        case RUDA_MANAGED_ARRAY_STORAGE_RAW32:
            return sizeof(uint32_t);
        case RUDA_MANAGED_ARRAY_STORAGE_RAW64:
            return sizeof(uint64_t);
        case RUDA_MANAGED_ARRAY_STORAGE_CELL:
            return sizeof(RudaManagedCell);
        case RUDA_MANAGED_ARRAY_STORAGE_INVALID:
            break;
    }
    return 0;
}

bool ruda_managed_array_descriptor_valid(
    const RudaManagedArrayDescriptor *descriptor
) {
    size_t stride = descriptor != NULL
        ? ruda_managed_array_kind_stride(descriptor->storage_kind) : 0;
    bool raw = descriptor != NULL
        && descriptor->storage_kind >= RUDA_MANAGED_ARRAY_STORAGE_RAW8
        && descriptor->storage_kind <= RUDA_MANAGED_ARRAY_STORAGE_RAW64;
    if (descriptor == NULL || stride == 0
        || descriptor->storage_stride != stride) {
        return false;
    }
    if (raw) {
        return descriptor->cell_kind == RUDA_MANAGED_CELL_RAW64
            && descriptor->reference_kind == RUDA_MANAGED_OBJECT_INVALID
            && descriptor->reference_descriptor == NULL;
    }
    if (descriptor->storage_kind != RUDA_MANAGED_ARRAY_STORAGE_CELL
        || descriptor->cell_kind != RUDA_MANAGED_CELL_REFERENCE
        || descriptor->reference_kind == RUDA_MANAGED_OBJECT_INVALID) {
        return false;
    }
    if (descriptor->reference_kind == RUDA_MANAGED_OBJECT_STRING) {
        return descriptor->reference_descriptor == NULL;
    }
    return descriptor->reference_descriptor != NULL;
}

size_t ruda_managed_array_storage_alignment(
    const RudaManagedArrayDescriptor *descriptor
) {
    if (!ruda_managed_array_descriptor_valid(descriptor)) {
        return 0;
    }
    return descriptor->storage_kind == RUDA_MANAGED_ARRAY_STORAGE_CELL
        ? _Alignof(RudaManagedCell) : 1;
}

bool ruda_managed_array_storage_bytes_for_capacity(
    const RudaManagedArrayDescriptor *descriptor,
    size_t capacity,
    size_t *bytes
) {
    if (bytes != NULL) {
        *bytes = 0;
    }
    return bytes != NULL && ruda_managed_array_descriptor_valid(descriptor)
        && ruda_size_multiply(capacity, descriptor->storage_stride, bytes);
}

size_t ruda_managed_array_storage_bytes(const RudaManagedArray *array) {
    size_t bytes = 0;
    return array != NULL && ruda_managed_array_storage_bytes_for_capacity(
            array->descriptor, array->capacity, &bytes
        ) ? bytes : 0;
}

size_t ruda_managed_array_child_cache_bytes(const RudaManagedArray *array) {
    size_t object_bytes = 0, bytes = 0;
    return array != NULL && array->generated_children != NULL
            && ruda_size_multiply(
                array->generated_children->capacity,
                sizeof(*array->generated_children->entries), &object_bytes
            ) && ruda_size_add(
                sizeof(*array->generated_children), object_bytes, &bytes
            )
        ? bytes : 0;
}

static bool ruda_managed_array_raw_value_valid(
    const RudaManagedArrayDescriptor *descriptor,
    RudaManagedCell value
) {
    if (descriptor->storage_kind == RUDA_MANAGED_ARRAY_STORAGE_RAW8) {
        return value.f64_bits <= UINT8_MAX;
    }
    if (descriptor->storage_kind == RUDA_MANAGED_ARRAY_STORAGE_RAW16) {
        return value.f64_bits <= UINT16_MAX;
    }
    if (descriptor->storage_kind == RUDA_MANAGED_ARRAY_STORAGE_RAW32) {
        return value.f64_bits <= UINT32_MAX;
    }
    return descriptor->storage_kind == RUDA_MANAGED_ARRAY_STORAGE_RAW64;
}

RudaManagedStatus ruda_managed_array_storage_load(
    const RudaManagedArray *array,
    size_t index,
    RudaManagedCell *result
) {
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (array == NULL || result == NULL
        || !ruda_managed_array_descriptor_valid(array->descriptor)
        || index >= array->length || array->storage == NULL) {
        return array == NULL || result == NULL
            ? RUDA_MANAGED_INVALID_ARGUMENT : RUDA_MANAGED_INVALID_REFERENCE;
    }
    ruda_managed_array_storage_read_unchecked(array, index, result);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_storage_store(
    RudaManagedArray *array,
    size_t index,
    RudaManagedCell value
) {
    if (array == NULL || !ruda_managed_array_descriptor_valid(array->descriptor)
        || index >= array->capacity || array->storage == NULL) {
        return array == NULL
            ? RUDA_MANAGED_INVALID_ARGUMENT : RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (array->descriptor->cell_kind == RUDA_MANAGED_CELL_RAW64
        && !ruda_managed_array_raw_value_valid(array->descriptor, value)) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    ruda_managed_array_storage_write_unchecked(array, index, value);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_storage_clear(
    RudaManagedArray *array,
    size_t start,
    size_t count
) {
    size_t end = 0;
    size_t bytes = 0;
    if (array == NULL || !ruda_managed_array_descriptor_valid(array->descriptor)
        || !ruda_size_add(start, count, &end) || end > array->capacity
        || !ruda_size_multiply(
            count, array->descriptor->storage_stride, &bytes
        ) || (bytes != 0 && array->storage == NULL)) {
        return array == NULL
            ? RUDA_MANAGED_INVALID_ARGUMENT : RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (bytes != 0) {
        memset(
            array->storage + start * array->descriptor->storage_stride,
            0,
            bytes
        );
    }
    return RUDA_MANAGED_OK;
}

void ruda_managed_array_dispose(
    RudaManagedHeap *heap,
    RudaManagedArray *array
) {
    size_t storage_bytes = ruda_managed_array_storage_bytes(array);
    if (heap == NULL || array == NULL) {
        return;
    }
    ruda_deallocate(&heap->allocator, array->storage, storage_bytes);
    ruda_deallocate(&heap->allocator, array->generated_children,
        ruda_managed_array_child_cache_bytes(array));
    ruda_deallocate(&heap->allocator, array, sizeof(*array));
}

RudaManagedStatus ruda_managed_array_resolve_mutable(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    RudaManagedArray **array
) {
    RudaManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || array == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (!ruda_managed_array_descriptor_valid(slot->as.array->descriptor)) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    *array = slot->as.array;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_validate_value(
    const RudaManagedHeap *heap,
    const RudaManagedArray *array,
    RudaManagedCell value
) {
    RudaManagedFieldDescriptor element;
    if (array == NULL || !ruda_managed_array_descriptor_valid(
            array->descriptor
        )) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (array->descriptor->cell_kind == RUDA_MANAGED_CELL_RAW64) {
        return ruda_managed_array_raw_value_valid(array->descriptor, value)
            ? RUDA_MANAGED_OK : RUDA_MANAGED_TYPE_MISMATCH;
    }
    element.cell_kind = RUDA_MANAGED_CELL_REFERENCE;
    element.is_mutable = true;
    element.reference_kind = array->descriptor->reference_kind;
    element.reference_descriptor = array->descriptor->reference_descriptor;
    return ruda_managed_validate_field(heap, &element, value);
}

static RudaManagedStatus ruda_managed_array_validate_cells(
    const RudaManagedHeap *heap,
    const RudaManagedArrayDescriptor *descriptor,
    const RudaManagedCell *cells,
    size_t length
) {
    RudaManagedArray array;
    size_t index = 0;
    if (!ruda_managed_array_descriptor_valid(descriptor)
        || (cells == NULL && length != 0)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    memset(&array, 0, sizeof(array));
    array.descriptor = descriptor;
    for (index = 0; index < length; index += 1) {
        RudaManagedStatus status = ruda_managed_array_validate_value(
            heap, &array, cells[index]
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    return RUDA_MANAGED_OK;
}

static RudaManagedStatus ruda_managed_array_collect_for_cells(
    RudaManagedHeap *heap,
    const RudaManagedArrayDescriptor *descriptor,
    const RudaManagedCell *cells,
    size_t length
) {
    if (descriptor->cell_kind != RUDA_MANAGED_CELL_REFERENCE || length == 0) {
        return ruda_managed_collect_internal(heap, NULL, 0, NULL, 0);
    }
    return ruda_managed_collect_internal(heap, NULL, 0, cells, length);
}

RudaManagedStatus ruda_managed_array_new(
    RudaManagedHeap *heap,
    const RudaManagedArrayDescriptor *descriptor,
    const RudaManagedCell *initial_cells,
    size_t initial_length,
    RudaManagedReference *result
) {
    RudaManagedStatus status;
    RudaManagedArray *array = NULL;
    RudaManagedSlot *resized = NULL;
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
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    ruda_managed_resource_failure_clear(heap);
    if (initial_length > ruda_managed_language_max_array_length()) {
        return RUDA_MANAGED_LENGTH_LIMIT;
    }
    if (initial_length > heap->options.max_array_length) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    status = ruda_managed_array_validate_cells(
        heap, descriptor, initial_cells, initial_length
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
retry:
    if ((heap->options.collect_before_allocation && !collected)
        || heap->live_object_count >= heap->options.max_objects) {
        status = ruda_managed_array_collect_for_cells(
            heap, descriptor, initial_cells, initial_length
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
        collected = true;
    }
    if (heap->live_object_count >= heap->options.max_objects
        || !ruda_managed_new_slot_capacity(
            heap, &slot_capacity, &slot_added_bytes
        ) || !ruda_managed_array_storage_bytes_for_capacity(
            descriptor, initial_length, &storage_bytes
        ) || !ruda_size_add(
            sizeof(RudaManagedArray), storage_bytes, &object_bytes
        ) || !ruda_size_add(object_bytes, slot_added_bytes, &added_bytes)) {
        if (!collected) {
            status = ruda_managed_array_collect_for_cells(
                heap, descriptor, initial_cells, initial_length
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
            heap->live_byte_count, added_bytes,
            heap->options.max_live_bytes, &next_bytes
        )) {
        if (!collected) {
            status = ruda_managed_array_collect_for_cells(
                heap, descriptor, initial_cells, initial_length
            );
            if (status != RUDA_MANAGED_OK) {
                return status;
            }
            collected = true;
            goto retry;
        }
        return ruda_managed_resource_budget_failure(heap, added_bytes);
    }
    array = ruda_allocate(&heap->allocator, sizeof(*array));
    if (array == NULL) {
        return ruda_managed_resource_host_failure(heap, sizeof(*array));
    }
    memset(array, 0, sizeof(*array));
    array->descriptor = descriptor;
    array->capacity = initial_length;
    if (storage_bytes != 0) {
        array->storage = ruda_allocate(&heap->allocator, storage_bytes);
        if (array->storage == NULL) {
            ruda_deallocate(&heap->allocator, array, sizeof(*array));
            return ruda_managed_resource_host_failure(heap, storage_bytes);
        }
        memset(array->storage, 0, storage_bytes);
        for (index = 0; index < initial_length; index += 1) {
            ruda_managed_array_storage_write_unchecked(
                array, index, initial_cells[index]
            );
        }
    }
    if (slot_capacity != heap->slot_capacity) {
        size_t old_bytes = heap->slot_capacity * sizeof(RudaManagedSlot);
        size_t new_bytes = slot_capacity * sizeof(RudaManagedSlot);
        resized = ruda_resize(
            &heap->allocator, heap->slots, old_bytes, new_bytes
        );
        if (resized == NULL) {
            ruda_deallocate(&heap->allocator, array->storage, storage_bytes);
            ruda_deallocate(&heap->allocator, array, sizeof(*array));
            return ruda_managed_resource_host_failure(heap, new_bytes);
        }
        memset(
            resized + heap->slot_capacity, 0,
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
    array->length = initial_length;
    heap->slots[slot_index].kind = RUDA_MANAGED_OBJECT_ARRAY;
    heap->slots[slot_index].as.array = array;
    heap->slots[slot_index].marked = false;
    heap->slots[slot_index].retired = false;
    heap->live_object_count += 1;
    heap->live_byte_count = next_bytes;
    result->heap_provenance = heap->provenance;
    result->slot = slot_index;
    result->generation = heap->slots[slot_index].generation;
    return RUDA_MANAGED_OK;
}
