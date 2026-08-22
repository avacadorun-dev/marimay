#include "managed_internal.h"

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
    if (!ruda_managed_heap_valid(heap) || expected == NULL || array == NULL) {
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
    if (!ruda_managed_heap_valid(heap) || expected == NULL || array == NULL) {
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
    *result = array->cells[resolved_index];
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
    array->cells[resolved_index] = value;
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
}
