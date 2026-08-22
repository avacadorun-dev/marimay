#include "managed_internal.h"

#include <string.h>

void mmay_managed_resource_failure_clear(MarimayManagedHeap *heap) {
    if (heap != NULL) {
        memset(&heap->last_resource_failure, 0,
            sizeof(heap->last_resource_failure));
    }
}

MarimayManagedStatus mmay_managed_resource_budget_failure(
    MarimayManagedHeap *heap,
    size_t requested_bytes
) {
    mmay_managed_resource_failure_clear(heap);
    if (heap != NULL) {
        heap->last_resource_failure.classification =
            MMAY_MANAGED_RESOURCE_FAILURE_BUDGET;
        heap->last_resource_failure.live_bytes = heap->live_byte_count;
        heap->last_resource_failure.requested_bytes = requested_bytes;
        heap->last_resource_failure.limit_bytes = heap->options.max_live_bytes;
    }
    return MMAY_MANAGED_LIMIT_REACHED;
}

MarimayManagedStatus mmay_managed_resource_host_failure(
    MarimayManagedHeap *heap,
    size_t requested_bytes
) {
    mmay_managed_resource_failure_clear(heap);
    if (heap != NULL) {
        heap->last_resource_failure.classification =
            MMAY_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION;
        heap->last_resource_failure.live_bytes = heap->live_byte_count;
        heap->last_resource_failure.requested_bytes = requested_bytes;
        heap->last_resource_failure.limit_bytes = heap->options.max_live_bytes;
    }
    return MMAY_MANAGED_ALLOCATION_FAILED;
}

bool mmay_managed_heap_last_resource_failure(
    const MarimayManagedHeap *heap,
    MarimayManagedResourceFailure *failure
) {
    if (!mmay_managed_heap_valid(heap) || failure == NULL
        || heap->last_resource_failure.classification
            == MMAY_MANAGED_RESOURCE_FAILURE_NONE) {
        return false;
    }
    *failure = heap->last_resource_failure;
    return true;
}
