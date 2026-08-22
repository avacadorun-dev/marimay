#include "managed_internal.h"
#include "managed_array_storage_internal.h"

#include <limits.h>
#include <string.h>

RudaManagedStatus ruda_managed_array_check_mutation_commit(
    const RudaManagedArray *array
) {
    if (array == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    return array->mutation_generation == UINT64_MAX
        ? RUDA_MANAGED_LIMIT_REACHED
        : RUDA_MANAGED_OK;
}

void ruda_managed_array_commit_mutation(RudaManagedArray *array) {
    array->mutation_generation += UINT64_C(1);
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
    ruda_managed_array_storage_write_unchecked(
        resolved, (size_t)index, value
    );
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
    if (ruda_managed_heap_valid(heap)) {
        ruda_managed_resource_failure_clear(heap);
    }
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
    if (next_length > resolved->capacity) {
        status = ruda_managed_array_ensure_capacity(
            heap, array, next_length, &value, &resolved
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    ruda_managed_array_storage_write_unchecked(
        resolved, resolved->length, value
    );
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

static RudaManagedStatus ruda_managed_array_remove_suffix(
    RudaManagedArray *array,
    size_t new_length
) {
    size_t removed = array->length - new_length;
    RudaManagedStatus status = ruda_managed_array_storage_clear(
        array, new_length, removed
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    array->length = new_length;
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
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
    ruda_managed_array_storage_read_unchecked(
        resolved, resolved->length - 1, result
    );
    status = ruda_managed_array_remove_suffix(
        resolved, resolved->length - 1
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
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
    return ruda_managed_array_remove_suffix(resolved, (size_t)new_length);
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
    return ruda_managed_array_remove_suffix(resolved, 0);
}

RudaManagedStatus ruda_managed_array_reserve(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t additional
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    size_t minimum_capacity = 0;
    if (ruda_managed_heap_valid(heap)) {
        ruda_managed_resource_failure_clear(heap);
    }
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

RudaManagedStatus ruda_managed_array_fill_raw64_pushes(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    size_t count,
    uint64_t bits
) {
    RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    size_t final_length = 0;
    size_t index = 0;
    unsigned char byte = (unsigned char)bits;
    bool repeated_byte = true;
    if (ruda_managed_heap_valid(heap)) {
        ruda_managed_resource_failure_clear(heap);
    }
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) return status;
    if (resolved->descriptor->storage_kind
            != RUDA_MANAGED_ARRAY_STORAGE_RAW64
        || resolved->descriptor->storage_stride != sizeof(bits)) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (count > ruda_managed_language_max_array_length() - resolved->length
        || !ruda_size_add(resolved->length, count, &final_length)) {
        return RUDA_MANAGED_LENGTH_LIMIT;
    }
    if ((uint64_t)count > UINT64_MAX - resolved->mutation_generation) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    while (resolved->capacity < final_length) {
        size_t minimum = resolved->capacity == 0
            ? resolved->length + 1 : resolved->capacity + 1;
        status = ruda_managed_array_ensure_capacity(
            heap, array, minimum, NULL, &resolved
        );
        if (status != RUDA_MANAGED_OK) return status;
    }
    for (index = 1; index < sizeof(bits); index += 1) {
        repeated_byte = repeated_byte
            && (unsigned char)(bits >> (index * CHAR_BIT)) == byte;
    }
    if (repeated_byte) {
        memset(
            resolved->storage + resolved->length * sizeof(bits), byte,
            count * sizeof(bits)
        );
    } else {
        for (index = resolved->length; index < final_length; index += 1) {
            memcpy(
                resolved->storage + index * sizeof(bits),
                &bits, sizeof(bits)
            );
        }
    }
    resolved->length = final_length;
    resolved->mutation_generation += (uint64_t)count;
    return RUDA_MANAGED_OK;
}
