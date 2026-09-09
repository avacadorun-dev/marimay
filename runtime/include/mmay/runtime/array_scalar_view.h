#ifndef MMAY_RUNTIME_ARRAY_SCALAR_VIEW_H
#define MMAY_RUNTIME_ARRAY_SCALAR_VIEW_H

#include "mmay/runtime/array_view.h"

/* The exact rooted view has the same lifetime as the D063 RAW64 view.
 * Typed temporaries preserve host byte order and avoid unaligned accesses. */
static inline size_t mmay_managed_array_scalar_view_width(
    const MarimayManagedArrayView *view
) {
    if (view == NULL || view->descriptor == NULL || view->storage == NULL
        || view->descriptor->cell_kind != MMAY_MANAGED_CELL_RAW64) return 0;
    size_t width = 0;
    switch (view->descriptor->storage_kind) {
        case MMAY_MANAGED_ARRAY_STORAGE_RAW8: width = sizeof(uint8_t); break;
        case MMAY_MANAGED_ARRAY_STORAGE_RAW16: width = sizeof(uint16_t); break;
        case MMAY_MANAGED_ARRAY_STORAGE_RAW32: width = sizeof(uint32_t); break;
        case MMAY_MANAGED_ARRAY_STORAGE_RAW64: width = sizeof(uint64_t); break;
        default: return 0;
    }
    return width == view->descriptor->storage_stride ? width : 0;
}

static inline MarimayManagedStatus mmay_managed_array_scalar_view_get_fast(
    const MarimayManagedArrayView *view, int64_t index,
    MarimayManagedArrayAccessMode mode, uint64_t *bits, size_t *current_length
) {
    size_t width = mmay_managed_array_scalar_view_width(view);
    size_t resolved = 0;
    if (bits != NULL) *bits = 0;
    if (current_length != NULL) *current_length = 0;
    if (bits == NULL || width == 0) return MMAY_MANAGED_INVALID_ARGUMENT;
    MarimayManagedStatus status = mmay_managed_array_view_index_fast(
        view, index, mode, &resolved, current_length);
    if (status != MMAY_MANAGED_OK) return status;
    const unsigned char *source = *view->storage + resolved * width;
    switch (view->descriptor->storage_kind) {
        case MMAY_MANAGED_ARRAY_STORAGE_RAW8: {
            uint8_t value = 0;
            memcpy(&value, source, sizeof(value));
            *bits = value;
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW16: {
            uint16_t value = 0;
            memcpy(&value, source, sizeof(value));
            *bits = value;
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW32: {
            uint32_t value = 0;
            memcpy(&value, source, sizeof(value));
            *bits = value;
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW64:
            memcpy(bits, source, sizeof(*bits));
            break;
        default: return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    return MMAY_MANAGED_OK;
}

static inline MarimayManagedStatus mmay_managed_array_scalar_view_set_fast(
    const MarimayManagedArrayView *view, int64_t index,
    MarimayManagedArrayAccessMode mode, uint64_t bits, size_t *current_length
) {
    size_t width = mmay_managed_array_scalar_view_width(view);
    size_t resolved = 0;
    if (current_length != NULL) *current_length = 0;
    if (width == 0 || view->mutation_generation == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    MarimayManagedStatus status = mmay_managed_array_view_index_fast(
        view, index, mode, &resolved, current_length);
    if (status != MMAY_MANAGED_OK) return status;
    if ((width == sizeof(uint8_t) && bits > UINT8_MAX)
        || (width == sizeof(uint16_t) && bits > UINT16_MAX)
        || (width == sizeof(uint32_t) && bits > UINT32_MAX)) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    if (*view->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    unsigned char *target = *view->storage + resolved * width;
    switch (view->descriptor->storage_kind) {
        case MMAY_MANAGED_ARRAY_STORAGE_RAW8: {
            uint8_t value = (uint8_t)bits;
            memcpy(target, &value, sizeof(value));
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW16: {
            uint16_t value = (uint16_t)bits;
            memcpy(target, &value, sizeof(value));
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW32: {
            uint32_t value = (uint32_t)bits;
            memcpy(target, &value, sizeof(value));
            break;
        }
        case MMAY_MANAGED_ARRAY_STORAGE_RAW64:
            memcpy(target, &bits, sizeof(bits));
            break;
        default: return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    *view->mutation_generation += UINT64_C(1);
    return MMAY_MANAGED_OK;
}

#endif
