#include "managed_internal.h"
#include "managed_array_storage_internal.h"

#include <string.h>

static MarimayManagedStatus mmay_managed_array_access_resolve_const(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    const MarimayManagedArray **array
) {
    const MarimayManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || array == NULL
        || !mmay_managed_array_descriptor_valid(expected)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *array = slot->as.array;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_array_access_resolve_mutable(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    MarimayManagedArray **array
) {
    MarimayManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || array == NULL
        || !mmay_managed_array_descriptor_valid(expected)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *array = slot->as.array;
    return MMAY_MANAGED_OK;
}

static MarimayManagedStatus mmay_managed_array_access_index(
    size_t length,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    size_t *resolved_index
) {
    if (resolved_index == NULL || (mode != MMAY_MANAGED_ARRAY_ACCESS_CHECKED
            && mode != MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (mode == MMAY_MANAGED_ARRAY_ACCESS_CHECKED
        && (index < 0 || (uint64_t)index >= (uint64_t)length)) {
        return MMAY_MANAGED_BOUNDS;
    }
    /* IN_BOUNDS is a verifier-owned capability, not a second runtime check. */
    *resolved_index = (size_t)index;
    return MMAY_MANAGED_OK;
}

static const MarimayManagedArray *mmay_managed_array_view_resolve_const(
    const MarimayManagedArrayView *view
) {
    const MarimayManagedArray *array = view != NULL ? view->object : NULL;
    return array != NULL && view->descriptor != NULL
        && array->descriptor == view->descriptor ? array : NULL;
}

static MarimayManagedArray *mmay_managed_array_view_resolve(
    const MarimayManagedArrayView *view
) {
    return (MarimayManagedArray *)mmay_managed_array_view_resolve_const(view);
}

static MarimayManagedStatus mmay_managed_array_child_object(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    MarimayManagedReference reference,
    void **object
) {
    MarimayManagedSlot *slot = NULL;
    if (object != NULL) *object = NULL;
    if (object == NULL || descriptor == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != descriptor->reference_kind) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (slot->kind == MMAY_MANAGED_OBJECT_ARRAY) {
        if (slot->as.array == NULL
            || slot->as.array->descriptor
                != descriptor->reference_descriptor) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *object = slot->as.array;
        return MMAY_MANAGED_OK;
    }
    if (slot->kind == MMAY_MANAGED_OBJECT_RECORD) {
        if (slot->as.record == NULL
            || slot->as.record->descriptor
                != descriptor->reference_descriptor) {
            return MMAY_MANAGED_TYPE_MISMATCH;
        }
        *object = slot->as.record;
        return MMAY_MANAGED_OK;
    }
    return MMAY_MANAGED_TYPE_MISMATCH;
}

MarimayManagedStatus mmay_managed_array_generated_cache_children(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedArrayGeneratedView *view
) {
    MarimayManagedSlot *slot = NULL;
    MarimayManagedArray *array = NULL;
    MarimayManagedArrayGeneratedChildren *resized = NULL;
    size_t old_bytes = 0, new_bytes = 0, object_bytes = 0;
    size_t added_bytes = 0;
    size_t next_live_bytes = 0, index = 0;
    MarimayManagedStatus status;
    if (view == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    if (!mmay_managed_heap_valid(heap) || view->object == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) return MMAY_MANAGED_INVALID_REFERENCE;
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY
        || slot->as.array != view->object) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    array = slot->as.array;
    if (!mmay_managed_array_descriptor_valid(array->descriptor)
        || array->descriptor->storage_kind
            != MMAY_MANAGED_ARRAY_STORAGE_CELL
        || (array->descriptor->reference_kind != MMAY_MANAGED_OBJECT_ARRAY
            && array->descriptor->reference_kind
                != MMAY_MANAGED_OBJECT_RECORD)) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (array->generated_children != NULL
        && array->generated_children->generation
            == array->mutation_generation
        && array->generated_children->count == array->length) {
        return MMAY_MANAGED_OK;
    }
    if (array->length == 0) return MMAY_MANAGED_OK;
    if (array->generated_children == NULL
        || array->generated_children->capacity < array->length) {
        old_bytes = mmay_managed_array_child_cache_bytes(array);
        if (!mmay_size_multiply(
                array->length,
                sizeof(*array->generated_children->entries), &object_bytes
            ) || !mmay_size_add(
                sizeof(*array->generated_children), object_bytes, &new_bytes
            )) return MMAY_MANAGED_LIMIT_REACHED;
        added_bytes = new_bytes - old_bytes;
        if (!mmay_managed_add_within_limit(
                heap->live_byte_count, added_bytes,
                heap->options.max_live_bytes, &next_live_bytes
            )) return mmay_managed_resource_budget_failure(
                heap, added_bytes
            );
        resized = mmay_resize(
            &heap->allocator, array->generated_children,
            old_bytes, new_bytes
        );
        if (resized == NULL) {
            return mmay_managed_resource_host_failure(heap, new_bytes);
        }
        memset((unsigned char *)resized + old_bytes, 0,
            new_bytes - old_bytes);
        array->generated_children = resized;
        array->generated_children->capacity = array->length;
        heap->live_byte_count = next_live_bytes;
    }
    for (index = 0; index < array->length; index += 1) {
        MarimayManagedCell child;
        void *object = NULL;
        mmay_managed_array_storage_read_unchecked(array, index, &child);
        status = mmay_managed_array_child_object(
            heap, array->descriptor, child.reference, &object
        );
        if (status != MMAY_MANAGED_OK) return status;
        array->generated_children->entries[index].object = object;
        if (array->descriptor->reference_kind
            == MMAY_MANAGED_OBJECT_ARRAY) {
            array->generated_children->entries[index].array_snapshot =
                *(MarimayManagedArray *)object;
        }
    }
    array->generated_children->count = array->length;
    array->generated_children->generation = array->mutation_generation;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    MarimayManagedArrayView *view
) {
    MarimayManagedArray *array = NULL;
    MarimayManagedStatus status;
    if (view == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    memset(view, 0, sizeof(*view));
    status = mmay_managed_array_access_resolve_mutable(
        heap, reference, expected, &array
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    view->reference = reference;
    view->descriptor = expected;
    view->object = array;
    view->length = &array->length;
    view->capacity = &array->capacity;
    view->mutation_generation = &array->mutation_generation;
    view->storage = &array->storage;
    heap->cached_array_reference = reference;
    heap->cached_array_descriptor = expected;
    heap->cached_array_object = view->object;
    heap->cached_array_length = view->length;
    heap->cached_array_capacity = view->capacity;
    heap->cached_array_mutation_generation = view->mutation_generation;
    heap->cached_array_storage = view->storage;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_length(
    const MarimayManagedArrayView *view,
    size_t *length
) {
    const MarimayManagedArray *array = mmay_managed_array_view_resolve_const(view);
    if (length != NULL) {
        *length = 0;
    }
    if (array == NULL || length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *length = array->length;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_require(
    const MarimayManagedArrayView *view,
    int64_t index,
    size_t *current_length
) {
    const MarimayManagedArray *array = mmay_managed_array_view_resolve_const(view);
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (array == NULL || current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    return mmay_managed_array_access_index(
        array->length, index, MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index
    );
}

MarimayManagedStatus mmay_managed_array_view_get(
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell *result,
    size_t *current_length
) {
    const MarimayManagedArray *array = mmay_managed_array_view_resolve_const(view);
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (array == NULL || result == NULL || current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    status = mmay_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    mmay_managed_array_storage_read_unchecked(array, resolved_index, result);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_set(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell value,
    size_t *current_length
) {
    MarimayManagedArray *array = mmay_managed_array_view_resolve(view);
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (!mmay_managed_heap_valid(heap) || array == NULL
        || current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    status = mmay_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_validate_value(heap, array, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_check_mutation_commit(array);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    mmay_managed_array_storage_write_unchecked(array, resolved_index, value);
    mmay_managed_array_commit_mutation(array);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_push(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayView *view,
    MarimayManagedCell value
) {
    MarimayManagedArray *array = mmay_managed_array_view_resolve(view);
    MarimayManagedStatus status;
    size_t next_length = 0;
    if (!mmay_managed_heap_valid(heap) || array == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    status = mmay_managed_array_validate_value(heap, array, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (array->length >= mmay_managed_language_max_array_length()
        || !mmay_size_add(array->length, 1, &next_length)) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if (next_length > heap->options.max_array_length) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    status = mmay_managed_array_check_mutation_commit(array);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (next_length > array->capacity) {
        status = mmay_managed_array_ensure_capacity(
            heap, view->reference, next_length, &value, &array
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    mmay_managed_array_storage_write_unchecked(array, array->length, value);
    array->length = next_length;
    mmay_managed_array_commit_mutation(array);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_view_pop(
    const MarimayManagedArrayView *view,
    MarimayManagedCell *result,
    bool *popped
) {
    MarimayManagedArray *array = mmay_managed_array_view_resolve(view);
    MarimayManagedStatus status;
    size_t index = 0;
    if (result != NULL) memset(result, 0, sizeof(*result));
    if (popped != NULL) *popped = false;
    if (array == NULL || result == NULL || popped == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (array->length == 0) {
        return MMAY_MANAGED_OK;
    }
    status = mmay_managed_array_check_mutation_commit(array);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    index = array->length - 1;
    mmay_managed_array_storage_read_unchecked(array, index, result);
    status = mmay_managed_array_storage_clear(array, index, 1);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    array->length = index;
    mmay_managed_array_commit_mutation(array);
    *popped = true;
    return MMAY_MANAGED_OK;
}


MarimayManagedStatus mmay_managed_array_access_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    size_t *length
) {
    const MarimayManagedArray *array = NULL;
    MarimayManagedStatus status;
    if (length != NULL) {
        *length = 0;
    }
    if (length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    *length = array->length;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_access_require(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    size_t *current_length
) {
    const MarimayManagedArray *array = NULL;
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    return mmay_managed_array_access_index(
        array->length, index, MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index
    );
}

MarimayManagedStatus mmay_managed_array_access_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell *result,
    size_t *current_length
) {
    const MarimayManagedArray *array = NULL;
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (result == NULL || current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    status = mmay_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    mmay_managed_array_storage_read_unchecked(array, resolved_index, result);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_access_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell value,
    size_t *current_length
) {
    MarimayManagedArray *array = NULL;
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_access_resolve_mutable(
        heap, reference, expected, &array
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    status = mmay_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_validate_value(heap, array, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_check_mutation_commit(array);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    mmay_managed_array_storage_write_unchecked(array, resolved_index, value);
    mmay_managed_array_commit_mutation(array);
    return MMAY_MANAGED_OK;
}
