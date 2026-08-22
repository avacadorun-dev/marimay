#include "managed_internal.h"

#include <string.h>

RudaManagedStatus ruda_managed_array_get_in_bounds(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    size_t index,
    RudaManagedCell *result
) {
    const RudaManagedSlot *slot = NULL;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    /* The verifier-backed precondition makes this indexed access safe. */
    *result = slot->as.array->cells[index];
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_set_in_bounds(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    size_t index,
    RudaManagedCell value
) {
    RudaManagedArray *array = NULL;
    RudaManagedStatus status = ruda_managed_array_resolve_mutable(
        heap, reference, &array
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
    /* The verifier-backed precondition makes this indexed access safe. */
    array->cells[index] = value;
    ruda_managed_array_commit_mutation(array);
    return RUDA_MANAGED_OK;
}
