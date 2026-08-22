#ifndef RUDA_RUNTIME_ARRAY_VIEW_H
#define RUDA_RUNTIME_ARRAY_VIEW_H

#include <limits.h>
#include <string.h>

#include "ruda/runtime/array.h"

typedef enum RudaManagedArrayAccessMode {
    RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
    RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS
} RudaManagedArrayAccessMode;

/*
 * Compiler-owned non-escaping view of one rooted, non-moving array object.
 * Growth may replace element storage but does not move the array object, so
 * these field locations remain valid while `reference` is precisely rooted or
 * the generated activation is proven unable to reach a managed safepoint.
 */
typedef struct RudaManagedArrayView {
    RudaManagedReference reference;
    const RudaManagedArrayDescriptor *descriptor;
    void *object;
    size_t *length;
    uint64_t *mutation_generation;
    unsigned char **storage;
} RudaManagedArrayView;

RudaManagedStatus ruda_managed_array_view_bind(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor *expected,
    RudaManagedArrayView *view
);

static inline RudaManagedStatus ruda_managed_array_view_index_fast(
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    size_t *resolved_index,
    size_t *current_length
) {
    size_t length = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (view == NULL || view->object == NULL || view->length == NULL
        || resolved_index == NULL || current_length == NULL
        || (mode != RUDA_MANAGED_ARRAY_ACCESS_CHECKED
            && mode != RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    length = *view->length;
    *current_length = length;
    if (mode == RUDA_MANAGED_ARRAY_ACCESS_CHECKED
        && (index < 0 || (uint64_t)index >= (uint64_t)length)) {
        return RUDA_MANAGED_BOUNDS;
    }
    *resolved_index = (size_t)index;
    return RUDA_MANAGED_OK;
}

static inline RudaManagedStatus ruda_managed_array_view_length_fast(
    const RudaManagedArrayView *view,
    size_t *length
) {
    if (length != NULL) {
        *length = 0;
    }
    if (view == NULL || view->object == NULL || view->length == NULL
        || length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    *length = *view->length;
    return RUDA_MANAGED_OK;
}

static inline RudaManagedStatus ruda_managed_array_view_require_fast(
    const RudaManagedArrayView *view,
    int64_t index,
    size_t *current_length
) {
    size_t resolved_index = 0;
    return ruda_managed_array_view_index_fast(
        view, index, RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index, current_length
    );
}

static inline RudaManagedStatus ruda_managed_array_view_get_raw64_fast(
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    uint64_t *bits,
    size_t *current_length
) {
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (bits != NULL) {
        *bits = 0;
    }
    if (view == NULL || bits == NULL || view->storage == NULL
        || view->descriptor == NULL
        || view->descriptor->storage_kind != RUDA_MANAGED_ARRAY_STORAGE_RAW64
        || view->descriptor->storage_stride != sizeof(*bits)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_view_index_fast(
        view, index, mode, &resolved_index, current_length
    );
    if (status == RUDA_MANAGED_OK) {
        memcpy(bits, *view->storage + resolved_index * sizeof(*bits),
            sizeof(*bits));
    }
    return status;
}

static inline RudaManagedStatus ruda_managed_array_view_set_raw64_fast(
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    uint64_t bits,
    size_t *current_length
) {
    RudaManagedStatus status;
    size_t resolved_index = 0;
    if (view == NULL || view->storage == NULL
        || view->mutation_generation == NULL || view->descriptor == NULL
        || view->descriptor->storage_kind != RUDA_MANAGED_ARRAY_STORAGE_RAW64
        || view->descriptor->storage_stride != sizeof(bits)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_view_index_fast(
        view, index, mode, &resolved_index, current_length
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    if (*view->mutation_generation == UINT64_MAX) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    memcpy(*view->storage + resolved_index * sizeof(bits), &bits,
        sizeof(bits));
    *view->mutation_generation += UINT64_C(1);
    return RUDA_MANAGED_OK;
}

#endif
