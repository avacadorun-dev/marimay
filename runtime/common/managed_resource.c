#include "managed_internal.h"

#include <string.h>

void ruda_managed_resource_failure_clear(RudaManagedHeap *heap) {
    if (heap != NULL) {
        memset(&heap->last_resource_failure, 0,
            sizeof(heap->last_resource_failure));
    }
}

RudaManagedStatus ruda_managed_resource_budget_failure(
    RudaManagedHeap *heap,
    size_t requested_bytes
) {
    ruda_managed_resource_failure_clear(heap);
    if (heap != NULL) {
        heap->last_resource_failure.classification =
            RUDA_MANAGED_RESOURCE_FAILURE_BUDGET;
        heap->last_resource_failure.live_bytes = heap->live_byte_count;
        heap->last_resource_failure.requested_bytes = requested_bytes;
        heap->last_resource_failure.limit_bytes = heap->options.max_live_bytes;
    }
    return RUDA_MANAGED_LIMIT_REACHED;
}

RudaManagedStatus ruda_managed_resource_host_failure(
    RudaManagedHeap *heap,
    size_t requested_bytes
) {
    ruda_managed_resource_failure_clear(heap);
    if (heap != NULL) {
        heap->last_resource_failure.classification =
            RUDA_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION;
        heap->last_resource_failure.live_bytes = heap->live_byte_count;
        heap->last_resource_failure.requested_bytes = requested_bytes;
        heap->last_resource_failure.limit_bytes = heap->options.max_live_bytes;
    }
    return RUDA_MANAGED_ALLOCATION_FAILED;
}

bool ruda_managed_heap_last_resource_failure(
    const RudaManagedHeap *heap,
    RudaManagedResourceFailure *failure
) {
    if (!ruda_managed_heap_valid(heap) || failure == NULL
        || heap->last_resource_failure.classification
            == RUDA_MANAGED_RESOURCE_FAILURE_NONE) {
        return false;
    }
    *failure = heap->last_resource_failure;
    return true;
}
