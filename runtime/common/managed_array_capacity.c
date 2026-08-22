#include "managed_internal.h"

#include <string.h>

bool ruda_managed_array_plan_capacity(
    size_t current_capacity,
    size_t minimum_capacity,
    size_t maximum_capacity,
    size_t *planned_capacity
) {
    size_t next_capacity = current_capacity;
    if (planned_capacity != NULL) {
        *planned_capacity = 0;
    }
    if (planned_capacity == NULL || current_capacity > maximum_capacity
        || minimum_capacity > maximum_capacity) {
        return false;
    }
    if (minimum_capacity <= current_capacity) {
        *planned_capacity = current_capacity;
        return true;
    }
    next_capacity = current_capacity == 0 ? 4 : current_capacity;
    if (next_capacity > maximum_capacity) {
        next_capacity = maximum_capacity;
    }
    while (next_capacity < minimum_capacity) {
        size_t doubled = 0;
        if (!ruda_size_multiply(next_capacity, 2, &doubled)
            || doubled > maximum_capacity) {
            next_capacity = maximum_capacity;
            break;
        }
        next_capacity = doubled;
    }
    if (next_capacity < minimum_capacity) {
        return false;
    }
    *planned_capacity = next_capacity;
    return true;
}

static RudaManagedStatus ruda_managed_array_collect_for_growth(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArray *resolved,
    const RudaManagedCell *temporary_value
) {
    RudaManagedReference roots[2];
    size_t count = 1;
    if (resolved == NULL || !ruda_managed_array_descriptor_valid(
            resolved->descriptor
        )) {
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

RudaManagedStatus ruda_managed_array_ensure_capacity(
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
    unsigned char *resized = NULL;
    bool collected = false;
    if (result != NULL) {
        *result = NULL;
    }
    if (result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_resolve_mutable(heap, array, &resolved);
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (minimum_capacity <= resolved->capacity) {
        *result = resolved;
        return RUDA_MANAGED_OK;
    }
    if (!ruda_managed_array_plan_capacity(
            resolved->capacity, minimum_capacity,
            heap->options.max_array_length, &next_capacity
        )) {
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
    if (!ruda_managed_array_plan_capacity(
            resolved->capacity, minimum_capacity,
            heap->options.max_array_length, &next_capacity
        ) || !ruda_managed_array_storage_bytes_for_capacity(
            resolved->descriptor, resolved->capacity, &old_bytes
        ) || !ruda_managed_array_storage_bytes_for_capacity(
            resolved->descriptor, next_capacity, &new_bytes
        )) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    added_bytes = new_bytes - old_bytes;
    if (!ruda_managed_add_within_limit(
            heap->live_byte_count, added_bytes,
            heap->options.max_live_bytes, &next_live_bytes
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
        return ruda_managed_resource_budget_failure(heap, added_bytes);
    }
    resized = ruda_resize(
        &heap->allocator, resolved->storage, old_bytes, new_bytes
    );
    if (resized == NULL) {
        return ruda_managed_resource_host_failure(heap, new_bytes);
    }
    memset(resized + old_bytes, 0, new_bytes - old_bytes);
    resolved->storage = resized;
    resolved->capacity = next_capacity;
    heap->live_byte_count = next_live_bytes;
    *result = resolved;
    return RUDA_MANAGED_OK;
}
