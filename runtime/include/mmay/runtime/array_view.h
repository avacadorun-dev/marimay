#ifndef MMAY_RUNTIME_ARRAY_VIEW_H
#define MMAY_RUNTIME_ARRAY_VIEW_H

#include <limits.h>
#include <string.h>

#include "mmay/runtime/array.h"

typedef enum MarimayManagedArrayAccessMode {
    MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
    MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS
} MarimayManagedArrayAccessMode;

/*
 * Compiler-owned non-escaping view of one rooted, non-moving array object.
 * Growth may replace element storage but does not move the array object, so
 * these field locations remain valid while `reference` is precisely rooted or
 * the generated activation is proven unable to reach a managed safepoint.
 */
typedef struct MarimayManagedArrayView {
    MarimayManagedReference reference;
    const MarimayManagedArrayDescriptor *descriptor;
    void *object;
    size_t *length;
    size_t *capacity;
    uint64_t *mutation_generation;
    unsigned char **storage;
} MarimayManagedArrayView;

MarimayManagedStatus mmay_managed_array_view_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor *expected,
    MarimayManagedArrayView *view
);

static inline MarimayManagedStatus mmay_managed_array_view_index_fast(
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    size_t *resolved_index,
    size_t *current_length
) {
    size_t length = 0;
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (view == NULL || view->object == NULL || view->length == NULL
        || resolved_index == NULL || current_length == NULL
        || (mode != MMAY_MANAGED_ARRAY_ACCESS_CHECKED
            && mode != MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    length = *view->length;
    *current_length = length;
    if (mode == MMAY_MANAGED_ARRAY_ACCESS_CHECKED
        && (index < 0 || (uint64_t)index >= (uint64_t)length)) {
        return MMAY_MANAGED_BOUNDS;
    }
    *resolved_index = (size_t)index;
    return MMAY_MANAGED_OK;
}

static inline MarimayManagedStatus mmay_managed_array_view_length_fast(
    const MarimayManagedArrayView *view,
    size_t *length
) {
    if (length != NULL) {
        *length = 0;
    }
    if (view == NULL || view->object == NULL || view->length == NULL
        || length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *length = *view->length;
    return MMAY_MANAGED_OK;
}

static inline MarimayManagedStatus mmay_managed_array_view_require_fast(
    const MarimayManagedArrayView *view,
    int64_t index,
    size_t *current_length
) {
    size_t resolved_index = 0;
    return mmay_managed_array_view_index_fast(
        view, index, MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        &resolved_index, current_length
    );
}

static inline MarimayManagedStatus mmay_managed_array_view_get_raw64_fast(
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    uint64_t *bits,
    size_t *current_length
) {
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (bits != NULL) {
        *bits = 0;
    }
    if (view == NULL || bits == NULL || view->storage == NULL
        || view->descriptor == NULL
        || view->descriptor->storage_kind != MMAY_MANAGED_ARRAY_STORAGE_RAW64
        || view->descriptor->storage_stride != sizeof(*bits)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_view_index_fast(
        view, index, mode, &resolved_index, current_length
    );
    if (status == MMAY_MANAGED_OK) {
        memcpy(bits, *view->storage + resolved_index * sizeof(*bits),
            sizeof(*bits));
    }
    return status;
}

static inline MarimayManagedStatus mmay_managed_array_view_set_raw64_fast(
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    uint64_t bits,
    size_t *current_length
) {
    MarimayManagedStatus status;
    size_t resolved_index = 0;
    if (view == NULL || view->storage == NULL
        || view->mutation_generation == NULL || view->descriptor == NULL
        || view->descriptor->storage_kind != MMAY_MANAGED_ARRAY_STORAGE_RAW64
        || view->descriptor->storage_stride != sizeof(bits)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_view_index_fast(
        view, index, mode, &resolved_index, current_length
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    if (*view->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    memcpy(*view->storage + resolved_index * sizeof(bits), &bits,
        sizeof(bits));
    *view->mutation_generation += UINT64_C(1);
    return MMAY_MANAGED_OK;
}

#endif
