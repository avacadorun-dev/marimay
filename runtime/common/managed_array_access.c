#include "managed_internal.h"
#include "managed_array_storage_internal.h"

#include <string.h>

static RudaManagedStatus ruda_managed_array_access_resolve_const(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    const RudaManagedArray **array
) {
    const RudaManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || array == NULL
        || !ruda_managed_array_descriptor_valid(expected)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *array = slot->as.array;
    return RUDA_MANAGED_OK;
}

static RudaManagedStatus ruda_managed_array_access_resolve_mutable(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    RudaManagedArray **array
) {
    RudaManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || array == NULL
        || !ruda_managed_array_descriptor_valid(expected)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL
        || slot->as.array->descriptor != expected) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *array = slot->as.array;
    return RUDA_MANAGED_OK;
}

static RudaManagedStatus ruda_managed_array_access_index(
    size_t length,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    size_t *resolved_index
) {
    if (resolved_index == NULL || (mode != RUDA_MANAGED_ARRAY_ACCESS_CHECKED
            && mode != RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (mode == RUDA_MANAGED_ARRAY_ACCESS_CHECKED
        && (index < 0 || (uint64_t)index >= (uint64_t)length)) {
        return RUDA_MANAGED_BOUNDS;
    }
    /* IN_BOUNDS is a verifier-owned capability, not a second runtime check. */
    *resolved_index = (size_t)index;
    return RUDA_MANAGED_OK;
}

static const RudaManagedArray *ruda_managed_array_view_resolve_const(
    const RudaManagedArrayView *view
) {
    const RudaManagedArray *array = view != NULL ? view->object : NULL;
    return array != NULL && view->descriptor != NULL
        && array->descriptor == view->descriptor ? array : NULL;
}

static RudaManagedArray *ruda_managed_array_view_resolve(
    const RudaManagedArrayView *view
) {
    return (RudaManagedArray *)ruda_managed_array_view_resolve_const(view);
}

static RudaManagedStatus ruda_managed_array_child_object(
    RudaManagedHeap *heap,
    const RudaManagedArrayDescriptor *descriptor,
    RudaManagedReference reference,
    void **object
) {
    RudaManagedSlot *slot = NULL;
    if (object != NULL) *object = NULL;
    if (object == NULL || descriptor == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve(heap, reference);
    if (slot == NULL) return RUDA_MANAGED_INVALID_REFERENCE;
    if (slot->kind != descriptor->reference_kind) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (slot->kind == RUDA_MANAGED_OBJECT_ARRAY) {
        if (slot->as.array == NULL
            || slot->as.array->descriptor
                != descriptor->reference_descriptor) {
            return RUDA_MANAGED_TYPE_MISMATCH;
        }
        *object = slot->as.array;
        return RUDA_MANAGED_OK;
    }
    if (slot->kind == RUDA_MANAGED_OBJECT_RECORD) {
        if (slot->as.record == NULL
            || slot->as.record->descriptor
                != descriptor->reference_descriptor) {
            return RUDA_MANAGED_TYPE_MISMATCH;
        }
        *object = slot->as.record;
        return RUDA_MANAGED_OK;
    }
    return RUDA_MANAGED_TYPE_MISMATCH;
}

RudaManagedStatus ruda_managed_array_generated_cache_children(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    RudaManagedArrayGeneratedView *view
) {
    RudaManagedSlot *slot = NULL;
    RudaManagedArray *array = NULL;
    RudaManagedArrayGeneratedChildren *resized = NULL;
    size_t old_bytes = 0, new_bytes = 0, object_bytes = 0;
    size_t added_bytes = 0;
    size_t next_live_bytes = 0, index = 0;
    RudaManagedStatus status;
    if (view == NULL) return RUDA_MANAGED_INVALID_ARGUMENT;
    if (!ruda_managed_heap_valid(heap) || view->object == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    ruda_managed_resource_failure_clear(heap);
    slot = ruda_managed_resolve(heap, reference);
    if (slot == NULL) return RUDA_MANAGED_INVALID_REFERENCE;
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY
        || slot->as.array != view->object) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    array = slot->as.array;
    if (!ruda_managed_array_descriptor_valid(array->descriptor)
        || array->descriptor->storage_kind
            != RUDA_MANAGED_ARRAY_STORAGE_CELL
        || (array->descriptor->reference_kind != RUDA_MANAGED_OBJECT_ARRAY
            && array->descriptor->reference_kind
                != RUDA_MANAGED_OBJECT_RECORD)) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (array->generated_children != NULL
        && array->generated_children->generation
            == array->mutation_generation
        && array->generated_children->count == array->length) {
        return RUDA_MANAGED_OK;
    }
    if (array->length == 0) return RUDA_MANAGED_OK;
    if (array->generated_children == NULL
        || array->generated_children->capacity < array->length) {
        old_bytes = ruda_managed_array_child_cache_bytes(array);
        if (!ruda_size_multiply(
                array->length,
                sizeof(*array->generated_children->entries), &object_bytes
            ) || !ruda_size_add(
                sizeof(*array->generated_children), object_bytes, &new_bytes
            )) return RUDA_MANAGED_LIMIT_REACHED;
        added_bytes = new_bytes - old_bytes;
        if (!ruda_managed_add_within_limit(
                heap->live_byte_count, added_bytes,
                heap->options.max_live_bytes, &next_live_bytes
            )) return ruda_managed_resource_budget_failure(
                heap, added_bytes
            );
        resized = ruda_resize(
            &heap->allocator, array->generated_children,
            old_bytes, new_bytes
        );
        if (resized == NULL) {
            return ruda_managed_resource_host_failure(heap, new_bytes);
        }
        memset((unsigned char *)resized + old_bytes, 0,
            new_bytes - old_bytes);
        array->generated_children = resized;
        array->generated_children->capacity = array->length;
        heap->live_byte_count = next_live_bytes;
    }
    for (index = 0; index < array->length; index += 1) {
        RudaManagedCell child;
        void *object = NULL;
        ruda_managed_array_storage_read_unchecked(array, index, &child);
        status = ruda_managed_array_child_object(
            heap, array->descriptor, child.reference, &object
        );
        if (status != RUDA_MANAGED_OK) return status;
        array->generated_children->entries[index].object = object;
        if (array->descriptor->reference_kind
            == RUDA_MANAGED_OBJECT_ARRAY) {
            array->generated_children->entries[index].array_snapshot =
                *(RudaManagedArray *)object;
        }
    }
    array->generated_children->count = array->length;
    array->generated_children->generation = array->mutation_generation;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_bind(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    RudaManagedArrayView *view
) {
    RudaManagedArray *array = NULL;
    RudaManagedStatus status;
    if (view == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    memset(view, 0, sizeof(*view));
    status = ruda_managed_array_access_resolve_mutable(
        heap, reference, expected, &array
    );
    if (status != RUDA_MANAGED_OK) {
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
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_length(
    const RudaManagedArrayView *view,
    size_t *length
) {
    const RudaManagedArray *array = ruda_managed_array_view_resolve_const(view);
    if (length != NULL) {
        *length = 0;
    }
    if (array == NULL || length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    *length = array->length;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_require(
    const RudaManagedArrayView *view,
    int64_t index,
    size_t *current_length
) {
    const RudaManagedArray *array = ruda_managed_array_view_resolve_const(view);
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (array == NULL || current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    return ruda_managed_array_access_index(
        array->length, index, RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index
    );
}

RudaManagedStatus ruda_managed_array_view_get(
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell *result,
    size_t *current_length
) {
    const RudaManagedArray *array = ruda_managed_array_view_resolve_const(view);
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (array == NULL || result == NULL || current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    status = ruda_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_storage_read_unchecked(array, resolved_index, result);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_set(
    RudaManagedHeap *heap,
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell value,
    size_t *current_length
) {
    RudaManagedArray *array = ruda_managed_array_view_resolve(view);
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (!ruda_managed_heap_valid(heap) || array == NULL
        || current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    *current_length = array->length;
    status = ruda_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_validate_value(heap, array, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_check_mutation_commit(array);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_storage_write_unchecked(array, resolved_index, value);
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_push(
    RudaManagedHeap *heap,
    const RudaManagedArrayView *view,
    RudaManagedCell value
) {
    RudaManagedArray *array = ruda_managed_array_view_resolve(view);
    RudaManagedStatus status;
    size_t next_length = 0;
    if (!ruda_managed_heap_valid(heap) || array == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    ruda_managed_resource_failure_clear(heap);
    status = ruda_managed_array_validate_value(heap, array, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (array->length >= ruda_managed_language_max_array_length()
        || !ruda_size_add(array->length, 1, &next_length)) {
        return RUDA_MANAGED_LENGTH_LIMIT;
    }
    if (next_length > heap->options.max_array_length) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    status = ruda_managed_array_check_mutation_commit(array);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (next_length > array->capacity) {
        status = ruda_managed_array_ensure_capacity(
            heap, view->reference, next_length, &value, &array
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    ruda_managed_array_storage_write_unchecked(array, array->length, value);
    array->length = next_length;
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_view_pop(
    const RudaManagedArrayView *view,
    RudaManagedCell *result,
    bool *popped
) {
    RudaManagedArray *array = ruda_managed_array_view_resolve(view);
    RudaManagedStatus status;
    size_t index = 0;
    if (result != NULL) memset(result, 0, sizeof(*result));
    if (popped != NULL) *popped = false;
    if (array == NULL || result == NULL || popped == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (array->length == 0) {
        return RUDA_MANAGED_OK;
    }
    status = ruda_managed_array_check_mutation_commit(array);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    index = array->length - 1;
    ruda_managed_array_storage_read_unchecked(array, index, result);
    status = ruda_managed_array_storage_clear(array, index, 1);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    array->length = index;
    ruda_managed_array_commit_mutation(array);
    *popped = true;
    return RUDA_MANAGED_OK;
}


RudaManagedStatus ruda_managed_array_access_length(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    size_t *length
) {
    const RudaManagedArray *array = NULL;
    RudaManagedStatus status;
    if (length != NULL) {
        *length = 0;
    }
    if (length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    *length = array->length;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_access_require(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    size_t *current_length
) {
    const RudaManagedArray *array = NULL;
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    return ruda_managed_array_access_index(
        array->length, index, RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index
    );
}

RudaManagedStatus ruda_managed_array_access_get(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell *result,
    size_t *current_length
) {
    const RudaManagedArray *array = NULL;
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (result == NULL || current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_access_resolve_const(
        heap, reference, expected, &array
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    status = ruda_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_storage_read_unchecked(array, resolved_index, result);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_access_set(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell value,
    size_t *current_length
) {
    RudaManagedArray *array = NULL;
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_access_resolve_mutable(
        heap, reference, expected, &array
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    *current_length = array->length;
    status = ruda_managed_array_access_index(
        array->length, index, mode, &resolved_index
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_validate_value(heap, array, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_check_mutation_commit(array);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_storage_write_unchecked(array, resolved_index, value);
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
}
