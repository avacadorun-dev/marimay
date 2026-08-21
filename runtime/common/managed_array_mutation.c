#include "managed_internal.h"

#include <limits.h>
#include <string.h>

static RudaManagedStatus ruda_managed_array_resolve_mutable(
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
    *array = slot->as.array;
    return RUDA_MANAGED_OK;
}

static RudaManagedStatus ruda_managed_array_validate_value(
    const RudaManagedHeap *heap,
    const RudaManagedArray *array,
    RudaManagedCell value
) {
    RudaManagedFieldDescriptor element;
    if (array == NULL || array->descriptor == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (array->descriptor->cell_kind != RUDA_MANAGED_CELL_REFERENCE) {
        return RUDA_MANAGED_OK;
    }
    element.cell_kind = RUDA_MANAGED_CELL_REFERENCE;
    element.is_mutable = true;
    element.reference_kind = array->descriptor->reference_kind;
    element.reference_descriptor = array->descriptor->reference_descriptor;
    return ruda_managed_validate_field(heap, &element, value);
}

static RudaManagedStatus ruda_managed_array_check_mutation_commit(
    const RudaManagedArray *array
) {
    if (array == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    return array->mutation_generation == UINT64_MAX
        ? RUDA_MANAGED_LIMIT_REACHED
        : RUDA_MANAGED_OK;
}

static void ruda_managed_array_commit_mutation(RudaManagedArray *array) {
    array->mutation_generation += UINT64_C(1);
}

static RudaManagedStatus ruda_managed_array_collect_for_growth(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArray *resolved,
    const RudaManagedCell *temporary_value
) {
    RudaManagedReference roots[2];
    size_t count = 1;
    if (resolved == NULL || resolved->descriptor == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    roots[0] = array;
    if (temporary_value != NULL
        && resolved->descriptor->cell_kind == RUDA_MANAGED_CELL_REFERENCE) {
        roots[1] = temporary_value->reference;
        count = 2;
    }
    return ruda_managed_collect_internal(heap, roots, count, NULL, 0);
}

static RudaManagedStatus ruda_managed_array_ensure_capacity(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    size_t minimum_capacity,
    const RudaManagedCell *temporary_value,
    RudaManagedArray **result
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    size_t next_capacity = 0;
    size_t old_bytes = 0;
    size_t new_bytes = 0;
    size_t added_bytes = 0;
    size_t next_live_bytes = 0;
    RudaManagedCell *resized = NULL;
    bool collected = false;
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (minimum_capacity <= resolved->capacity) {
        *result = resolved;
        return RUDA_MANAGED_OK;
    }
    if (minimum_capacity > heap->options.max_array_length) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
retry:
    if (heap->options.collect_before_allocation && !collected) {
        status = ruda_managed_array_collect_for_growth(
            heap, array, resolved, temporary_value
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
        collected = true;
        status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    next_capacity = resolved->capacity == 0 ? 4 : resolved->capacity;
    while (next_capacity < minimum_capacity) {
        size_t doubled = 0;
        if (!ruda_size_multiply(next_capacity, 2, &doubled)
            || doubled > heap->options.max_array_length) {
            next_capacity = heap->options.max_array_length;
            break;
        }
        next_capacity = doubled;
    }
    if (next_capacity < minimum_capacity
        || !ruda_size_multiply(
            resolved->capacity, sizeof(RudaManagedCell), &old_bytes
        ) || !ruda_size_multiply(
            next_capacity, sizeof(RudaManagedCell), &new_bytes
        )) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    added_bytes = new_bytes - old_bytes;
    if (!ruda_managed_add_within_limit(
            heap->live_byte_count,
            added_bytes,
            heap->options.max_live_bytes,
            &next_live_bytes
        )) {
        if (!collected) {
            status = ruda_managed_array_collect_for_growth(
                heap, array, resolved, temporary_value
            );
            if (status != RUDA_MANAGED_OK) {
                return status;
            }
            collected = true;
            status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
            if (status != RUDA_MANAGED_OK) {
                return status;
            }
            goto retry;
        }
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    resized = ruda_resize(&heap->allocator, resolved->cells, old_bytes, new_bytes);
    if (resized == NULL) {
        return RUDA_MANAGED_ALLOCATION_FAILED;
    }
    resolved->cells = resized;
    resolved->capacity = next_capacity;
    heap->live_byte_count = next_live_bytes;
    *result = resolved;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_set(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t index,
    RudaManagedCell value,
    size_t *current_length
) {
    const RudaManagedArray *resolved_const = NULL;
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status = ruda_managed_array_index(
        heap, array, index, &resolved_const, current_length
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_validate_value(heap, resolved, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_check_mutation_commit(resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    (void)resolved_const;
    resolved->cells[(size_t)index] = value;
    ruda_managed_array_commit_mutation(resolved);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_push(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    RudaManagedCell value
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    size_t next_length = 0;
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_validate_value(heap, resolved, value);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (resolved->length >= ruda_managed_language_max_array_length()
        || !ruda_size_add(resolved->length, 1, &next_length)) {
        return RUDA_MANAGED_LENGTH_LIMIT;
    }
    if (next_length > heap->options.max_array_length) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    status = ruda_managed_array_check_mutation_commit(resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    status = ruda_managed_array_ensure_capacity(
        heap, array, next_length, &value, &resolved
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    resolved->cells[resolved->length] = value;
    resolved->length = next_length;
    ruda_managed_array_commit_mutation(resolved);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_mutation_generation(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    uint64_t *generation
) {
    const RudaManagedSlot *slot = NULL;
    if (generation != NULL) {
        *generation = 0;
    }
    if (!ruda_managed_heap_valid(heap) || generation == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *generation = slot->as.array->mutation_generation;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_check_mutation_generation(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    uint64_t expected_generation
) {
    uint64_t actual_generation = 0;
    RudaManagedStatus status = ruda_managed_array_mutation_generation(
        heap, array, &actual_generation
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    return actual_generation == expected_generation
        ? RUDA_MANAGED_OK
        : RUDA_MANAGED_ITERATOR_INVALIDATED;
}

static void ruda_managed_array_remove_suffix(
    RudaManagedArray *array,
    size_t new_length
) {
    size_t removed = array->length - new_length;
    memset(array->cells + new_length, 0, removed * sizeof(*array->cells));
    array->length = new_length;
    ruda_managed_array_commit_mutation(array);
}

RudaManagedStatus ruda_managed_array_pop(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    RudaManagedCell *result,
    bool *popped
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (popped != NULL) {
        *popped = false;
    }
    if (result == NULL || popped == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK || resolved->length == 0) {
        return status;
    }
    status = ruda_managed_array_check_mutation_commit(resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    *result = resolved->cells[resolved->length - 1];
    ruda_managed_array_remove_suffix(resolved, resolved->length - 1);
    *popped = true;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_truncate(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t new_length
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status = ruda_managed_array_resolve_mutable(
        heap, array, &resolved
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (new_length < 0 || (uint64_t)new_length > (uint64_t)resolved->length) {
        return RUDA_MANAGED_TRUNCATE_INVALID_LENGTH;
    }
    if ((size_t)new_length == resolved->length) {
        return RUDA_MANAGED_OK;
    }
    status = ruda_managed_array_check_mutation_commit(resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_remove_suffix(resolved, (size_t)new_length);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_clear(
    RudaManagedHeap *heap,
    RudaManagedReference array
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status = ruda_managed_array_resolve_mutable(
        heap, array, &resolved
    );
    if (status != RUDA_MANAGED_OK || resolved->length == 0) {
        return status;
    }
    status = ruda_managed_array_check_mutation_commit(resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    ruda_managed_array_remove_suffix(resolved, 0);
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_reserve(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t additional
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    size_t minimum_capacity = 0;
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (additional < 0) {
        return RUDA_MANAGED_NEGATIVE_ADDITIONAL;
    }
    if ((uint64_t)additional
        > (uint64_t)ruda_managed_language_max_array_length() - resolved->length) {
        return RUDA_MANAGED_LENGTH_LIMIT;
    }
    minimum_capacity = resolved->length + (size_t)additional;
    return ruda_managed_array_ensure_capacity(
        heap, array, minimum_capacity, NULL, &resolved
    );
}
