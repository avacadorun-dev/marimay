#include "managed_internal.h"
#include "mmay/runtime/array_edit.h"
#include "managed_array_storage_internal.h"

#include <limits.h>
#include <string.h>

MarimayManagedStatus mmay_managed_array_check_mutation_commit(
    const MarimayManagedArray *array
) {
    if (array == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    return array->mutation_generation == UINT64_MAX
        ? MMAY_MANAGED_LIMIT_REACHED
        : MMAY_MANAGED_OK;
}

void mmay_managed_array_commit_mutation(MarimayManagedArray *array) {
    array->mutation_generation += UINT64_C(1);
}

MarimayManagedStatus mmay_managed_array_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t index,
    MarimayManagedCell value,
    size_t *current_length
) {
    const MarimayManagedArray *resolved_const = NULL;
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status = mmay_managed_array_index(
        heap, array, index, &resolved_const, current_length
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_validate_value(heap, resolved, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    (void)resolved_const;
    mmay_managed_array_storage_write_unchecked(
        resolved, (size_t)index, value
    );
    mmay_managed_array_commit_mutation(resolved);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_push(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    MarimayManagedCell value
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    size_t next_length = 0;
    if (mmay_managed_heap_valid(heap)) {
        mmay_managed_resource_failure_clear(heap);
    }
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    status = mmay_managed_array_validate_value(heap, resolved, value);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (resolved->length >= mmay_managed_language_max_array_length()
        || !mmay_size_add(resolved->length, 1, &next_length)) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if (next_length > heap->options.max_array_length) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (next_length > resolved->capacity) {
        status = mmay_managed_array_ensure_capacity(
            heap, array, next_length, &value, &resolved
        );
        if (status != MMAY_MANAGED_OK) {
            return status;
        }
    }
    mmay_managed_array_storage_write_unchecked(
        resolved, resolved->length, value
    );
    resolved->length = next_length;
    mmay_managed_array_commit_mutation(resolved);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    uint64_t *generation
) {
    const MarimayManagedSlot *slot = NULL;
    if (generation != NULL) {
        *generation = 0;
    }
    if (!mmay_managed_heap_valid(heap) || generation == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *generation = slot->as.array->mutation_generation;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    uint64_t expected_generation
) {
    uint64_t actual_generation = 0;
    MarimayManagedStatus status = mmay_managed_array_mutation_generation(
        heap, array, &actual_generation
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    return actual_generation == expected_generation
        ? MMAY_MANAGED_OK
        : MMAY_MANAGED_ITERATOR_INVALIDATED;
}

static MarimayManagedStatus mmay_managed_array_remove_suffix(
    MarimayManagedArray *array,
    size_t new_length
) {
    size_t removed = array->length - new_length;
    MarimayManagedStatus status = mmay_managed_array_storage_clear(
        array, new_length, removed
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    array->length = new_length;
    mmay_managed_array_commit_mutation(array);
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_pop(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    MarimayManagedCell *result,
    bool *popped
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (popped != NULL) {
        *popped = false;
    }
    if (result == NULL || popped == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK || resolved->length == 0) {
        return status;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    mmay_managed_array_storage_read_unchecked(
        resolved, resolved->length - 1, result
    );
    status = mmay_managed_array_remove_suffix(
        resolved, resolved->length - 1
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    *popped = true;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_truncate(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t new_length
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status = mmay_managed_array_resolve_mutable(
        heap, array, &resolved
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (new_length < 0 || (uint64_t)new_length > (uint64_t)resolved->length) {
        return MMAY_MANAGED_TRUNCATE_INVALID_LENGTH;
    }
    if ((size_t)new_length == resolved->length) {
        return MMAY_MANAGED_OK;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    return mmay_managed_array_remove_suffix(resolved, (size_t)new_length);
}

MarimayManagedStatus mmay_managed_array_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference array
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status = mmay_managed_array_resolve_mutable(
        heap, array, &resolved
    );
    if (status != MMAY_MANAGED_OK || resolved->length == 0) {
        return status;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    return mmay_managed_array_remove_suffix(resolved, 0);
}

MarimayManagedStatus mmay_managed_array_reserve(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t additional
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    size_t minimum_capacity = 0;
    if (mmay_managed_heap_valid(heap)) {
        mmay_managed_resource_failure_clear(heap);
    }
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (additional < 0) {
        return MMAY_MANAGED_NEGATIVE_ADDITIONAL;
    }
    if ((uint64_t)additional
        > (uint64_t)mmay_managed_language_max_array_length() - resolved->length) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    minimum_capacity = resolved->length + (size_t)additional;
    return mmay_managed_array_ensure_capacity(
        heap, array, minimum_capacity, NULL, &resolved
    );
}

MarimayManagedStatus mmay_managed_array_fill_raw64_pushes(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    size_t count,
    uint64_t bits
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    size_t final_length = 0;
    size_t index = 0;
    unsigned char byte = (unsigned char)bits;
    bool repeated_byte = true;
    if (mmay_managed_heap_valid(heap)) {
        mmay_managed_resource_failure_clear(heap);
    }
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK) return status;
    if (resolved->descriptor->storage_kind
            != MMAY_MANAGED_ARRAY_STORAGE_RAW64
        || resolved->descriptor->storage_stride != sizeof(bits)) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (count > mmay_managed_language_max_array_length() - resolved->length
        || !mmay_size_add(resolved->length, count, &final_length)) {
        return MMAY_MANAGED_LENGTH_LIMIT;
    }
    if ((uint64_t)count > UINT64_MAX - resolved->mutation_generation) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    while (resolved->capacity < final_length) {
        size_t minimum = resolved->capacity == 0
            ? resolved->length + 1 : resolved->capacity + 1;
        status = mmay_managed_array_ensure_capacity(
            heap, array, minimum, NULL, &resolved
        );
        if (status != MMAY_MANAGED_OK) return status;
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
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_remove_at(
    MarimayManagedHeap *heap, MarimayManagedReference array,
    int64_t index, size_t *current_length
) {
    MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    size_t position = 0, stride = 0, bytes = 0;
    if (current_length != NULL) *current_length = 0;
    if (current_length == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    status = mmay_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != MMAY_MANAGED_OK) return status;
    if (resolved->length > resolved->capacity
        || !mmay_managed_array_storage_bytes_for_capacity(
            resolved->descriptor, resolved->capacity, &bytes)
        || (bytes != 0 && resolved->storage == NULL)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    *current_length = resolved->length;
    if (index < 0 || (uint64_t)index >= (uint64_t)resolved->length) {
        return MMAY_MANAGED_BOUNDS;
    }
    status = mmay_managed_array_check_mutation_commit(resolved);
    if (status != MMAY_MANAGED_OK) return status;
    position = (size_t)index;
    stride = resolved->descriptor->storage_stride;
    /* Capacity multiplication above bounds every offset; no fallible work
       follows the first write. Clear the vacated cell even for raw layouts. */
    memmove(resolved->storage + position * stride,
        resolved->storage + (position + 1) * stride,
        (resolved->length - position - 1) * stride);
    memset(resolved->storage + (resolved->length - 1) * stride, 0, stride);
    resolved->length -= 1;
    mmay_managed_array_commit_mutation(resolved);
    return MMAY_MANAGED_OK;
}
